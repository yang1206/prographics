#include "prographics/charts/prpd/prpd.h"
#include "prographics/charts/prps/prps.h"
#include "prographics/core/graphics/primitive2d.h"
#include "prographics/utils/utils.h"
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <cmath>

namespace ProGraphics {

PRPDChart::PRPDChart(QWidget* parent) : Coordinate2D(parent) {
    setAxisName('x', "Phase", "°");
    setAxisName('y', "", "dBm");

    m_rangeMode = RangeMode::Fixed;
    m_fixedMin = m_configuredMin = -75.0f;
    m_fixedMax = m_configuredMax = -30.0f;

    setTicksRange('x', PRPSConstants::PHASE_MIN, PRPSConstants::PHASE_MAX, 90);
    updateAxisTicks(m_fixedMin, m_fixedMax);
    setAxisVisible('z', false);
    setGridVisible(true);
    setAxisVisible('x', false);
    setAxisVisible('y', false);

    for (auto& phaseRow : m_frequencyTable) {
        std::fill(phaseRow.begin(), phaseRow.end(), 0);
    }
}

PRPDChart::~PRPDChart() {
    if (m_draggingAmplitudeRef) {
        releaseMouse();
        m_draggingAmplitudeRef     = false;
        m_draggingAmplitudeRefIndex = -1;
    }
    makeCurrent();
    m_amplitudeLineDrawers.clear();
    m_pointRenderer.reset();
    doneCurrent();
}

void PRPDChart::initializeGLObjects() {
    Coordinate2D::initializeGLObjects();

    m_pointRenderer = std::make_unique<Point2D>();

    Primitive2DStyle pointStyle;
    pointStyle.pointSize = PRPDConstants::POINT_SIZE;
    m_pointRenderer->setStyle(pointStyle);
    m_pointRenderer->initialize();

    m_cycleBuffer.data.reserve(PRPDConstants::MAX_CYCLES);
    m_cycleBuffer.binIndices.reserve(PRPDConstants::MAX_CYCLES);
    m_renderBatchMap.reserve(100);

    syncAmplitudeLineDrawers();
}

void PRPDChart::paintGLObjects() {
    Coordinate2D::paintGLObjects();

    if (m_pointRenderer && !m_renderBatchMap.empty()) {
        for (auto& [freq, batch] : m_renderBatchMap) {
            if (batch.pointMap.empty())
                continue;

            QVector4D color = calculateColor(batch.frequency);
            batch.rebuildTransforms(color);

            m_pointRenderer->setColor(color);
            m_pointRenderer->drawInstanced(camera().getProjectionMatrix(), camera().getViewMatrix(),
                                             batch.transforms);
        }
    }

    if (hasVisibleAmplitudeLines()) {
        paintAmplitudeLines();
    }
}

bool PRPDChart::hasVisibleAmplitudeLines() const {
    if (m_amplitudeLineSpecs.size() != m_amplitudeLineDrawers.size()) {
        return false;
    }
    for (const auto& spec : m_amplitudeLineSpecs) {
        if (spec.visible) {
            return true;
        }
    }
    return false;
}

void PRPDChart::setAmplitudeLines(const std::vector<AmplitudeLine>& lines) {
    m_amplitudeLineSpecs = lines;
    if (isValid()) {
        makeCurrent();
        syncAmplitudeLineDrawers();
        doneCurrent();
    }
    update();
}

void PRPDChart::clearAmplitudeLines() {
    m_amplitudeLineSpecs.clear();
    if (isValid()) {
        makeCurrent();
        syncAmplitudeLineDrawers();
        doneCurrent();
    }
    update();
}

void PRPDChart::syncAmplitudeLineDrawers() {
    m_amplitudeLineDrawers.clear();

    if (m_amplitudeLineSpecs.empty()) {
        return;
    }

    for (const auto& spec : m_amplitudeLineSpecs) {
        auto ln = std::make_unique<Line2D>(
            QVector3D(0.0f, 0.0f, 0.0f),
            QVector3D(PRPDConstants::GL_AXIS_LENGTH, 0.0f, 0.0f),
            spec.color);

        Primitive2DStyle style;
        style.lineWidth = spec.lineWidth;
        ln->setStyle(style);
        ln->initialize();
        m_amplitudeLineDrawers.push_back(std::move(ln));
    }
}

void PRPDChart::paintAmplitudeLines() {
    if (m_amplitudeLineSpecs.size() != m_amplitudeLineDrawers.size()) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const QMatrix4x4 projection = camera().getProjectionMatrix();
    const QMatrix4x4 view       = camera().getViewMatrix();

    const float x0 = mapPhaseToGL(m_phaseMin);
    const float x1 = mapPhaseToGL(m_phaseMax);

    for (size_t i = 0; i < m_amplitudeLineSpecs.size(); ++i) {
        const AmplitudeLine& spec = m_amplitudeLineSpecs[i];
        if (!spec.visible) {
            continue;
        }

        const float y = mapAmplitudeToGL(spec.amplitudeDbm);

        Line2D* ln = m_amplitudeLineDrawers[i].get();
        ln->setPoints(QVector3D(x0, y, 0.0f), QVector3D(x1, y, 0.0f));
        ln->setColor(spec.color);
        Primitive2DStyle style;
        style.lineWidth = spec.lineWidth;
        ln->setStyle(style);
        ln->draw(projection, view);
    }
}

void PRPDChart::addCycleData(const std::vector<float>& cycleData) {
    if (!m_acceptData) {
        return;
    }
    if (cycleData.size() != m_phasePoints) {
        qWarning() << "Invalid cycle data size:" << cycleData.size() << "expected:" << m_phasePoints;
        return;
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
        rebuildFrequencyTable();
        return;
    }

    std::vector<BinIndex> currentBinIndices(m_phasePoints);
    for (int i = 0; i < m_phasePoints; ++i) {
        currentBinIndices[i] = getAmplitudeBinIndex(cycleData[i]);
    }

    if (m_cycleBuffer.data.size() == PRPDConstants::MAX_CYCLES) {
        const auto& oldestBinIndices = m_cycleBuffer.binIndices[m_cycleBuffer.currentIndex];

        for (int phaseIdx = 0; phaseIdx < m_phasePoints; ++phaseIdx) {
            BinIndex binIdx = oldestBinIndices[phaseIdx];
            if (binIdx < PRPDConstants::AMPLITUDE_BINS) {
                int& freq = m_frequencyTable[phaseIdx][binIdx];
                if (freq > 0) {
                    int oldFreq = freq;
                    freq--;
                    removePointFromBatch(phaseIdx, binIdx, oldFreq);
                    if (freq > 0) {
                        addPointToBatch(phaseIdx, binIdx, freq);
                    }
                }
            }
        }
    }

    if (m_cycleBuffer.data.size() < PRPDConstants::MAX_CYCLES) {
        m_cycleBuffer.data.push_back(cycleData);
        m_cycleBuffer.binIndices.push_back(currentBinIndices);
    } else {
        m_cycleBuffer.data[m_cycleBuffer.currentIndex]       = cycleData;
        m_cycleBuffer.binIndices[m_cycleBuffer.currentIndex] = currentBinIndices;
        m_cycleBuffer.currentIndex = (m_cycleBuffer.currentIndex + 1) % PRPDConstants::MAX_CYCLES;
        m_cycleBuffer.isFull       = true;
    }

    for (int phaseIdx = 0; phaseIdx < m_phasePoints; ++phaseIdx) {
        BinIndex binIdx = currentBinIndices[phaseIdx];
        if (binIdx < PRPDConstants::AMPLITUDE_BINS) {
            int& freq    = m_frequencyTable[phaseIdx][binIdx];
            int  oldFreq = freq;
            freq++;

            if (oldFreq > 0) {
                removePointFromBatch(phaseIdx, binIdx, oldFreq);
            }
            addPointToBatch(phaseIdx, binIdx, freq);

            if (freq > m_maxFrequency) {
                m_maxFrequency = freq;
            }
        }
    }

    // NOTE: 每 10 个周期重新计算最大频次，确保准确性
    static int cycleCount = 0;
    cycleCount            = (cycleCount + 1) % 10;
    if (cycleCount == 0) {
        m_maxFrequency = 0;
        for (const auto& phaseMap : m_frequencyTable) {
            for (int freq : phaseMap) {
                m_maxFrequency = std::max(m_maxFrequency, freq);
            }
        }
    }

    update();
}

void PRPDChart::removePointFromBatch(int phaseIdx, BinIndex binIdx, int frequency) {
    auto it = m_renderBatchMap.find(frequency);
    if (it != m_renderBatchMap.end()) {
        it->second.pointMap.erase({phaseIdx, binIdx});
        it->second.needsRebuild = true;
        if (it->second.pointMap.empty()) {
            m_renderBatchMap.erase(it);
        }
    }
}

void PRPDChart::addPointToBatch(int phaseIdx, BinIndex binIdx, int frequency) {
    float phase     = static_cast<float>(phaseIdx) * (PRPDConstants::PHASE_MAX / m_phasePoints);
    float amplitude = getBinCenterAmplitude(binIdx);

    Transform2D transform;
    transform.position = QVector2D(mapPhaseToGL(phase), mapAmplitudeToGL(amplitude));
    transform.scale    = QVector2D(1.0f, 1.0f);

    auto& batch                        = m_renderBatchMap[frequency];
    batch.frequency                    = frequency;
    batch.pointMap[{phaseIdx, binIdx}] = transform;
    batch.needsRebuild                 = true;
}

void PRPDChart::clearFrequencyTable() {
    for (auto& phaseRow : m_frequencyTable) {
        std::fill(phaseRow.begin(), phaseRow.end(), 0);
    }
    m_maxFrequency = 0;
}

void PRPDChart::updatePointTransformsFromFrequencyTable() {
    m_renderBatchMap.clear();

    for (int phaseIdx = 0; phaseIdx < m_phasePoints; ++phaseIdx) {
        float phase = static_cast<float>(phaseIdx) * (PRPDConstants::PHASE_MAX / m_phasePoints);

        for (BinIndex binIdx = 0; binIdx < PRPDConstants::AMPLITUDE_BINS; ++binIdx) {
            int frequency = m_frequencyTable[phaseIdx][binIdx];
            if (frequency <= 0)
                continue;

            float amplitude = getBinCenterAmplitude(binIdx);

            Transform2D transform;
            transform.position = QVector2D(mapPhaseToGL(phase), mapAmplitudeToGL(amplitude));
            transform.scale    = QVector2D(1.0f, 1.0f);

            auto& batch                        = m_renderBatchMap[frequency];
            batch.frequency                    = frequency;
            batch.pointMap[{phaseIdx, binIdx}] = transform;
            batch.needsRebuild                 = true;
        }
    }
}

QVector4D PRPDChart::calculateColor(int frequency) const {
    float intensity  = static_cast<float>(frequency) / m_maxFrequency;
    float hue        = 240.0f - intensity * 240.0f;
    float saturation = 1.0f;
    float value      = 0.8f + intensity * 0.2f;
    float alpha      = 0.6f + intensity * 0.4f;
    return hsvToRgb(hue, saturation, value, alpha);
}

// ==================== 量程设置 API 实现 ====================

void PRPDChart::setFixedRange(float min, float max) {
    m_rangeMode = RangeMode::Fixed;
    m_fixedMin = m_configuredMin = min;
    m_fixedMax = m_configuredMax = max;
    updateAxisTicks(min, max);
    rebuildFrequencyTable();
    update();
}

void PRPDChart::setAutoRange(const DynamicRange::DynamicRangeConfig& config) {
    m_rangeMode = RangeMode::Auto;
    m_dynamicRange.setConfig(config);

    auto [currentMin, currentMax] = m_dynamicRange.getDisplayRange();
    m_configuredMin               = currentMin;
    m_configuredMax               = currentMax;

    updateAxisTicks(currentMin, currentMax);
    rebuildFrequencyTable();
    update();
}

void PRPDChart::setAdaptiveRange(float initialMin, float initialMax, const DynamicRange::DynamicRangeConfig& config) {
    m_rangeMode     = RangeMode::Adaptive;
    m_configuredMin = initialMin;
    m_configuredMax = initialMax;

    m_dynamicRange.setConfig(config);
    m_dynamicRange.setInitialRange(initialMin, initialMax);

    auto [currentMin, currentMax] = m_dynamicRange.getDisplayRange();
    updateAxisTicks(currentMin, currentMax);
    rebuildFrequencyTable();
    update();
}

// ==================== 量程查询 API 实现 ====================

std::pair<float, float> PRPDChart::getCurrentRange() const {
    switch (m_rangeMode) {
        case RangeMode::Fixed:
            return {m_fixedMin, m_fixedMax};
        case RangeMode::Auto:
        case RangeMode::Adaptive:
            return m_dynamicRange.getDisplayRange();
    }
    return {0, 0};
}

std::pair<float, float> PRPDChart::getConfiguredRange() const {
    return {m_configuredMin, m_configuredMax};
}

// ==================== 运行时调整 API 实现 ====================

void PRPDChart::updateAutoRangeConfig(const DynamicRange::DynamicRangeConfig& config) {
    if (m_rangeMode == RangeMode::Auto || m_rangeMode == RangeMode::Adaptive) {
        m_dynamicRange.setConfig(config);
        auto [currentMin, currentMax] = m_dynamicRange.getDisplayRange();
        updateAxisTicks(currentMin, currentMax);
        rebuildFrequencyTable();
        update();
    }
}

void PRPDChart::switchToFixedRange(float min, float max) {
    setFixedRange(min, max);
}

void PRPDChart::switchToAutoRange() {
    setAutoRange();
}

// ==================== 硬限制 API 实现 ====================

void PRPDChart::setHardLimits(float min, float max, bool enabled) {
    m_dynamicRange.setHardLimits(min, max, enabled);
    if (m_rangeMode != RangeMode::Fixed) {
        forceUpdateRange();
    }
}

std::pair<float, float> PRPDChart::getHardLimits() const {
    return m_dynamicRange.getHardLimits();
}

void PRPDChart::enableHardLimits(bool enabled) {
    m_dynamicRange.enableHardLimits(enabled);
    if (m_rangeMode != RangeMode::Fixed) {
        forceUpdateRange();
    }
}

bool PRPDChart::isHardLimitsEnabled() const {
    return m_dynamicRange.isHardLimitsEnabled();
}

// ==================== 私有方法实现 ====================

void PRPDChart::forceUpdateRange() {
    auto [newMin, newMax] = m_dynamicRange.getDisplayRange();
    updateAxisTicks(newMin, newMax);
    rebuildFrequencyTable();
    update();
}

void PRPDChart::updateAxisTicks(float min, float max) {
    int targetTicks = m_dynamicRange.getConfig().targetTickCount;
    float step = calculateNiceTickStep(max - min, targetTicks);
    setTicksRange('y', min, max, step);
}

void PRPDChart::setPhaseRange(float min, float max) {
    m_phaseMin = min;
    m_phaseMax = max;
    setTicksRange('x', min, max, 85);
    update();
}

float PRPDChart::mapPhaseToGL(float phase) const {
    return (phase - m_phaseMin) / (m_phaseMax - m_phaseMin) * PRPDConstants::GL_AXIS_LENGTH;
}

void PRPDChart::queryDisplayedAmplitudeRange(float &displayMin, float &displayMax) const {
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
}

float PRPDChart::mapAmplitudeToGL(float amplitude) const {
    float displayMin = 0.f;
    float displayMax = 0.f;
    queryDisplayedAmplitudeRange(displayMin, displayMax);

    if (amplitude < displayMin) {
        return 0.0f;
    }
    if (amplitude > displayMax) {
        return PRPDConstants::GL_AXIS_LENGTH;
    }

    return (amplitude - displayMin) / (displayMax - displayMin) * PRPDConstants::GL_AXIS_LENGTH;
}

QVector3D PRPDChart::unprojectWidgetToChartPlane(const QPoint &widgetPos) const {
    const float w = std::max(1, width());
    const float h = std::max(1, height());
    const float nx = 2.0f * static_cast<float>(widgetPos.x()) / w - 1.0f;
    const float ny = 1.0f - 2.0f * static_cast<float>(widgetPos.y()) / h;
    const QMatrix4x4 pv = camera().getProjectionMatrix() * camera().getViewMatrix();
    bool invertible = false;
    const QMatrix4x4 inv = pv.inverted(&invertible);
    if (!invertible) {
        return QVector3D();
    }
    QVector4D world = inv * QVector4D(nx, ny, 0.0f, 1.0f);
    if (std::abs(world.w()) > 1e-8f) {
        world /= world.w();
    }
    return QVector3D(world.x(), world.y(), world.z());
}

QPointF PRPDChart::projectChartToWidget(const QVector3D &chartPos) const {
    const QMatrix4x4    pv   = camera().getProjectionMatrix() * camera().getViewMatrix();
    QVector4D           clip = pv * QVector4D(chartPos, 1.0f);
    const float         iw   = clip.w();
    if (std::abs(iw) > 1e-8f) {
        clip /= iw;
    }
    const float ww = std::max(1, width());
    const float hh = std::max(1, height());
    const float sx = (clip.x() * 0.5f + 0.5f) * ww;
    const float sy = (1.0f - (clip.y() * 0.5f + 0.5f)) * hh;
    return QPointF(sx, sy);
}

float PRPDChart::snapDragAmplitude(float amplitudeDbm) const {
    float displayMin = 0.f;
    float displayMax = 0.f;
    queryDisplayedAmplitudeRange(displayMin, displayMax);
    float a = std::clamp(amplitudeDbm, displayMin, displayMax);
    if (m_dragSnapStep > 1e-20f) {
        a = std::round(a / m_dragSnapStep) * m_dragSnapStep;
    }
    return std::clamp(a, displayMin, displayMax);
}

float PRPDChart::amplitudeFromWidgetPos(const QPoint &pos) const {
    const QVector3D c = unprojectWidgetToChartPlane(pos);
    float           displayMin = 0.f;
    float           displayMax = 0.f;
    queryDisplayedAmplitudeRange(displayMin, displayMax);
    const float L = PRPDConstants::GL_AXIS_LENGTH;
    float       chartY = c.y();
    float       linearAmp;
    if (chartY <= 0.f) {
        linearAmp = displayMin;
    } else if (chartY >= L) {
        linearAmp = displayMax;
    } else {
        linearAmp = (chartY / L) * (displayMax - displayMin) + displayMin;
    }
    return snapDragAmplitude(linearAmp);
}

int PRPDChart::pickAmplitudeLineAt(const QPoint &widgetPos) const {
    if (!m_amplitudeRefDragEnabled) {
        return -1;
    }
    if (m_amplitudeLineSpecs.size() != m_amplitudeLineDrawers.size()) {
        return -1;
    }

    const QVector3D chartHit = unprojectWidgetToChartPlane(widgetPos);
    const float       x0     = mapPhaseToGL(m_phaseMin);
    const float       x1     = mapPhaseToGL(m_phaseMax);
    const float       xmin   = std::min(x0, x1);
    const float       xmax   = std::max(x0, x1);
    if (chartHit.x() < xmin || chartHit.x() > xmax) {
        return -1;
    }

    int         bestIdx    = -1;
    float       bestPixelDy = m_dragPickTolerancePx + 1.f;
    const float midX       = 0.5f * (x0 + x1);

    for (size_t i = 0; i < m_amplitudeLineSpecs.size(); ++i) {
        if (!m_amplitudeLineSpecs[i].visible) {
            continue;
        }
        const float yGl      = mapAmplitudeToGL(m_amplitudeLineSpecs[i].amplitudeDbm);
        const QPointF screenMid = projectChartToWidget(QVector3D(midX, yGl, 0.0f));
        const float dy =
            std::abs(static_cast<float>(widgetPos.y()) - static_cast<float>(screenMid.y()));
        if (dy <= m_dragPickTolerancePx && dy < bestPixelDy) {
            bestPixelDy = dy;
            bestIdx     = static_cast<int>(i);
        }
    }
    return bestIdx;
}

void PRPDChart::setAmplitudeReferenceLinesDraggable(bool enabled) {
    if (m_amplitudeRefDragEnabled == enabled) {
        return;
    }
    if (!enabled && m_draggingAmplitudeRef) {
        const int idx = m_draggingAmplitudeRefIndex;
        float     amp = 0.f;
        if (idx >= 0 && idx < static_cast<int>(m_amplitudeLineSpecs.size())) {
            amp = m_amplitudeLineSpecs[static_cast<size_t>(idx)].amplitudeDbm;
        }
        m_draggingAmplitudeRef      = false;
        m_draggingAmplitudeRefIndex = -1;
        setMouseTracking(false);
        releaseMouse();
        emit amplitudeReferenceLineDragEnded(idx, amp);
    }
    m_amplitudeRefDragEnabled = enabled;
}

void PRPDChart::setAmplitudeReferenceLineDragSnapStep(float step) {
    m_dragSnapStep = step > 0.f ? step : 1.f;
}

void PRPDChart::setAmplitudeReferenceLinePickTolerancePx(float pixels) {
    m_dragPickTolerancePx = std::max(1.f, pixels);
}

void PRPDChart::mousePressEvent(QMouseEvent *event) {
    if (!m_amplitudeRefDragEnabled || event->button() != Qt::LeftButton) {
        QOpenGLWidget::mousePressEvent(event);
        return;
    }
    const int idx = pickAmplitudeLineAt(event->pos());
    if (idx < 0) {
        QOpenGLWidget::mousePressEvent(event);
        return;
    }
    m_draggingAmplitudeRef      = true;
    m_draggingAmplitudeRefIndex = idx;
    setMouseTracking(true);
    grabMouse();
    event->accept();
    const float startAmp = m_amplitudeLineSpecs[static_cast<size_t>(idx)].amplitudeDbm;
    emit amplitudeReferenceLineDragStarted(idx, startAmp);
}

void PRPDChart::mouseMoveEvent(QMouseEvent *event) {
    if (m_draggingAmplitudeRef && m_draggingAmplitudeRefIndex >= 0) {
        const int idx = m_draggingAmplitudeRefIndex;
        if (idx >= static_cast<int>(m_amplitudeLineSpecs.size())) {
            m_draggingAmplitudeRef      = false;
            m_draggingAmplitudeRefIndex = -1;
            setMouseTracking(false);
            releaseMouse();
            QOpenGLWidget::mouseMoveEvent(event);
            return;
        }
        const float newAmp = amplitudeFromWidgetPos(event->pos());
        float &stored = m_amplitudeLineSpecs[static_cast<size_t>(idx)].amplitudeDbm;
        if (std::abs(newAmp - stored) > 1e-6f) {
            stored = newAmp;
            update();
            emit amplitudeReferenceLineDragged(idx, newAmp);
        }
        event->accept();
        return;
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void PRPDChart::mouseReleaseEvent(QMouseEvent *event) {
    if (m_draggingAmplitudeRef && event->button() == Qt::LeftButton) {
        const int idx = m_draggingAmplitudeRefIndex;
        float     amp = 0.f;
        if (idx >= 0 && idx < static_cast<int>(m_amplitudeLineSpecs.size())) {
            amp = m_amplitudeLineSpecs[static_cast<size_t>(idx)].amplitudeDbm;
        }
        m_draggingAmplitudeRef      = false;
        m_draggingAmplitudeRefIndex = -1;
        setMouseTracking(false);
        releaseMouse();
        emit amplitudeReferenceLineDragEnded(idx, amp);
        event->accept();
        return;
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void PRPDChart::rebuildFrequencyTable() {
    clearFrequencyTable();

    for (size_t i = 0; i < m_cycleBuffer.data.size(); ++i) {
        const auto& cycle = m_cycleBuffer.data[i];

        std::vector<BinIndex> newBinIndices(m_phasePoints);
        for (int j = 0; j < m_phasePoints; ++j) {
            newBinIndices[j] = getAmplitudeBinIndex(cycle[j]);
        }

        m_cycleBuffer.binIndices[i] = newBinIndices;

        for (int phaseIdx = 0; phaseIdx < m_phasePoints; ++phaseIdx) {
            BinIndex binIdx = newBinIndices[phaseIdx];
            if (binIdx < PRPDConstants::AMPLITUDE_BINS) {
                int& freq = m_frequencyTable[phaseIdx][binIdx];
                freq++;
                if (freq > m_maxFrequency) {
                    m_maxFrequency = freq;
                }
            }
        }
    }

    updatePointTransformsFromFrequencyTable();
    update();
}

PRPDChart::BinIndex PRPDChart::getAmplitudeBinIndex(float amplitude) const {
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

    if (amplitude <= displayMin) {
        return 0;
    }
    if (amplitude >= displayMax) {
        return PRPDConstants::AMPLITUDE_BINS - 1;
    }

    float range = displayMax - displayMin;
    if (range < 1e-6f) {
        return PRPDConstants::AMPLITUDE_BINS / 2;
    }

    float normalizedPos = (amplitude - displayMin) / range;
    normalizedPos       = std::max(0.0f, std::min(0.9999f, normalizedPos));
    int binIndex        = static_cast<int>(normalizedPos * PRPDConstants::AMPLITUDE_BINS);

    return std::clamp(binIndex, 0, PRPDConstants::AMPLITUDE_BINS - 1);
}

float PRPDChart::getBinCenterAmplitude(BinIndex binIndex) const {
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

    if (binIndex >= PRPDConstants::AMPLITUDE_BINS) {
        return displayMin;
    }

    float normalizedPos = (binIndex + 0.5f) / PRPDConstants::AMPLITUDE_BINS;
    return displayMin + normalizedPos * (displayMax - displayMin);
}

void PRPDChart::setPhasePoint(int phasePoint) {
    m_phasePoints = phasePoint;
}

// ==================== 暂停/恢复 API 实现 ====================

void PRPDChart::pause(bool blockNewData) {
    m_paused = true;
    m_acceptData = !blockNewData;
}

void PRPDChart::resume() {
    m_paused = false;
    m_acceptData = true;
}

void PRPDChart::togglePause() {
    if (m_paused) {
        resume();
    } else {
        pause();
    }
}

} // namespace ProGraphics
