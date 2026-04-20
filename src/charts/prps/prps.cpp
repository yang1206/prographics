#include "prographics/charts/prps/prps.h"
#include <algorithm>
#include <random>
#include "prographics/utils/utils.h"

namespace ProGraphics {

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
    QMutexLocker locker(&m_mutex);
    m_updateInterval = intervalMs;
}

void UpdateThread::run() {
    while (true) {
        {
            QMutexLocker locker(&m_mutex);
            if (m_abort) {
                return;
            }
            if (m_paused) {
                m_condition.wait(&m_mutex);
                continue;
            }
        }
        emit updateAnimation();
        msleep(m_updateInterval);
    }
}

// ==================== PRPSChart 实现 ====================

PRPSChart::PRPSChart(QWidget* parent) : Coordinate3D(parent), m_updateThread(this) {
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

    connect(&m_updateThread, &UpdateThread::updateAnimation,
            this, &PRPSChart::updatePRPSAnimation, Qt::QueuedConnection);

    m_updateThread.start();
}

PRPSChart::~PRPSChart() {
    m_updateThread.stop();
    makeCurrent();
    m_lineGroups.clear();
    doneCurrent();
}

void PRPSChart::resetData() {
    makeCurrent();
    m_lineGroups.clear();
    doneCurrent();

    m_currentCycles.clear();
    m_threshold = 0.1f;

    float displayMin;
    float displayMax;
    if (m_rangeMode == RangeMode::Fixed) {
        displayMin = m_fixedMin;
        displayMax = m_fixedMax;
    } else {
        displayMin = m_configuredMin;
        displayMax = m_configuredMax;
        m_dynamicRange.reset();
        m_dynamicRange.setInitialRange(displayMin, displayMax);
    }

    updateAxisTicks(displayMin, displayMax);
    update();
}

void PRPSChart::initializeGLObjects() {
    Coordinate3D::initializeGLObjects();
}

void PRPSChart::paintGLObjects() {
    Coordinate3D::paintGLObjects();

    if (m_lineGroups.empty()) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.0f);

    for (const auto& group : m_lineGroups) {
        if (group->instancedLine && group->isActive) {
            QMatrix4x4 model;
            model.translate(0, 0, group->zPosition);

            const float alphaRep =
                (group->zPosition < 2.0f) ? (group->zPosition / 2.0f) : -1.0f;

            group->instancedLine->drawInstanced(
                camera().getProjectionMatrix(),
                camera().getViewMatrix() * model,
                group->transforms,
                alphaRep,
                group->instanceBufferDirty);
            group->instanceBufferDirty = false;
        }
    }

    glLineWidth(1.0f);
    glDisable(GL_BLEND);
}

void PRPSChart::addCycleData(const std::vector<float>& cycleData) {
    if (!m_acceptData) {
        return;
    }
    
    if (cycleData.size() != m_phasePoints) {
        qWarning() << "Invalid cycle data size:" << cycleData.size() << "expected:" << m_phasePoints;
        return;
    }

    if (m_lineGroups.size() >= PRPSConstants::MAX_LINE_GROUPS) {
        m_lineGroups.erase(m_lineGroups.begin());
    }

    bool rangeChanged = false;
    switch (m_rangeMode) {
        case RangeMode::Fixed:
            break;
        case RangeMode::Auto:
        case RangeMode::Adaptive:
            rangeChanged = m_dynamicRange.updateRange(cycleData);
            break;
    }

    if (rangeChanged) {
        auto [newDisplayMin, newDisplayMax] = m_dynamicRange.getDisplayRange();
        updateAxisTicks(newDisplayMin, newDisplayMax);
        recalculateLineGroups();
    }

    m_currentCycles.push_back(cycleData);
    processCurrentCycles();
    m_currentCycles.clear();
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
        float peak = cycleData[static_cast<size_t>(i0)];
        for (int i = i0 + 1; i < i1; ++i) {
            peak = std::max(peak, cycleData[static_cast<size_t>(i)]);
        }
        const float phase = m_phaseMin + (static_cast<float>(b) + 0.5f) * span / static_cast<float>(B);
        pushLine(phase, peak);
    }
}

