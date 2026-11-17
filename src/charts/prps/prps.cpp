#include "prographics/charts/prps/prps.h"
#include "core/renderer/prps_renderer_p.h"
#include "charts/cycle_phase_p.h"
#include "charts/offline_view_p.h"
#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <algorithm>
#include <cmath>
#include <random>
#include "prographics/utils/utils.h"
#include <limits>

namespace ProGraphics {
static_assert(PRPSConstants::MAX_LINE_GROUPS == PRPSRenderer::SlotCount);

// ==================== UpdateThread 实现 ====================

UpdateThread::UpdateThread(QObject* parent) : QThread(parent) {}

UpdateThread::~UpdateThread() {
    stop();
}

void UpdateThread::stop() {
    QMutexLocker locker(&m_mutex);
    m_abort = true;
    m_condition.wakeAll();
    locker.unlock();
    wait();
}

void UpdateThread::setPaused(bool paused) {
    QMutexLocker locker(&m_mutex);
    m_paused = paused;
    if (!paused) {
        m_condition.wakeAll();
    }
}

void UpdateThread::setUpdateInterval(int intervalMs) {
    if (intervalMs <= 0) {
        qWarning() << "动画更新间隔必须大于 0:" << intervalMs;
        return;
    }
    QMutexLocker locker(&m_mutex);
    m_updateInterval = intervalMs;
}

void UpdateThread::run() {
    while (true) {
        int intervalMs;
        {
            QMutexLocker locker(&m_mutex);
            if (m_abort) {
                return;
            }
            if (m_paused) {
                m_condition.wait(&m_mutex);
                continue;
            }
            intervalMs = m_updateInterval;
        }
        emit updateAnimation();
        msleep(static_cast<unsigned long>(intervalMs));
    }
}

// ==================== PRPSChart 实现 ====================

PRPSChart::PRPSChart(QWidget* parent) : Coordinate3D(parent) {
    setSize(PRPSConstants::GL_AXIS_LENGTH);

    setAxisName('x', "相位", "°");
    setAxisName('y', "幅值", "dBm");
    setAxisEnabled(false);

    m_rangeMode = RangeMode::Fixed;
    m_fixedMin = m_configuredMin = -75.0f;
    m_fixedMax = m_configuredMax = -30.0f;

    setTicksRange('x', PRPSConstants::PHASE_MIN, PRPSConstants::PHASE_MAX, 90);
    updateAxisTicks(m_fixedMin, m_fixedMax);
    setAxisVisible('z', false);

    m_animationTimer.setInterval(20);
    m_animationTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_animationTimer, &QTimer::timeout, this, &PRPSChart::updatePRPSAnimation);
    m_recycledGroups.reserve(PRPSConstants::MAX_LINE_GROUPS);
    m_staticEmptyLabel = new QLabel(QStringLiteral("暂无事件数据"), this);
    m_staticEmptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_staticEmptyLabel->setStyleSheet(QStringLiteral("QLabel { color: white; background: #263448; padding: 8px; }"));
    m_staticEmptyLabel->hide();
}

PRPSChart::~PRPSChart() {
    m_animationTimer.stop();
    makeCurrent();
    m_pulseRenderer.reset();
    m_phaseAmplitudePlaneDrawers.clear();
    m_lineGroups.clear();
    m_recycledGroups.clear();
    doneCurrent();
}

void PRPSChart::resetData() {
    if (m_dataMode == ChartDataMode::StaticSnapshot) { clearStaticEvents(); return; }
    m_animationTimer.stop();
    m_animationClock.invalidate();
    makeCurrent();
    m_pulseRenderer.reset();
    m_lineGroups.clear();
    m_recycledGroups.clear();
    doneCurrent();
    m_nextRenderSlot = 0;
    m_rangeWindow.clear();
    m_lineTransformsDirty = false;
    m_dynamicRange.reset();
    applyCurrentRange(true);
    update();
}

void PRPSChart::setUpdateInterval(int intervalMs) {
    if (intervalMs <= 0) {
        qWarning() << "PRPS 动画更新间隔必须大于 0:" << intervalMs;
        return;
    }
    m_animationTimer.setInterval(intervalMs);
}

void PRPSChart::setPlaybackRate(double rate) {
    if (!std::isfinite(rate) || rate <= 0.0) {
        qWarning() << "PRPS 滚动动画倍率必须为正有限数:" << rate;
        return;
    }
    if (rate == m_playbackRate) return;

    // 先按旧倍率推进到切换时刻；暂停或隐藏时不推进，也不启动动画。
    if (m_animationTimer.isActive()) {
        updatePRPSAnimation();
    }
    m_playbackRate = rate;
}

