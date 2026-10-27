#include "prographics/charts/prpd/prpd.h"
#include "prographics/core/graphics/primitive2d.h"
#include "prographics/utils/utils.h"
#include "charts/cycle_phase_p.h"
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <cmath>
#include <limits>

namespace ProGraphics {

PRPDChart::PRPDChart(QWidget* parent) : Coordinate2D(parent) {
    setAxisName('x', "Phase", "°");
    setAxisName('y', "", "dBm");

    m_rangeMode = RangeMode::Fixed;
    m_fixedMin = m_configuredMin = -75.0f;
    m_fixedMax = m_configuredMax = -30.0f;

    setTicksRange('x', PRPDConstants::PHASE_MIN, PRPDConstants::PHASE_MAX, 90);
    updateAxisTicks(m_fixedMin, m_fixedMax);
    setAxisVisible('z', false);
    setGridVisible(true);
    setAxisVisible('x', false);
    setAxisVisible('y', false);

    clearFrequencyTable();
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
    m_pointDataDirty = true;

    m_cycleBuffer.data.reserve(PRPDConstants::MAX_CYCLES);
    m_cycleBuffer.binIndices.reserve(PRPDConstants::MAX_CYCLES);
    syncAmplitudeLineDrawers();
}

void PRPDChart::paintGLObjects() {
    if (m_frequencyTableDirty) {
        auto timing = measureRenderStage(RenderStage::Data);
        rebuildFrequencyTable();
    }
    Coordinate2D::paintGLObjects();

    if (renderOptions().data && m_pointRenderer && !m_activeCells.empty()) {
        auto timing = measureRenderStage(RenderStage::Data);
        const bool uploadInstanceData = m_pointDataDirty;
        if (m_pointDataDirty) {
            if (m_pointCoordinatesDirty) {
                m_phasePositions.resize(static_cast<size_t>(m_phasePoints));
                for (int i = 0; i < m_phasePoints; ++i) {
                    const double phase = CyclePhase::samplePhase(i, m_phasePoints, m_phaseMin, m_phaseMax);
                    m_phasePositions[static_cast<size_t>(i)] = mapPhaseToGL(phase);
                }
                for (BinIndex bin = 0; bin < PRPDConstants::AMPLITUDE_BINS; ++bin) {
                    m_binPositions[bin] = mapAmplitudeToGL(getBinCenterAmplitude(bin));
                }
                m_pointCoordinatesDirty = false;
            }
            if (m_colorMaxFrequency != m_maxFrequency) {
                for (int frequency = 1; frequency <= m_maxFrequency; ++frequency) {
                    m_frequencyColors[frequency] = calculateColor(frequency);
                }
                m_colorMaxFrequency = m_maxFrequency;
            }
            m_pointTransforms.resize(m_activeCells.size());
            for (size_t i = 0; i < m_activeCells.size(); ++i) {
                const size_t cell = m_activeCells[i];
                const size_t phase = cell / PRPDConstants::AMPLITUDE_BINS;
                const size_t bin = cell % PRPDConstants::AMPLITUDE_BINS;
                auto& transform = m_pointTransforms[i];
                transform.position = QVector2D(m_phasePositions[phase], m_binPositions[bin]);
                transform.color = m_frequencyColors[m_frequencyTable[phase][bin]];
            }
        }
        // 旧文字路径在 QPainter::end() 后关闭混合和深度测试，PRPD 点直接覆盖。
        // 文字缓存会开启混合，必须在点层明确设置状态，避免缓存开关改变点的颜色。
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        m_pointRenderer->drawInstanced(camera().getProjectionMatrix(), camera().getViewMatrix(),
                                      m_pointTransforms, -1.0f, uploadInstanceData);
        // 后续参考线继续使用自身颜色的透明度。
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        m_pointDataDirty = false;
    }

    if (renderOptions().referenceLines && hasVisibleAmplitudeLines()) {
        auto timing = measureRenderStage(RenderStage::ReferenceLines);
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
        if (!spec.visible || !std::isfinite(spec.amplitudeDbm) ||
            spec.amplitudeDbm < m_displayMin || spec.amplitudeDbm > m_displayMax) {
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

    m_rangeWindow.append(cycleData);
    updateRangeFromWindow();
    const auto [newDisplayMin, newDisplayMax] = getCurrentRange();

    const bool full = m_cycleBuffer.data.size() == PRPDConstants::MAX_CYCLES;
    const size_t index = full ? static_cast<size_t>(m_cycleBuffer.currentIndex) : m_cycleBuffer.data.size();
    if (!full) {
        m_cycleBuffer.data.emplace_back();
        m_cycleBuffer.binIndices.emplace_back(static_cast<size_t>(m_phasePoints));
    }

    auto& bins = m_cycleBuffer.binIndices[index];
    if (!m_frequencyTableDirty) {
        for (int phase = 0; phase < m_phasePoints; ++phase) {
            const BinIndex bin = getAmplitudeBinIndex(cycleData[phase], newDisplayMin, newDisplayMax);
            // 满窗口中旧值与新值落在同一格时，统计及颜色完全不变。
            if (full && bins[phase] == bin) continue;
            if (full) changeFrequency(phase, bins[phase], -1);
            changeFrequency(phase, bin, 1);
            bins[phase] = bin;
        }
    }
    m_cycleBuffer.data[index] = cycleData;
    if (full) {
        m_cycleBuffer.currentIndex = (m_cycleBuffer.currentIndex + 1) % PRPDConstants::MAX_CYCLES;
        m_cycleBuffer.isFull       = true;
    }

    while (m_maxFrequency > 0 && m_frequencyCounts[m_maxFrequency] == 0) {
        --m_maxFrequency;
    }
    if (m_pointDataDirty || m_frequencyTableDirty) {
        update();
    }
}

void PRPDChart::changeFrequency(int phaseIdx, BinIndex binIdx, int delta) {
    // NaN/Inf 不参与统计，也不能作为数组下标。
    if (binIdx >= PRPDConstants::AMPLITUDE_BINS) return;
    int& frequency = m_frequencyTable[phaseIdx][binIdx];
    const int oldFrequency = frequency;
    if (oldFrequency > 0) --m_frequencyCounts[oldFrequency];
    frequency += delta;
    if (frequency > 0) ++m_frequencyCounts[frequency];

    const size_t cell = static_cast<size_t>(phaseIdx) * PRPDConstants::AMPLITUDE_BINS + binIdx;
    if (oldFrequency == 0) {
        m_activeCellPositions[cell] = m_activeCells.size();
        m_activeCells.push_back(cell);
    } else if (frequency == 0) {
        const size_t position = m_activeCellPositions[cell];
        const size_t lastCell = m_activeCells.back();
        m_activeCells[position] = lastCell;
        m_activeCellPositions[lastCell] = position;
        m_activeCells.pop_back();
        m_activeCellPositions[cell] = std::numeric_limits<size_t>::max();
    }
    m_maxFrequency = std::max(m_maxFrequency, frequency);
    m_pointDataDirty = true;
}

void PRPDChart::clearFrequencyTable() {
    for (auto& phaseRow : m_frequencyTable) {
        std::fill(phaseRow.begin(), phaseRow.end(), 0);
    }
    m_maxFrequency = 0;
    m_frequencyCounts.fill(0);
    m_activeCells.clear();
    m_activeCellPositions.assign(static_cast<size_t>(m_phasePoints) * PRPDConstants::AMPLITUDE_BINS,
                                 std::numeric_limits<size_t>::max());
    m_frequencyTableDirty = false;
    m_pointCoordinatesDirty = true;
    m_colorMaxFrequency = -1;
    m_pointDataDirty = true;
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
    LinearScale::Range range;
    if (!LinearScale::normalize(min, max, range, 0.0)) {
        qWarning() << "PRPD 量程端点必须为有限数值";
        return;
    }
    m_rangeMode = RangeMode::Fixed;
    m_fixedMin = m_configuredMin = range.first;
    m_fixedMax = m_configuredMax = range.second;
    applyCurrentRange(true);
}

void PRPDChart::setAutoRange(const DynamicRange::DynamicRangeConfig& config) {
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

void PRPDChart::setAdaptiveRange(float initialMin, float initialMax, const DynamicRange::DynamicRangeConfig& config) {
    LinearScale::Range range;
    if (!LinearScale::normalize(initialMin, initialMax, range, 0.0)) {
        qWarning() << "PRPD 初始量程端点必须为有限数值";
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

std::pair<float, float> PRPDChart::getCurrentRange() const {
    return {m_displayMin, m_displayMax};
}

std::pair<float, float> PRPDChart::getConfiguredRange() const {
    return {m_configuredMin, m_configuredMax};
}

// ==================== 运行时调整 API 实现 ====================

void PRPDChart::updateAutoRangeConfig(const DynamicRange::DynamicRangeConfig& config) {
    if (m_rangeMode == RangeMode::Auto || m_rangeMode == RangeMode::Adaptive) {
        m_dynamicRange.setConfig(config);
        forceUpdateRange();
    }
}

void PRPDChart::switchToFixedRange(float min, float max) {
    setFixedRange(min, max);
}

void PRPDChart::switchToAutoRange() {
    setAutoRange(m_dynamicRange.getConfig());
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
    updateRangeFromWindow();
    applyCurrentRange(true);
}

void PRPDChart::updateRangeFromWindow() {
    LinearScale::Range bounds;
    if (m_rangeMode != RangeMode::Fixed && m_rangeWindow.bounds(bounds)) {
        m_dynamicRange.updateBounds(bounds.first, bounds.second);
        applyCurrentRange();
    }
}

void PRPDChart::applyCurrentRange(bool forceTicks) {
    const auto range = m_rangeMode == RangeMode::Fixed
        ? LinearScale::Range{m_fixedMin, m_fixedMax} : m_dynamicRange.getDisplayRange();
    const bool changed = range != getCurrentRange();
    if (!changed && !forceTicks) return;
    m_displayMin = range.first;
    m_displayMax = range.second;
    updateAxisTicks(range.first, range.second);
    if (changed) {
        // 刻度与映射先共同提交，保留周期在下次绘制前只重分箱一次。
        m_frequencyMin = range.first;
        m_frequencyMax = range.second;
        m_frequencyTableDirty = true;
        m_pointCoordinatesDirty = true;
        m_pointDataDirty = true;
    }
    update();
    if (changed) emit rangeChanged(range.first, range.second);
}

void PRPDChart::updateAxisTicks(float min, float max) {
    int targetTicks = m_dynamicRange.getConfig().targetTickCount;
    // 固定量程沿用旧刻度选择，自动量程与新的拟合算法使用相同步长。
    const double requestedStep = m_rangeMode == RangeMode::Fixed
        ? LinearScale::ceilingTickStep(static_cast<double>(max) - min, targetTicks)
        : LinearScale::tickStep(static_cast<double>(max) - min, targetTicks);
    const float step = static_cast<float>(std::clamp(requestedStep,
        static_cast<double>(std::numeric_limits<float>::denorm_min()),
        static_cast<double>(std::numeric_limits<float>::max())));
    setTicksRange('y', min, max, step);
}

void PRPDChart::setPhaseRange(float min, float max) {
    if (!CyclePhase::validRange(min, max)) {
        qWarning() << "PRPD 相位范围必须为有限数值且下限小于上限";
        return;
    }
    if (min == m_phaseMin && max == m_phaseMax) return;
    m_phaseMin = min;
    m_phaseMax = max;
    m_pointCoordinatesDirty = true;
    m_pointDataDirty = true;
    setTicksRange('x', min, max, CyclePhase::tickStep(min, max));
    update();
}

float PRPDChart::mapPhaseToGL(double phase) const {
    return CyclePhase::toPosition(phase, m_phaseMin, m_phaseMax, PRPDConstants::GL_AXIS_LENGTH);
}

void PRPDChart::queryDisplayedAmplitudeRange(float &displayMin, float &displayMax) const {
    std::tie(displayMin, displayMax) = getCurrentRange();
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

    return static_cast<float>((static_cast<double>(amplitude) - displayMin) /
        (static_cast<double>(displayMax) - displayMin) * PRPDConstants::GL_AXIS_LENGTH);
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
        linearAmp = static_cast<float>((chartY / L) * (static_cast<double>(displayMax) - displayMin) + displayMin);
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
        const auto& spec = m_amplitudeLineSpecs[i];
        if (!spec.visible || !std::isfinite(spec.amplitudeDbm) ||
            spec.amplitudeDbm < m_displayMin || spec.amplitudeDbm > m_displayMax) {
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
    std::tie(m_frequencyMin, m_frequencyMax) = getCurrentRange();

    for (size_t i = 0; i < m_cycleBuffer.data.size(); ++i) {
        const auto& cycle = m_cycleBuffer.data[i];

        auto& newBinIndices = m_cycleBuffer.binIndices[i];
        for (int j = 0; j < m_phasePoints; ++j) {
            newBinIndices[j] = getAmplitudeBinIndex(cycle[j], m_frequencyMin, m_frequencyMax);
            changeFrequency(j, newBinIndices[j], 1);
        }
    }
}

PRPDChart::BinIndex PRPDChart::getAmplitudeBinIndex(float amplitude, float displayMin, float displayMax) {
    if (!std::isfinite(amplitude)) return PRPDConstants::AMPLITUDE_BINS;
    if (amplitude <= displayMin) {
        return 0;
    }
    if (amplitude >= displayMax) {
        return PRPDConstants::AMPLITUDE_BINS - 1;
    }

    const double range = static_cast<double>(displayMax) - displayMin;
    float normalizedPos = static_cast<float>((static_cast<double>(amplitude) - displayMin) / range);
    normalizedPos       = std::max(0.0f, std::min(0.9999f, normalizedPos));
    int binIndex        = static_cast<int>(normalizedPos * PRPDConstants::AMPLITUDE_BINS);

    return std::clamp(binIndex, 0, PRPDConstants::AMPLITUDE_BINS - 1);
}

float PRPDChart::getBinCenterAmplitude(BinIndex binIndex) const {
    const auto [displayMin, displayMax] = getCurrentRange();

    if (binIndex >= PRPDConstants::AMPLITUDE_BINS) {
        return displayMin;
    }

    float normalizedPos = (binIndex + 0.5f) / PRPDConstants::AMPLITUDE_BINS;
    return static_cast<float>(displayMin + normalizedPos * (static_cast<double>(displayMax) - displayMin));
}

void PRPDChart::setPhasePoint(int phasePoint) {
    if (phasePoint <= 0) {
        qWarning() << "PRPD 相位采样点数必须大于 0:" << phasePoint;
        return;
    }
    if (phasePoint == m_phasePoints) {
        return;
    }

    // 先分配成功再切换配置，旧周期不能按新的相位长度读取。
    FrequencyTable frequencyTable(static_cast<size_t>(phasePoint));
    m_frequencyTable.swap(frequencyTable);
    m_phasePoints = phasePoint;
    resetData();
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
