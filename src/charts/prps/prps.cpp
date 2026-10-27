#include "prographics/charts/prps/prps.h"
#include "core/renderer/prps_renderer_p.h"
#include <QHideEvent>
#include <QShowEvent>
#include <algorithm>
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
    const bool shouldRun = m_animationVisible && isVisible() && !m_paused && !m_lineGroups.empty();
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
    for (auto& group : m_lineGroups) group->instanceBufferDirty = true;
    syncPhaseAmplitudePlaneDrawers();
}

void PRPSChart::paintGLObjects() {
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
    const float x0 = mapPhaseToGL(m_phaseMin);
    const float x1 = mapPhaseToGL(m_phaseMax);

    for (size_t i = 0; i < m_phaseAmplitudePlaneSpecs.size(); ++i) {
        const PhaseAmplitudePlaneLine& spec = m_phaseAmplitudePlaneSpecs[i];
        if (!spec.visible || !std::isfinite(spec.amplitudeDbm) ||
            spec.amplitudeDbm < m_displayMin || spec.amplitudeDbm > m_displayMax) {
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

    const float span = m_phaseMax - m_phaseMin;
    const int   B    = m_displayLineCount;

    auto pushLine = [&](float phase, float amplitudeSample) {
        if (!std::isfinite(amplitudeSample)) return;
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

    if (B <= 0 || B >= N) {
        if (N == 1) {
            const float phase = 0.5f * (m_phaseMin + m_phaseMax);
            pushLine(phase, cycleData[0]);
            return;
        }
        for (int i = 0; i < N; ++i) {
            const float phase = static_cast<float>(i) / static_cast<float>(N - 1) * m_phaseMax;
            pushLine(phase, cycleData[i]);
        }
        return;
    }

    for (int b = 0; b < B; ++b) {
        const int i0 = b * N / B;
        const int i1 = (b + 1) * N / B;
        if (i0 >= i1) {
            continue;
        }
        float peak = -std::numeric_limits<float>::infinity();
        for (int i = i0; i < i1; ++i) {
            const float sample = cycleData[static_cast<size_t>(i)];
            if (std::isfinite(sample)) peak = std::max(peak, sample);
        }
        const float phase = m_phaseMin + (static_cast<float>(b) + 0.5f) * span / static_cast<float>(B);
        pushLine(phase, peak);
    }
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
    if (m_paused || !m_animationVisible || !isVisible() || m_lineGroups.empty()) {
        updateAnimationState();
        return;
    }

    const float displacement = m_prpsScrollSpeed *
        static_cast<float>(m_animationClock.nsecsElapsed()) / 1000000000.0f;
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
    return {m_displayMin, m_displayMax};
}

std::pair<float, float> PRPSChart::getConfiguredRange() const {
    return {m_configuredMin, m_configuredMax};
}

// ==================== 运行时调整 API 实现 ====================

void PRPSChart::updateAutoRangeConfig(const DynamicRange::DynamicRangeConfig& config) {
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
    m_dynamicRange.setHardLimits(min, max, enabled);
    if (m_rangeMode != RangeMode::Fixed) {
        forceUpdateRange();
    }
}

std::pair<float, float> PRPSChart::getHardLimits() const {
    return m_dynamicRange.getHardLimits();
}

void PRPSChart::enableHardLimits(bool enabled) {
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
    m_phaseMin = min;
    m_phaseMax = max;
    setTicksRange('x', min, max, 85);
    update();
}

void PRPSChart::setPhasePoint(int phasePoint) {
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

float PRPSChart::mapPhaseToGL(float phase) const {
    return (phase - m_phaseMin) / (m_phaseMax - m_phaseMin) * PRPSConstants::GL_AXIS_LENGTH;
}

float PRPSChart::mapAmplitudeToGL(float amplitude) const {
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
    return glX / PRPSConstants::GL_AXIS_LENGTH * (m_phaseMax - m_phaseMin) + m_phaseMin;
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

} // namespace ProGraphics