void PRPSChart::showEvent(QShowEvent* event) {
    Coordinate3D::showEvent(event);
    m_animationVisible = true;
    updateAnimationState();
}

void PRPSChart::hideEvent(QHideEvent* event) {
    m_animationVisible = false;
    m_animationTimer.stop();
    m_animationClock.invalidate();
    Coordinate3D::hideEvent(event);
}

void PRPSChart::updateAnimationState() {
    const bool shouldRun = m_dataMode == ChartDataMode::Realtime && m_animationVisible && isVisible() && !m_paused && !m_lineGroups.empty();
    if (shouldRun && !m_animationTimer.isActive()) {
        // 暂停或隐藏期间不累计动画时间，恢复时从当前画面继续。
        m_animationClock.start();
        m_animationTimer.start();
    } else if (!shouldRun) {
        m_animationTimer.stop();
        m_animationClock.invalidate();
    }
}

void PRPSChart::initializeGLObjects() {
    Coordinate3D::initializeGLObjects();
    m_pulseRenderer.reset();
    m_staticEventsDirty = true;
    m_staticEventsFailed = false;
    for (auto& group : m_lineGroups) group->instanceBufferDirty = true;
    syncPhaseAmplitudePlaneDrawers();
}

void PRPSChart::paintGLObjects() {
    if (m_dataMode == ChartDataMode::StaticSnapshot) {
        Coordinate3D::paintGLObjects();
        paintStaticEvents();
        if (renderOptions().referenceLines && hasVisiblePhaseAmplitudePlaneLines()) paintPhaseAmplitudePlaneLines();
        return;
    }
    if (m_staticEventsDirty) {
        if (m_pulseRenderer) m_pulseRenderer->clearEvents();
        m_staticEventsDirty = false;
    }
    if (m_lineTransformsDirty) {
        auto timing = measureRenderStage(RenderStage::Data);
        rebuildLineGroups();
    }
    Coordinate3D::paintGLObjects();

    const bool drawPulses = renderOptions().data && !m_lineGroups.empty();
    const bool drawPlaneLines = renderOptions().referenceLines && hasVisiblePhaseAmplitudePlaneLines();

    if (!drawPulses && !drawPlaneLines) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.0f);

    if (drawPulses) {
        auto timing = measureRenderStage(RenderStage::Data);
        const int maximumCapacity = (m_displayLineCount > 0 && m_displayLineCount < m_phasePoints)
            ? m_displayLineCount : m_phasePoints;
        int requiredCapacity = 0;
        for (const auto& group : m_lineGroups) {
            requiredCapacity = std::max(requiredCapacity, static_cast<int>(group->transforms.size()));
        }
        if (requiredCapacity > 0) {
            if (!m_pulseRenderer) m_pulseRenderer = std::make_unique<PRPSRenderer>();
            const int previousCapacity = m_pulseRenderer->rowCapacity();
            int capacity = previousCapacity;
            if (capacity < requiredCapacity || capacity > maximumCapacity) {
                // 按实际可见竖线数扩容，避免稀疏周期也提交完整相位数的填充实例。
                capacity = std::min(maximumCapacity,
                    std::max(requiredCapacity, std::max(1, previousCapacity) * 2));
            }
            const bool capacityChanged = previousCapacity != capacity;
            if (m_pulseRenderer->setRowCapacity(capacity)) {
                std::array<float, PRPSRenderer::SlotCount> zPositions{};
                for (const auto& group : m_lineGroups) {
                    zPositions[group->renderSlot] = group->zPosition;
                    if (capacityChanged || group->instanceBufferDirty) {
                        group->instanceBufferDirty = !m_pulseRenderer->uploadRow(group->renderSlot, group->transforms);
                    }
                }
                m_pulseRenderer->draw(camera().getProjectionMatrix(), camera().getViewMatrix(), zPositions,
                                      m_lineGroups.front()->renderSlot, static_cast<int>(m_lineGroups.size()));
            }
        }
    }

    if (drawPlaneLines) {
        auto timing = measureRenderStage(RenderStage::ReferenceLines);
        paintPhaseAmplitudePlaneLines();
    }

    glLineWidth(1.0f);
    glDisable(GL_BLEND);
}

