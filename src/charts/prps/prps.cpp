#include "prographics/charts/prps/prps.h"
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

void PRPSChart::initializeGLObjects() {
    Coordinate3D::initializeGLObjects();
}

void PRPSChart::paintGLObjects() {
    Coordinate3D::paintGLObjects();

    if (m_lineGroups.empty()) {
        return;
    }

    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.0f);

    for (const auto& group : m_lineGroups) {
        if (group->instancedLine && group->isActive) {
            QMatrix4x4 model;
            model.translate(0, 0, group->zPosition);

            group->instancedLine->drawInstanced(
                camera().getProjectionMatrix(),
                camera().getViewMatrix() * model,
                group->transforms);
        }
    }

    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
}

void PRPSChart::addCycleData(const std::vector<float>& cycleData) {
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

void PRPSChart::processCurrentCycles() {
    makeCurrent();

    auto newGroup = std::make_unique<LineGroup>();
    const auto& cycleData = m_currentCycles.front();
    newGroup->amplitudes  = cycleData;

    int validCount = 0;
    for (int i = 0; i < m_phasePoints; ++i) {
        float glY = mapAmplitudeToGL(cycleData[i]);
        if (glY > 0.0f)
            validCount++;
    }

    newGroup->instancedLine = std::make_unique<Line2D>(
        QVector3D(0.0f, 0.0f, 0.0f),
        QVector3D(0.0f, 1.0f, 0.0f),
        QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    newGroup->transforms.reserve(validCount);

    for (int i = 0; i < m_phasePoints; ++i) {
        float phase = (float) i / (m_phasePoints - 1) * m_phaseMax;
        float glX   = mapPhaseToGL(phase);
        float glY   = mapAmplitudeToGL(cycleData[i]);
        if (glY <= 0.0f)
            continue;

        Transform2D transform;
        transform.position = QVector2D(glX, 0.0f);
        transform.scale    = QVector2D(1.0f, glY);

        float intensity = glY / PRPSConstants::GL_AXIS_LENGTH;
        transform.color = calculateColor(intensity);

        newGroup->transforms.push_back(transform);
    }

    newGroup->instancedLine->initialize();
    m_lineGroups.push_back(std::move(newGroup));
    doneCurrent();
}

void PRPSChart::updatePRPSAnimation() {
    bool needCleanup = false;

    for (auto& group : m_lineGroups) {
        group->zPosition -= m_prpsAnimationSpeed;

        if (group->zPosition < 2.0f) {
            float alpha = group->zPosition / 2.0f;
            for (auto& transform : group->transforms) {
                transform.color.setW(alpha);
            }
        }

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

    for (auto& group : m_lineGroups) {
        group->transforms.clear();

        int validCount = 0;
        for (float amplitude : group->amplitudes) {
            float glY = mapAmplitudeToGL(amplitude);
            if (glY > 0.0f)
                validCount++;
        }
        group->transforms.reserve(validCount);

        for (int i = 0; i < m_phasePoints; ++i) {
            float phase = (float) i / (m_phasePoints - 1) * m_phaseMax;
            float glX   = mapPhaseToGL(phase);
            float glY   = mapAmplitudeToGL(group->amplitudes[i]);

            if (glY <= 0.0f)
                continue;

            Transform2D transform;
            transform.position = QVector2D(glX, 0.0f);
            transform.scale    = QVector2D(1.0f, glY);
            transform.color    = calculateColor(glY / PRPSConstants::GL_AXIS_LENGTH);

            group->transforms.push_back(transform);
        }
    }

    doneCurrent();
    update();
}

} // namespace ProGraphics