void PRPSChart::processCurrentCycles() {
    makeCurrent();

    auto newGroup = std::make_unique<LineGroup>();
    const auto& cycleData = m_currentCycles.front();
    newGroup->amplitudes  = cycleData;

    newGroup->instancedLine = std::make_unique<Line2D>(
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f),
        QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    const int cap = (m_displayLineCount > 0 && m_displayLineCount < m_phasePoints) ? m_displayLineCount : m_phasePoints;
    newGroup->transforms.reserve(static_cast<size_t>(cap));

    buildLineTransformsFromCycle(cycleData, newGroup->transforms);

    newGroup->instancedLine->initialize();
    m_lineGroups.push_back(std::move(newGroup));
    doneCurrent();
}

void PRPSChart::updatePRPSAnimation() {
    bool needCleanup = false;

    for (auto& group : m_lineGroups) {
        group->zPosition -= m_prpsAnimationSpeed;

        if (group->zPosition <= PRPSConstants::MIN_Z_POSITION) {
            group->isActive = false;
            needCleanup     = true;
        }
    }

    if (needCleanup) {
        cleanupInactiveGroups();
    }

    update();
}

void PRPSChart::cleanupInactiveGroups() {
    makeCurrent();

    auto it = std::remove_if(m_lineGroups.begin(), m_lineGroups.end(),
        [](const std::unique_ptr<LineGroup>& group) {
            return !group->isActive;
        });

    if (it != m_lineGroups.end()) {
        m_lineGroups.erase(it, m_lineGroups.end());
    }

    doneCurrent();
}

// ==================== 量程设置 API 实现 ====================

void PRPSChart::setFixedRange(float min, float max) {
    m_rangeMode = RangeMode::Fixed;
    m_fixedMin = m_configuredMin = min;
    m_fixedMax = m_configuredMax = max;
    updateAxisTicks(min, max);
    recalculateLineGroups();
    update();
}

void PRPSChart::setAutoRange(const DynamicRange::DynamicRangeConfig& config) {
    m_rangeMode = RangeMode::Auto;
    m_dynamicRange.setConfig(config);

    auto [currentMin, currentMax] = m_dynamicRange.getDisplayRange();
    m_configuredMin               = currentMin;
    m_configuredMax               = currentMax;

    updateAxisTicks(currentMin, currentMax);
    recalculateLineGroups();
    update();
}

void PRPSChart::setAdaptiveRange(float initialMin, float initialMax, const DynamicRange::DynamicRangeConfig& config) {
    m_rangeMode     = RangeMode::Adaptive;
    m_configuredMin = initialMin;
    m_configuredMax = initialMax;

    m_dynamicRange.setConfig(config);
    m_dynamicRange.setInitialRange(initialMin, initialMax);

    auto [currentMin, currentMax] = m_dynamicRange.getDisplayRange();
    updateAxisTicks(currentMin, currentMax);
    recalculateLineGroups();
    update();
}

// ==================== 量程查询 API 实现 ====================

std::pair<float, float> PRPSChart::getCurrentRange() const {
    switch (m_rangeMode) {
        case RangeMode::Fixed:
            return {m_fixedMin, m_fixedMax};
        case RangeMode::Auto:
        case RangeMode::Adaptive:
            return m_dynamicRange.getDisplayRange();
    }
    return {0, 0};
}

std::pair<float, float> PRPSChart::getConfiguredRange() const {
    return {m_configuredMin, m_configuredMax};
}

// ==================== 运行时调整 API 实现 ====================

