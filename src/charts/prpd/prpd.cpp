#include "prographics/charts/prpd/prpd.h"

#include <QMouseEvent>

#include "prographics/charts/prps/prps.h"
#include "prographics/utils/utils.h"

namespace ProGraphics {
    PRPDChart::PRPDChart(QWidget *parent) : Coordinate2D(parent) {
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

        for (auto &phaseRow: m_frequencyTable) {
            std::fill(phaseRow.begin(), phaseRow.end(), 0);
        }
    }

    PRPDChart::~PRPDChart() {
        makeCurrent();
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
    }

    void PRPDChart::paintGLObjects() {
        Coordinate2D::paintGLObjects();

        if (!m_pointRenderer || m_renderBatchMap.empty()) {
            return;
        }

        for (auto &[freq, batch]: m_renderBatchMap) {
            if (batch.pointMap.empty())
                continue;

            QVector4D color = calculateColor(batch.frequency);
            batch.rebuildTransforms(color);

            m_pointRenderer->setColor(color);
            m_pointRenderer->drawInstanced(camera().getProjectionMatrix(), camera().getViewMatrix(), batch.transforms);
        }
        glPopAttrib();
    }

    void PRPDChart::paintGL() {
        Coordinate2D::paintGL();

        if (!m_hasSelection || !m_selectionBoxEnabled) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QRectF screenRect = dataRectToScreen(m_selectionDataRect);

        QPen pen(QColor(255, 165, 0, 255));
        pen.setWidth(2);
        painter.setPen(pen);

        QBrush brush(QColor(255, 165, 0, 20));
        painter.setBrush(brush);

        painter.drawRect(screenRect);

        painter.setBrush(QColor(255, 165, 0, 255));
        float handleSize = 8.0f;

        QPointF handles[4] = {screenRect.topLeft(), screenRect.topRight(), screenRect.bottomLeft(),
                              screenRect.bottomRight()};

        for (const auto &handle : handles) {
            painter.drawEllipse(handle, handleSize / 2, handleSize / 2);
        }

        QFont font = painter.font();
        font.setPointSize(9);
        painter.setFont(font);
        painter.setPen(QColor(255, 255, 255));

        QString info = QString("Phase: %1° - %2°\nAmp: %3 - %4 dBm")
                           .arg(m_selectionDataRect.left(), 0, 'f', 1)
                           .arg(m_selectionDataRect.right(), 0, 'f', 1)
                           .arg(m_selectionDataRect.bottom(), 0, 'f', 1)
                           .arg(m_selectionDataRect.top(), 0, 'f', 1);

        QRectF textRect = screenRect.adjusted(5, 5, -5, -5);
        painter.drawText(textRect, Qt::AlignTop | Qt::AlignLeft, info);

        painter.end();
    }