bool PRPSChart::hasVisiblePhaseAmplitudePlaneLines() const {
    if (m_phaseAmplitudePlaneSpecs.size() != m_phaseAmplitudePlaneDrawers.size()) {
        return false;
    }
    for (const auto& spec : m_phaseAmplitudePlaneSpecs) {
        if (spec.visible) {
            return true;
        }
    }
    return false;
}

void PRPSChart::setPhaseAmplitudePlaneLines(const std::vector<PhaseAmplitudePlaneLine>& lines) {
    m_phaseAmplitudePlaneSpecs = lines;
    if (isValid()) {
        makeCurrent();
        syncPhaseAmplitudePlaneDrawers();
        doneCurrent();
    }
    update();
}

void PRPSChart::clearPhaseAmplitudePlaneLines() {
    m_phaseAmplitudePlaneSpecs.clear();
    if (isValid()) {
        makeCurrent();
        syncPhaseAmplitudePlaneDrawers();
        doneCurrent();
    }
    update();
}

void PRPSChart::syncPhaseAmplitudePlaneDrawers() {
    m_phaseAmplitudePlaneDrawers.clear();

    if (m_phaseAmplitudePlaneSpecs.empty()) {
        return;
    }

    for (const auto& spec : m_phaseAmplitudePlaneSpecs) {
        auto ln = std::make_unique<Line2D>(
            QVector3D(0.0f, 0.0f, 0.0f),
            QVector3D(PRPSConstants::GL_AXIS_LENGTH, 0.0f, 0.0f),
            spec.color);

        Primitive2DStyle style;
        style.lineWidth = spec.lineWidth;
        ln->setStyle(style);
        ln->initialize();
        m_phaseAmplitudePlaneDrawers.push_back(std::move(ln));
    }
}

void PRPSChart::paintPhaseAmplitudePlaneLines() {
    if (m_phaseAmplitudePlaneSpecs.size() != m_phaseAmplitudePlaneDrawers.size()) {
        return;
    }

    const QMatrix4x4 projection = camera().getProjectionMatrix();
    const float x0 = m_dataMode == ChartDataMode::StaticSnapshot ? 0.0f : mapPhaseToGL(m_phaseMin);
    const float x1 = m_dataMode == ChartDataMode::StaticSnapshot ? PRPSConstants::GL_AXIS_LENGTH : mapPhaseToGL(m_phaseMax);
    const auto [displayMin, displayMax] = getCurrentRange();

    for (size_t i = 0; i < m_phaseAmplitudePlaneSpecs.size(); ++i) {
        const PhaseAmplitudePlaneLine& spec = m_phaseAmplitudePlaneSpecs[i];
        if (!spec.visible || !std::isfinite(spec.amplitudeDbm) ||
            spec.amplitudeDbm < displayMin || spec.amplitudeDbm > displayMax) {
            continue;
        }

        const float y = mapAmplitudeToGL(spec.amplitudeDbm);

        Line2D* ln = m_phaseAmplitudePlaneDrawers[i].get();
        ln->setPoints(QVector3D(x0, y, 0.0f), QVector3D(x1, y, 0.0f));
        ln->setColor(spec.color);
        Primitive2DStyle style;
        style.lineWidth = spec.lineWidth;
        ln->setStyle(style);
        ln->draw(projection, camera().getViewMatrix());
    }
}

void PRPSChart::addCycleData(const std::vector<float>& cycleData) {
    if (m_dataMode == ChartDataMode::StaticSnapshot) return;
    if (!m_acceptData) {
        return;
    }
    
    if (cycleData.size() != m_phasePoints) {
        qWarning() << "Invalid cycle data size:" << cycleData.size() << "expected:" << m_phasePoints;
        return;
    }

    m_rangeWindow.append(cycleData);
    updateRangeFromWindow();
    processCycle(cycleData);
    updateAnimationState();
    if (m_paused) {
        // 允许暂停时接收数据的情况下，也需要显示新增周期。
        update();
    }
}

void PRPSChart::setDisplayLineCount(int count) {
    if (rejectStaticConfiguration("setDisplayLineCount")) return;
    m_displayLineCount = count;
    recalculateLineGroups();
}