void PRPSChart::updateAutoRangeConfig(const DynamicRange::DynamicRangeConfig& config) {
    if (m_rangeMode == RangeMode::Auto || m_rangeMode == RangeMode::Adaptive) {
        m_dynamicRange.setConfig(config);
        auto [currentMin, currentMax] = m_dynamicRange.getDisplayRange();
        updateAxisTicks(currentMin, currentMax);
        recalculateLineGroups();
        update();
    }
}

void PRPSChart::switchToFixedRange(float min, float max) {
    setFixedRange(min, max);
}

void PRPSChart::switchToAutoRange() {
    setAutoRange();
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
    auto [newMin, newMax] = m_dynamicRange.getDisplayRange();
    updateAxisTicks(newMin, newMax);
    recalculateLineGroups();
    update();
}

void PRPSChart::updateAxisTicks(float min, float max) {
    int targetTicks = m_dynamicRange.getConfig().targetTickCount;
    float step = calculateNiceTickStep(max - min, targetTicks);
    setTicksRange('y', min, max, step);
}

void PRPSChart::setPhaseRange(float min, float max) {
    m_phaseMin = min;
    m_phaseMax = max;
    setTicksRange('x', min, max, 85);
    update();
}

void PRPSChart::setPhasePoint(int phasePoint) {
    m_phasePoints = phasePoint;
}

float PRPSChart::mapPhaseToGL(float phase) const {
    return (phase - m_phaseMin) / (m_phaseMax - m_phaseMin) * PRPSConstants::GL_AXIS_LENGTH;
}

float PRPSChart::mapAmplitudeToGL(float amplitude) const {
    float displayMin = m_fixedMin, displayMax = m_fixedMax;

    switch (m_rangeMode) {
        case RangeMode::Fixed:
            displayMin = m_fixedMin;
            displayMax = m_fixedMax;
            break;
        case RangeMode::Auto:
        case RangeMode::Adaptive:
            std::tie(displayMin, displayMax) = m_dynamicRange.getDisplayRange();
            break;
    }

    if (amplitude < displayMin) {
        return 0.0f;
    }
    if (amplitude > displayMax) {
        return PRPSConstants::GL_AXIS_LENGTH;
    }

    return (amplitude - displayMin) / (displayMax - displayMin) * PRPSConstants::GL_AXIS_LENGTH;
}

float PRPSChart::mapGLToPhase(float glX) const {
    return glX / PRPSConstants::GL_AXIS_LENGTH * (m_phaseMax - m_phaseMin) + m_phaseMin;
}

float PRPSChart::mapGLToAmplitude(float glY) const {
    float displayMin = m_fixedMin, displayMax = m_fixedMax;

    switch (m_rangeMode) {
        case RangeMode::Fixed:
            displayMin = m_fixedMin;
            displayMax = m_fixedMax;
            break;
        case RangeMode::Auto:
        case RangeMode::Adaptive:
            std::tie(displayMin, displayMax) = m_dynamicRange.getDisplayRange();
            break;
    }

    return glY / PRPSConstants::GL_AXIS_LENGTH * (displayMax - displayMin) + displayMin;
}

void PRPSChart::recalculateLineGroups() {
    makeCurrent();

    const int cap = (m_displayLineCount > 0 && m_displayLineCount < m_phasePoints) ? m_displayLineCount : m_phasePoints;

    for (auto& group : m_lineGroups) {
        group->transforms.clear();
        group->transforms.reserve(static_cast<size_t>(cap));
        buildLineTransformsFromCycle(group->amplitudes, group->transforms);
        group->instanceBufferDirty = true;
    }

    doneCurrent();
    update();
}

// ==================== 暂停/恢复 API 实现 ====================

void PRPSChart::pause(bool blockNewData) {
    m_paused = true;
    m_updateThread.setPaused(true);
    m_acceptData = !blockNewData;
}

void PRPSChart::resume() {
    m_paused = false;
    m_updateThread.setPaused(false);
    m_acceptData = true;
}

void PRPSChart::togglePause() {
    if (m_paused) {
        resume();
    } else {
        pause();
    }
}

} // namespace ProGraphics