    void PRPDChart::addCycleData(const std::vector<float> &cycleData) {
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
            const auto &oldestBinIndices = m_cycleBuffer.binIndices[m_cycleBuffer.currentIndex];

            for (int phaseIdx = 0; phaseIdx < m_phasePoints; ++phaseIdx) {
                BinIndex binIdx = oldestBinIndices[phaseIdx];
                if (binIdx < PRPDConstants::AMPLITUDE_BINS) {
                    int &freq = m_frequencyTable[phaseIdx][binIdx];
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
            m_cycleBuffer.data[m_cycleBuffer.currentIndex] = cycleData;
            m_cycleBuffer.binIndices[m_cycleBuffer.currentIndex] = currentBinIndices;
            m_cycleBuffer.currentIndex = (m_cycleBuffer.currentIndex + 1) % PRPDConstants::MAX_CYCLES;
            m_cycleBuffer.isFull = true;
        }

        for (int phaseIdx = 0; phaseIdx < m_phasePoints; ++phaseIdx) {
            BinIndex binIdx = currentBinIndices[phaseIdx];
            if (binIdx < PRPDConstants::AMPLITUDE_BINS) {
                int &freq = m_frequencyTable[phaseIdx][binIdx];
                int oldFreq = freq;
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
        cycleCount = (cycleCount + 1) % 10;
        if (cycleCount == 0) {
            m_maxFrequency = 0;
            for (const auto &phaseMap: m_frequencyTable) {
                for (int freq: phaseMap) {
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
        float phase = static_cast<float>(phaseIdx) * (PRPDConstants::PHASE_MAX / m_phasePoints);
        float amplitude = getBinCenterAmplitude(binIdx);

        Transform2D transform;
        transform.position = QVector2D(mapPhaseToGL(phase), mapAmplitudeToGL(amplitude));
        transform.scale = QVector2D(1.0f, 1.0f);

        auto &batch = m_renderBatchMap[frequency];
        batch.frequency = frequency;
        batch.pointMap[{phaseIdx, binIdx}] = transform;
        batch.needsRebuild = true;
    }

    void PRPDChart::clearFrequencyTable() {
        for (auto &phaseRow: m_frequencyTable) {
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
                transform.scale = QVector2D(1.0f, 1.0f);

                auto &batch = m_renderBatchMap[frequency];
                batch.frequency = frequency;
                batch.pointMap[{phaseIdx, binIdx}] = transform;
                batch.needsRebuild = true;
            }
        }
    }

    QVector4D PRPDChart::calculateColor(int frequency) const {
        float intensity = static_cast<float>(frequency) / m_maxFrequency;
        float hue = 240.0f - intensity * 240.0f;
        float saturation = 1.0f;
        float value = 0.8f + intensity * 0.2f;
        float alpha = 0.6f + intensity * 0.4f;
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

    void PRPDChart::setAutoRange(const DynamicRange::DynamicRangeConfig &config) {
        m_rangeMode = RangeMode::Auto;
        m_dynamicRange.setConfig(config);

        auto [currentMin, currentMax] = m_dynamicRange.getDisplayRange();
        m_configuredMin = currentMin;
        m_configuredMax = currentMax;

        updateAxisTicks(currentMin, currentMax);
        rebuildFrequencyTable();
        update();
    }

    void PRPDChart::setAdaptiveRange(float initialMin, float initialMax,
                                     const DynamicRange::DynamicRangeConfig &config) {
        m_rangeMode = RangeMode::Adaptive;
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

    void PRPDChart::updateAutoRangeConfig(const DynamicRange::DynamicRangeConfig &config) {
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

    float PRPDChart::mapAmplitudeToGL(float amplitude) const {
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
            return PRPDConstants::GL_AXIS_LENGTH;
        }

        return (amplitude - displayMin) / (displayMax - displayMin) * PRPDConstants::GL_AXIS_LENGTH;
    }

    void PRPDChart::rebuildFrequencyTable() {
        clearFrequencyTable();

        for (size_t i = 0; i < m_cycleBuffer.data.size(); ++i) {
            const auto &cycle = m_cycleBuffer.data[i];

            std::vector<BinIndex> newBinIndices(m_phasePoints);
            for (int j = 0; j < m_phasePoints; ++j) {
                newBinIndices[j] = getAmplitudeBinIndex(cycle[j]);
            }

            m_cycleBuffer.binIndices[i] = newBinIndices;

            for (int phaseIdx = 0; phaseIdx < m_phasePoints; ++phaseIdx) {
                BinIndex binIdx = newBinIndices[phaseIdx];
                if (binIdx < PRPDConstants::AMPLITUDE_BINS) {
                    int &freq = m_frequencyTable[phaseIdx][binIdx];
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
        normalizedPos = std::max(0.0f, std::min(0.9999f, normalizedPos));
        int binIndex = static_cast<int>(normalizedPos * PRPDConstants::AMPLITUDE_BINS);

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

    void PRPDChart::mousePressEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton && m_selectionBoxEnabled && m_hasSelection) {
            QRectF screenRect = dataRectToScreen(m_selectionDataRect);
            m_activeHandle = hitTestHandle(event->pos(), screenRect);

            if (m_activeHandle != HandleType::None) {
                m_isDraggingHandle = true;
                m_dragStartPos = event->pos();
                m_dragStartRect = m_selectionDataRect;
                updateCursor(m_activeHandle);
                return;
            }
        }
        Coordinate2D::mousePressEvent(event);
    }

    void PRPDChart::mouseMoveEvent(QMouseEvent *event) {
        if (m_isDraggingHandle) {
            QVector3D dragStart = screenToWorld(m_dragStartPos);
            QVector3D dragCurrent = screenToWorld(QPointF(event->pos()));

            float deltaPhase = (dragCurrent.x() - dragStart.x()) / PRPDConstants::GL_AXIS_LENGTH *
                               (m_phaseMax - m_phaseMin);

            auto [displayMin, displayMax] = getCurrentRange();
            float deltaAmp = (dragCurrent.y() - dragStart.y()) / PRPDConstants::GL_AXIS_LENGTH *
                             (displayMax - displayMin);

            QRectF newRect = m_dragStartRect;

            switch (m_activeHandle) {
                case HandleType::TopLeft:
                    newRect.setTopLeft(QPointF(m_dragStartRect.left() + deltaPhase, m_dragStartRect.top() + deltaAmp));
                    break;
                case HandleType::TopRight:
                    newRect.setTopRight(QPointF(m_dragStartRect.right() + deltaPhase,
                                                m_dragStartRect.top() + deltaAmp));
                    break;
                case HandleType::BottomLeft:
                    newRect.setBottomLeft(
                        QPointF(m_dragStartRect.left() + deltaPhase, m_dragStartRect.bottom() + deltaAmp));
                    break;
                case HandleType::BottomRight:
                    newRect.setBottomRight(
                        QPointF(m_dragStartRect.right() + deltaPhase, m_dragStartRect.bottom() + deltaAmp));
                    break;
                case HandleType::Body:
                    newRect.translate(deltaPhase, deltaAmp);
                    break;
                default:
                    break;
            }

            newRect = newRect.normalized();

            if (newRect.width() >= MIN_SELECTION_SIZE && newRect.height() >= MIN_SELECTION_SIZE) {
                newRect = clampToDataBounds(newRect);
                m_selectionDataRect = newRect;
                update();
            }
        } else if (m_selectionBoxEnabled && m_hasSelection) {
            QRectF screenRect = dataRectToScreen(m_selectionDataRect);
            HandleType handle = hitTestHandle(event->pos(), screenRect);
            updateCursor(handle);
        }

        Coordinate2D::mouseMoveEvent(event);
    }

    void PRPDChart::mouseReleaseEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton && m_isDraggingHandle) {
            m_isDraggingHandle = false;
            m_activeHandle = HandleType::None;
            setCursor(Qt::ArrowCursor);

            auto points = getPointsInRegion(m_selectionDataRect);
            emit regionSelected(m_selectionDataRect, points);
        }
        Coordinate2D::mouseReleaseEvent(event);
    }

    void PRPDChart::keyPressEvent(QKeyEvent *event) {
        if (event->key() == Qt::Key_Escape) {
            if (m_hasSelection) {
                clearSelection();
            }
        }
        Coordinate2D::keyPressEvent(event);
    }

    void PRPDChart::paintEvent(QPaintEvent *event) {
        Coordinate2D::paintEvent(event);
    }

    void PRPDChart::enableSelectionBox(bool enabled) {
        m_selectionBoxEnabled = enabled;

        if (enabled) {
            auto [displayMin, displayMax] = getCurrentRange();
            m_selectionDataRect = QRectF(QPointF(m_phaseMin, displayMin), QPointF(m_phaseMax, displayMax));
            m_hasSelection = true;

            auto points = getPointsInRegion(m_selectionDataRect);
            emit regionSelected(m_selectionDataRect, points);
        } else {
            setCursor(Qt::ArrowCursor);
        }

        update();
    }

    void PRPDChart::setSelectionBox(const QRectF &dataRect) {
        if (m_selectionBoxEnabled) {
            m_selectionDataRect = clampToDataBounds(dataRect);
            m_hasSelection = true;

            auto points = getPointsInRegion(m_selectionDataRect);
            emit regionSelected(m_selectionDataRect, points);

            update();
        }
    }

    QRectF PRPDChart::getSelectionBox() const {
        return m_selectionDataRect;
    }

    PRPDChart::HandleType PRPDChart::hitTestHandle(const QPointF &screenPos, const QRectF &screenRect) const {
        float handleSize = 12.0f;

        QPointF handles[4] = {
            screenRect.topLeft(), screenRect.topRight(), screenRect.bottomLeft(),
            screenRect.bottomRight()
        };

        HandleType types[4] = {
            HandleType::TopLeft, HandleType::TopRight, HandleType::BottomLeft,
            HandleType::BottomRight
        };

        for (int i = 0; i < 4; ++i) {
            QRectF handleRect(handles[i].x() - handleSize / 2, handles[i].y() - handleSize / 2, handleSize, handleSize);
            if (handleRect.contains(screenPos)) {
                return types[i];
            }
        }

        if (screenRect.contains(screenPos)) {
            return HandleType::Body;
        }

        return HandleType::None;
    }

    void PRPDChart::updateCursor(HandleType handle) {
        switch (handle) {
            case HandleType::TopLeft:
            case HandleType::BottomRight:
                setCursor(Qt::SizeFDiagCursor);
                break;
            case HandleType::TopRight:
            case HandleType::BottomLeft:
                setCursor(Qt::SizeBDiagCursor);
                break;
            case HandleType::Body:
                setCursor(Qt::SizeAllCursor);
                break;
            default:
                setCursor(Qt::ArrowCursor);
                break;
        }
    }

    void PRPDChart::clearSelection() {
        m_hasSelection = false;
        m_selectionDataRect = QRectF();
        update();
    }

    std::vector<PRPDChart::PointInfo> PRPDChart::getPointsInRegion(const QRectF &dataRect) {
        std::vector<PointInfo> points;

        int phaseIdxMin = static_cast<int>((dataRect.left() - m_phaseMin) / (m_phaseMax - m_phaseMin) * m_phasePoints);
        int phaseIdxMax = static_cast<int>((dataRect.right() - m_phaseMin) / (m_phaseMax - m_phaseMin) * m_phasePoints);
        phaseIdxMin = std::clamp(phaseIdxMin, 0, m_phasePoints - 1);
        phaseIdxMax = std::clamp(phaseIdxMax, 0, m_phasePoints - 1);

        for (int phaseIdx = phaseIdxMin; phaseIdx <= phaseIdxMax; ++phaseIdx) {
            for (BinIndex binIdx = 0; binIdx < PRPDConstants::AMPLITUDE_BINS; ++binIdx) {
                int freq = m_frequencyTable[phaseIdx][binIdx];
                if (freq <= 0) continue;

                float phase = static_cast<float>(phaseIdx) * (m_phaseMax / m_phasePoints);
                float amplitude = getBinCenterAmplitude(binIdx);

                if (amplitude >= dataRect.bottom() && amplitude <= dataRect.top() && phase >= dataRect.left() &&
                    phase <= dataRect.right()) {
                    PointInfo info;
                    info.phase = phase;
                    info.amplitude = amplitude;
                    info.frequency = freq;
                    info.phaseIndex = phaseIdx;
                    info.binIndex = binIdx;
                    points.push_back(info);
                }
            }
        }

        return points;
    }

    QRectF PRPDChart::dataRectToScreen(const QRectF &dataRect) const {
        float glXMin = (dataRect.left() - m_phaseMin) / (m_phaseMax - m_phaseMin) * PRPDConstants::GL_AXIS_LENGTH;
        float glXMax = (dataRect.right() - m_phaseMin) / (m_phaseMax - m_phaseMin) * PRPDConstants::GL_AXIS_LENGTH;

        auto [displayMin, displayMax] = getCurrentRange();
        float glYMin = (dataRect.bottom() - displayMin) / (displayMax - displayMin) * PRPDConstants::GL_AXIS_LENGTH;
        float glYMax = (dataRect.top() - displayMin) / (displayMax - displayMin) * PRPDConstants::GL_AXIS_LENGTH;

        QPointF screenMin = worldToScreen(QVector3D(glXMin, glYMin, 0));
        QPointF screenMax = worldToScreen(QVector3D(glXMax, glYMax, 0));

        return QRectF(screenMin, screenMax).normalized();
    }

    QRectF PRPDChart::clampToDataBounds(const QRectF &rect) const {
        auto [displayMin, displayMax] = getCurrentRange();

        qreal left = std::clamp(rect.left(), static_cast<qreal>(m_phaseMin), static_cast<qreal>(m_phaseMax));
        qreal right = std::clamp(rect.right(), static_cast<qreal>(m_phaseMin), static_cast<qreal>(m_phaseMax));
        qreal bottom = std::clamp(rect.bottom(), static_cast<qreal>(displayMin), static_cast<qreal>(displayMax));
        qreal top = std::clamp(rect.top(), static_cast<qreal>(displayMin), static_cast<qreal>(displayMax));

        return QRectF(QPointF(left, bottom), QPointF(right, top));
    }

    std::pair<float, float> PRPDChart::screenToData(const QPoint &screenPos) {
        QVector3D worldPos = screenToWorld(QPointF(screenPos));
        float phase = (worldPos.x() / PRPDConstants::GL_AXIS_LENGTH) * (m_phaseMax - m_phaseMin) + m_phaseMin;

        auto [displayMin, displayMax] = getCurrentRange();
        float amplitude = (worldPos.y() / PRPDConstants::GL_AXIS_LENGTH) * (displayMax - displayMin) + displayMin;

        return {phase, amplitude};
    }

    std::tuple<bool, float, float, int> PRPDChart::findPointAt(const QPoint &screenPos) {
        auto [phase, amplitude] = screenToData(screenPos);

        int phaseIdx = static_cast<int>((phase - m_phaseMin) / (m_phaseMax - m_phaseMin) * m_phasePoints);
        if (phaseIdx < 0 || phaseIdx >= m_phasePoints) {
            return {false, 0, 0, 0};
        }

        BinIndex binIdx = getAmplitudeBinIndex(amplitude);
        if (binIdx >= PRPDConstants::AMPLITUDE_BINS) {
            return {false, 0, 0, 0};
        }

        int searchRadius = 2;
        int maxFreq = 0;
        int bestPhaseIdx = -1;
        BinIndex bestBinIdx = 0;

        for (int dp = -searchRadius; dp <= searchRadius; ++dp) {
            int p = phaseIdx + dp;
            if (p < 0 || p >= m_phasePoints) continue;

            for (int db = -searchRadius; db <= searchRadius; ++db) {
                int b = binIdx + db;
                if (b < 0 || b >= PRPDConstants::AMPLITUDE_BINS) continue;

                int freq = m_frequencyTable[p][b];
                if (freq > maxFreq) {
                    maxFreq = freq;
                    bestPhaseIdx = p;
                    bestBinIdx = b;
                }
            }
        }

        if (maxFreq > 0) {
            float resultPhase = static_cast<float>(bestPhaseIdx) * (m_phaseMax / m_phasePoints);
            float resultAmp = getBinCenterAmplitude(bestBinIdx);
            return {true, resultPhase, resultAmp, maxFreq};
        }

        return {false, 0, 0, 0};
    }
} // namespace ProGraphics