void PRPSChart::buildLineTransformsFromCycle(const std::vector<float>& cycleData,
                                             std::vector<Transform2D>& out) const {
    out.clear();
    const int N = static_cast<int>(cycleData.size());
    if (N == 0 || N != m_phasePoints) {
        return;
    }

    auto pushLine = [&](size_t sampleIndex, float amplitudeSample) {
        const double phase = CyclePhase::samplePhase(sampleIndex, cycleData.size(), m_phaseMin, m_phaseMax);
        const float glX = mapPhaseToGL(phase);
        const float glY = mapAmplitudeToGL(amplitudeSample);
        if (glY <= 0.0f) {
            return;
        }
        Transform2D transform;
        transform.position = QVector2D(glX, 0.0f);
        transform.scale    = QVector2D(1.0f, glY);
        const float intensity = glY / PRPSConstants::GL_AXIS_LENGTH;
        transform.color       = calculateColor(intensity);
        out.push_back(transform);
    };

    CyclePhase::forEachPeak(cycleData, m_displayLineCount, pushLine);
}

void PRPSChart::processCycle(const std::vector<float>& cycleData) {
    std::unique_ptr<LineGroup> newGroup;
    if (m_lineGroups.size() >= PRPSConstants::MAX_LINE_GROUPS) {
        newGroup = std::move(m_lineGroups.front());
        m_lineGroups.pop_front();
    } else if (!m_recycledGroups.empty()) {
        newGroup = std::move(m_recycledGroups.back());
        m_recycledGroups.pop_back();
    } else {
        newGroup = std::make_unique<LineGroup>();
    }

    newGroup->zPosition = PRPSConstants::MAX_Z_POSITION;
    newGroup->isActive = true;
    newGroup->instanceBufferDirty = true;
    newGroup->renderSlot = m_nextRenderSlot;
    m_nextRenderSlot = (m_nextRenderSlot + 1) % PRPSRenderer::SlotCount;
    newGroup->amplitudes = cycleData;

    const int cap = (m_displayLineCount > 0 && m_displayLineCount < m_phasePoints) ? m_displayLineCount : m_phasePoints;
    newGroup->transforms.reserve(static_cast<size_t>(cap));

    if (m_lineTransformsDirty) newGroup->transforms.clear();
    else buildLineTransformsFromCycle(cycleData, newGroup->transforms);

    m_lineGroups.push_back(std::move(newGroup));
}

void PRPSChart::updatePRPSAnimation() {
    if (m_dataMode == ChartDataMode::StaticSnapshot) { updateAnimationState(); return; }
    if (m_paused || !m_animationVisible || !isVisible() || m_lineGroups.empty()) {
        updateAnimationState();
        return;
    }

    const double elapsedSeconds = static_cast<double>(m_animationClock.nsecsElapsed()) / 1000000000.0;
    // 达到整个滚动跨度的位移已足以让所有周期退出，限制转换范围以兼容极大的有限倍率。
    const float displacement = static_cast<float>(std::min(
        static_cast<double>(PRPSConstants::MAX_Z_POSITION - PRPSConstants::MIN_Z_POSITION),
        m_prpsScrollSpeed * elapsedSeconds * m_playbackRate));
    m_animationClock.start();
    bool needCleanup = false;

    for (auto& group : m_lineGroups) {
        group->zPosition -= displacement;

        if (group->zPosition <= PRPSConstants::MIN_Z_POSITION) {
            group->isActive = false;
            needCleanup     = true;
        }
    }

    if (needCleanup) {
        cleanupInactiveGroups();
    }
    // 即使停止送入，历史周期退出后也按同一时间滞回规则更新量程。
    updateRangeFromWindow();

    updateAnimationState();
    update();
}

void PRPSChart::cleanupInactiveGroups() {
    for (auto it = m_lineGroups.begin(); it != m_lineGroups.end();) {
        if (!(*it)->isActive) {
            m_rangeWindow.removeOldest();
            // 保留 CPU 数据容量供下一个周期复用，GPU 槽由环形批量渲染器管理。
            m_recycledGroups.push_back(std::move(*it));
            it = m_lineGroups.erase(it);
        } else {
            ++it;
        }
    }
}

// ==================== 量程设置 API 实现 ====================

void PRPSChart::setFixedRange(float min, float max) {
    if (rejectStaticConfiguration("setFixedRange")) return;
    LinearScale::Range range;
    if (!LinearScale::normalize(min, max, range, 0.0)) {
        qWarning() << "PRPS 量程端点必须为有限数值";
        return;
    }
    m_rangeMode = RangeMode::Fixed;
    m_fixedMin = m_configuredMin = range.first;
    m_fixedMax = m_configuredMax = range.second;
    applyCurrentRange(true);
}

void PRPSChart::setAutoRange(const DynamicRange::DynamicRangeConfig& config) {
    if (rejectStaticConfiguration("setAutoRange")) return;
    const auto seed = getCurrentRange();
    m_rangeMode = RangeMode::Auto;
    m_dynamicRange.setConfig(config);
    m_dynamicRange.setInitialRange(seed.first, seed.second);
    m_dynamicRange.setUseInitialRange(false);
    m_configuredMin = seed.first;
    m_configuredMax = seed.second;
    updateRangeFromWindow();
    applyCurrentRange(true);
}

void PRPSChart::setAdaptiveRange(float initialMin, float initialMax, const DynamicRange::DynamicRangeConfig& config) {
    if (rejectStaticConfiguration("setAdaptiveRange")) return;
    LinearScale::Range range;
    if (!LinearScale::normalize(initialMin, initialMax, range, 0.0)) {
        qWarning() << "PRPS 初始量程端点必须为有限数值";
        return;
    }
    m_rangeMode     = RangeMode::Adaptive;
    m_configuredMin = range.first;
    m_configuredMax = range.second;
    m_dynamicRange.setConfig(config);
    m_dynamicRange.setInitialRange(range.first, range.second);
    m_dynamicRange.setUseInitialRange(true);
    updateRangeFromWindow();
    applyCurrentRange(true);
}

// ==================== 量程查询 API 实现 ====================

std::pair<float, float> PRPSChart::getCurrentRange() const {
    if (m_dataMode == ChartDataMode::StaticSnapshot) return {static_cast<float>(m_staticAmplitudeAxis.minimum), static_cast<float>(m_staticAmplitudeAxis.maximum)};
    return {m_displayMin, m_displayMax};
}

std::pair<float, float> PRPSChart::getConfiguredRange() const {
    if (m_dataMode == ChartDataMode::StaticSnapshot) return getCurrentRange();
    return {m_configuredMin, m_configuredMax};
}

// ==================== 运行时调整 API 实现 ====================

void PRPSChart::updateAutoRangeConfig(const DynamicRange::DynamicRangeConfig& config) {
    if (rejectStaticConfiguration("updateAutoRangeConfig")) return;
    if (m_rangeMode == RangeMode::Auto || m_rangeMode == RangeMode::Adaptive) {
        m_dynamicRange.setConfig(config);
        forceUpdateRange();
    }
}

void PRPSChart::switchToFixedRange(float min, float max) {
    setFixedRange(min, max);
}

void PRPSChart::switchToAutoRange() {
    setAutoRange(m_dynamicRange.getConfig());
}

// ==================== 硬限制 API 实现 ====================

void PRPSChart::setHardLimits(float min, float max, bool enabled) {
    if (rejectStaticConfiguration("setHardLimits")) return;
    m_dynamicRange.setHardLimits(min, max, enabled);
    if (m_rangeMode != RangeMode::Fixed) {
        forceUpdateRange();
    }
}

std::pair<float, float> PRPSChart::getHardLimits() const {
    return m_dynamicRange.getHardLimits();
}

void PRPSChart::enableHardLimits(bool enabled) {
    if (rejectStaticConfiguration("enableHardLimits")) return;
    m_dynamicRange.enableHardLimits(enabled);
    if (m_rangeMode != RangeMode::Fixed) {
        forceUpdateRange();
    }
}

bool PRPSChart::isHardLimitsEnabled() const {
    return m_dynamicRange.isHardLimitsEnabled();
}

// ==================== 私有方法实现 ====================

void PRPSChart::forceUpdateRange() {
    updateRangeFromWindow();
    applyCurrentRange(true);
}

void PRPSChart::updateRangeFromWindow() {
    LinearScale::Range bounds;
    if (m_rangeMode != RangeMode::Fixed && m_rangeWindow.bounds(bounds)) {
        m_dynamicRange.updateBounds(bounds.first, bounds.second);
        applyCurrentRange();
    }
}

void PRPSChart::applyCurrentRange(bool forceTicks) {
    const auto range = m_rangeMode == RangeMode::Fixed
        ? LinearScale::Range{m_fixedMin, m_fixedMax} : m_dynamicRange.getDisplayRange();
    const bool changed = range != getCurrentRange();
    if (!changed && !forceTicks) return;
    m_displayMin = range.first;
    m_displayMax = range.second;
    updateAxisTicks(range.first, range.second);
    if (changed) recalculateLineGroups();
    update();
    if (changed) emit rangeChanged(range.first, range.second);
}

void PRPSChart::updateAxisTicks(float min, float max) {
    int targetTicks = m_dynamicRange.getConfig().targetTickCount;
    const double requestedStep = m_rangeMode == RangeMode::Fixed
        ? LinearScale::ceilingTickStep(static_cast<double>(max) - min, targetTicks)
        : LinearScale::tickStep(static_cast<double>(max) - min, targetTicks);
    const float step = static_cast<float>(std::clamp(requestedStep,
        static_cast<double>(std::numeric_limits<float>::denorm_min()),
        static_cast<double>(std::numeric_limits<float>::max())));
    setTicksRange('y', min, max, step);
}

void PRPSChart::setPhaseRange(float min, float max) {
    if (rejectStaticConfiguration("setPhaseRange")) return;
    if (!CyclePhase::validRange(min, max)) {
        qWarning() << "PRPS 相位范围必须为有限数值且下限小于上限";
        return;
    }
    if (min == m_phaseMin && max == m_phaseMax) return;
    m_phaseMin = min;
    m_phaseMax = max;
    setTicksRange('x', min, max, CyclePhase::tickStep(min, max));
    recalculateLineGroups();
}

void PRPSChart::setPhasePoint(int phasePoint) {
    if (rejectStaticConfiguration("setPhasePoint")) return;
    if (phasePoint <= 0) {
        qWarning() << "PRPS 相位采样点数必须大于 0:" << phasePoint;
        return;
    }
    if (phasePoint == m_phasePoints) {
        return;
    }
    m_phasePoints = phasePoint;
    resetData();
}

float PRPSChart::mapPhaseToGL(double phase) const {
    if (m_dataMode == ChartDataMode::StaticSnapshot) return static_cast<float>((phase - m_staticPhaseAxis.minimum) /
        (m_staticPhaseAxis.maximum - m_staticPhaseAxis.minimum) * PRPSConstants::GL_AXIS_LENGTH);
    return CyclePhase::toPosition(phase, m_phaseMin, m_phaseMax, PRPSConstants::GL_AXIS_LENGTH);
}

float PRPSChart::mapAmplitudeToGL(float amplitude) const {
    if (m_dataMode == ChartDataMode::StaticSnapshot) return static_cast<float>(std::clamp(
        (amplitude - m_staticAmplitudeAxis.minimum) / (m_staticAmplitudeAxis.maximum - m_staticAmplitudeAxis.minimum), 0.0, 1.0) * PRPSConstants::GL_AXIS_LENGTH);
    const auto [displayMin, displayMax] = getCurrentRange();

    if (amplitude < displayMin) {
        return 0.0f;
    }
    if (amplitude > displayMax) {
        return PRPSConstants::GL_AXIS_LENGTH;
    }

    return static_cast<float>((static_cast<double>(amplitude) - displayMin) /
        (static_cast<double>(displayMax) - displayMin) * PRPSConstants::GL_AXIS_LENGTH);
}

float PRPSChart::mapGLToPhase(float glX) const {
    return CyclePhase::fromPosition(glX, m_phaseMin, m_phaseMax, PRPSConstants::GL_AXIS_LENGTH);
}

float PRPSChart::mapGLToAmplitude(float glY) const {
    const auto [displayMin, displayMax] = getCurrentRange();
    return static_cast<float>(glY / PRPSConstants::GL_AXIS_LENGTH *
        (static_cast<double>(displayMax) - displayMin) + displayMin);
}

void PRPSChart::recalculateLineGroups() {
    // 一帧前可能送入多个周期，历史映射只按最终量程重建一次。
    m_lineTransformsDirty = true;
    update();
}

void PRPSChart::rebuildLineGroups() {
    const int cap = (m_displayLineCount > 0 && m_displayLineCount < m_phasePoints) ? m_displayLineCount : m_phasePoints;

    for (auto& group : m_lineGroups) {
        group->transforms.clear();
        group->transforms.reserve(static_cast<size_t>(cap));
        buildLineTransformsFromCycle(group->amplitudes, group->transforms);
        group->instanceBufferDirty = true;
    }

    m_lineTransformsDirty = false;
}

// ==================== 暂停/恢复 API 实现 ====================

void PRPSChart::pause(bool blockNewData) {
    m_paused = true;
    m_acceptData = !blockNewData;
    updateAnimationState();
}

void PRPSChart::resume() {
    m_paused = false;
    m_acceptData = true;
    updateAnimationState();
}

void PRPSChart::togglePause() {
    if (m_paused) {
        resume();
    } else {
        pause();
    }
}

bool PRPSChart::rejectStaticConfiguration(const char* method) const {
    if (m_dataMode != ChartDataMode::StaticSnapshot) return false;
    qWarning() << method << "静态坐标由快照定义，请准备并提交新快照或先切回实时模式";
    return true;
}

bool PRPSChart::setStaticEvents(PRPSEventSnapshot::Ptr snapshot) {
    if (QThread::currentThread() != thread() || !snapshot) {
        qWarning() << "静态 PRPS 需在所属线程提交有效快照";
        return false;
    }
    if (m_dataMode == ChartDataMode::Realtime) {
        if (m_animationTimer.isActive()) updatePRPSAnimation();
        m_realtimeCoordinateConfig = config();
    }
    const auto previousRange = getCurrentRange();
    m_staticEvents = std::move(snapshot);
    m_staticPickCandidate = false;
    m_dataMode = ChartDataMode::StaticSnapshot;
    updateAnimationState();
    const auto& d = m_staticEvents->data();
    m_staticPhaseAxis = d.phase;
    m_staticAmplitudeAxis = d.amplitude;
    auto axes = config();
    axes.size = PRPSConstants::GL_AXIS_LENGTH;
    axes.names.enabled = axes.ticks.enabled = true;
    axes.axis.enabled = axes.axis.z.visible = true;
    OfflineView::configureAxis(axes.ticks.x, axes.names.x, d.phase);
    OfflineView::configureAxis(axes.ticks.y, axes.names.y, d.amplitude);
    OfflineView::configureAxis(axes.ticks.z, axes.names.z, d.sequence);
    setConfig(axes);
    m_staticEventsDirty = true;
    m_staticEventsFailed = false;
    m_staticEmptyLabel->setText(QStringLiteral("暂无事件数据"));
    m_staticEmptyLabel->setVisible(m_staticEvents->empty());
    layoutStaticOverlay();
    update();
    if (previousRange != getCurrentRange()) emit rangeChanged(getCurrentRange().first, getCurrentRange().second);
    return true;
}

void PRPSChart::clearStaticEvents() {
    if (QThread::currentThread() != thread() || m_dataMode != ChartDataMode::StaticSnapshot) return;
    m_staticPickCandidate = false;
    m_staticEvents.reset();
    m_staticEventsDirty = true;
    m_staticEventsFailed = false;
    m_staticEmptyLabel->setText(QStringLiteral("暂无事件数据"));
    m_staticEmptyLabel->show();
    layoutStaticOverlay();
    update();
}

void PRPSChart::useRealtimeData() {
    if (QThread::currentThread() != thread() || m_dataMode == ChartDataMode::Realtime) return;
    m_staticPickCandidate = false;
    const auto previousRange = getCurrentRange();
    m_dataMode = ChartDataMode::Realtime;
    m_staticEvents.reset();
    m_staticEventsDirty = true;
    m_staticEmptyLabel->hide();
    if (m_realtimeCoordinateConfig) setConfig(*m_realtimeCoordinateConfig);
    m_realtimeCoordinateConfig.reset();
    updateAnimationState();
    update();
    if (previousRange != getCurrentRange()) emit rangeChanged(m_displayMin, m_displayMax);
}

void PRPSChart::paintStaticEvents() {
    if (!m_staticEvents || m_staticEvents->empty()) {
        if (m_pulseRenderer) m_pulseRenderer->clearEvents();
        return;
    }
    if (!renderOptions().data || m_staticEventsFailed) return;
    auto timing = measureRenderStage(RenderStage::Data);
    if (!m_pulseRenderer) m_pulseRenderer = std::make_unique<PRPSRenderer>();
    if (m_staticEventsDirty) {
        m_staticEventsDirty = false;
        if (!m_pulseRenderer->uploadEvents(*m_staticEvents)) {
            m_staticEventsFailed = true;
            qWarning() << "静态 PRPS 缓冲上传失败，CPU 快照仍完整保留";
            m_staticEmptyLabel->setText(QStringLiteral("事件绘制失败，完整数据仍已保留"));
            m_staticEmptyLabel->show();
            layoutStaticOverlay();
            return;
        }
    }
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.0f);
    if (!m_pulseRenderer->drawEvents(camera().getProjectionMatrix(), camera().getViewMatrix(), PRPSConstants::GL_AXIS_LENGTH)) {
        m_staticEventsFailed = true;
        qWarning() << "静态 PRPS 绘制失败，CPU 快照仍完整保留";
        m_staticEmptyLabel->setText(QStringLiteral("事件绘制失败，完整数据仍已保留"));
        m_staticEmptyLabel->show();
        layoutStaticOverlay();
    }
}

std::optional<StaticEventHit> PRPSChart::pickStaticEvent(const QPointF& pos, double tolerance) const {
    if (m_dataMode != ChartDataMode::StaticSnapshot || !m_staticEvents || m_staticEventsFailed ||
        !renderOptions().data || !std::isfinite(tolerance) || tolerance < 0.0 || !rect().contains(pos.toPoint())) return {};
    const QMatrix4x4 matrix = camera().getProjectionMatrix() * camera().getViewMatrix();
    double bestDistance = tolerance * tolerance;
    double bestDepth = std::numeric_limits<double>::infinity();
    std::optional<StaticEventHit> result;
    const auto& instances = m_staticEvents->m_instances;
    for (std::size_t i = 0; i < instances.size(); ++i) {
        const auto& instance = instances[i];
        const float x = instance.phaseHeight[0] * PRPSConstants::GL_AXIS_LENGTH;
        const float y = instance.phaseHeight[1] * PRPSConstants::GL_AXIS_LENGTH;
        const float z = instance.sequence * PRPSConstants::GL_AXIS_LENGTH;
        QVector4D a = matrix * QVector4D(x, 0.0f, z, 1.0f);
        QVector4D b = matrix * QVector4D(x, y, z, 1.0f);
        // 与 GPU 一样先裁剪线段，避免相机背后或近裁面之外的事件误命中。
        double low = 0.0, high = 1.0;
        for (int axis = 0; axis < 3 && low <= high; ++axis) {
            for (int sign : {-1, 1}) {
                const double start = a.w() + sign * a[axis];
                const double end = b.w() + sign * b[axis];
                if (start < 0.0 && end < 0.0) { low = 1.0; high = 0.0; break; }
                if (start < 0.0) low = std::max(low, start / (start - end));
                else if (end < 0.0) high = std::min(high, start / (start - end));
            }
        }
        if (low > high) continue;
        const auto delta = b - a;
        b = a + delta * static_cast<float>(high);
        a += delta * static_cast<float>(low);
        if (a.w() <= 1e-8f || b.w() <= 1e-8f) continue;
        a /= a.w(); b /= b.w();
        const QPointF start((a.x() + 1.0) * width() / 2.0, (1.0 - a.y()) * height() / 2.0);
        const QPointF end((b.x() + 1.0) * width() / 2.0, (1.0 - b.y()) * height() / 2.0);
        const auto line = end - start;
        const double length = QPointF::dotProduct(line, line);
        const double t = length > 0.0 ? std::clamp(QPointF::dotProduct(pos - start, line) / length, 0.0, 1.0) : 0.0;
        const auto difference = pos - (start + line * t);
        const double distance = QPointF::dotProduct(difference, difference);
        const double depth = a.z() + t * (b.z() - a.z());
        if (distance < bestDistance || (distance == bestDistance && depth < bestDepth)) {
            bestDistance = distance;
            bestDepth = depth;
            result = m_staticEvents->event(i);
        }
    }
    return result;
}

void PRPSChart::mousePressEvent(QMouseEvent* event) {
    m_staticPressPosition = event->pos();
    m_staticPickCandidate = m_dataMode == ChartDataMode::StaticSnapshot && event->button() == Qt::LeftButton;
    Coordinate3D::mousePressEvent(event);
}

void PRPSChart::mouseMoveEvent(QMouseEvent* event) {
    if ((QPointF(event->pos()) - m_staticPressPosition).manhattanLength() >= QApplication::startDragDistance())
        m_staticPickCandidate = false;
    Coordinate3D::mouseMoveEvent(event);
}

void PRPSChart::mouseReleaseEvent(QMouseEvent* event) {
    Coordinate3D::mouseReleaseEvent(event);
    const bool canPick = m_staticPickCandidate;
    m_staticPickCandidate = false;
    if (canPick && m_dataMode == ChartDataMode::StaticSnapshot && event->button() == Qt::LeftButton &&
        (QPointF(event->pos()) - m_staticPressPosition).manhattanLength() < QApplication::startDragDistance()) {
        const auto hit = pickStaticEvent(event->pos());
        if (hit) emit staticEventSelected(static_cast<quint64>(hit->event.sourceEventId), static_cast<quint64>(hit->index));
    }
}

void PRPSChart::layoutStaticOverlay() {
    m_staticEmptyLabel->adjustSize();
    m_staticEmptyLabel->move((width() - m_staticEmptyLabel->width()) / 2, (height() - m_staticEmptyLabel->height()) / 2);
}

void PRPSChart::resizeEvent(QResizeEvent* event) {
    Coordinate3D::resizeEvent(event);
    layoutStaticOverlay();
}
} // namespace ProGraphics
