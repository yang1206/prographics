#pragma once
#include "prographics/charts/coordinate/coordinate2d.h"
#include <QColor>
#include <QVector>
#include <memory>

namespace ProGraphics {
class LineChartOverlay;

/// 通用多序列折线图。数据和交互接口均在控件所属线程调用。
class PROGRAPHICS_EXPORT LineChart : public Coordinate2D {
    Q_OBJECT
public:
    struct Range { double min = 0, max = 1; };
    struct Series {
        QString id;                         ///< 非空且在批次内唯一；用于颜色、可见性和游标。
        QString name;
        QColor color{80, 200, 255};
        bool visible = true;
        double xStart = 0, xStep = 1;        ///< x 为空时使用等间隔 X，步长必须为正。
        QVector<double> x;                  ///< 非空时必须与 y 等长、按 X 非递减排序。
        QVector<double> y;                  ///< 所有 X/Y 必须有限；空序列合法。
    };
    struct Sample {
        QString seriesId;
        qsizetype index = 0;
        double x = 0, y = 0;
    };
    explicit LineChart(QWidget* parent = nullptr);
    ~LineChart() override;

    /// 原子替换整个多序列快照；无效批次返回 false 并保留原数据。
    /// 首次非空数据自动适配；后续换帧保留可视范围。Qt 容器按需共享。
    bool setSeries(QVector<Series> series);
    const QVector<Series>& series() const { return m_series; }
    void clear();
    bool setSeriesVisible(const QString& id, bool visible);
    bool setSeriesColor(const QString& id, const QColor& color);

    Range xRange() const { return m_x; }
    Range yRange() const { return m_y; }
    bool setViewRange(Range x, Range y);
    bool setXRange(Range range) { return setViewRange(range, m_y); }
    bool setYRange(Range range) { return setViewRange(m_x, range); }
    void fitToData(Qt::Orientations axes = Qt::Horizontal | Qt::Vertical);
    /// 滚轮和拖动的默认作用轴；Shift 临时限定 X，Ctrl 临时限定 Y。
    void setInteractionAxes(Qt::Orientations axes) { m_axes = axes; }
    Qt::Orientations interactionAxes() const { return m_axes; }

    void setHoverEnabled(bool enabled);
    bool isHoverEnabled() const { return m_hoverEnabled; }
    /// 仅命中 radius 逻辑像素内的真实采样点；不对折线插值。
    std::optional<Sample> sampleAt(QPointF widgetPosition, double radius = 8) const;
    void setCursorsEnabled(bool enabled);
    bool areCursorsEnabled() const { return m_cursorsEnabled; }
    bool setCursor(int cursor, const QString& seriesId, qsizetype sampleIndex);
    std::optional<Sample> cursor(int cursor) const;
    void clearCursors();
    /// 用于诊断数据上传与视图交互；上下文重建也计为一次上传。
    quint64 dataUploadCount() const { return m_uploads; }

signals:
    void viewRangeChanged();
    void cursorsChanged();

protected:
    QRectF chartDataRect() const override;
    void initializeGLObjects() override;
    void paintGLObjects() override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    friend class LineChartOverlay;
    struct Gpu;
    struct Cursor { QString id; qsizetype index = -1; };
    static bool valid(Range range);
    static double xAt(const Series& series, qsizetype index);
    const Series* findSeries(const QString& id) const;
    std::optional<Sample> nearestSample(QPointF position, double radius) const;
    Qt::Orientations axesFor(Qt::KeyboardModifiers modifiers) const;
    void updateTicks();
    void updateOverlay();
    void cleanupGL();
    void moveCursor(int cursor, QPointF position);

    QVector<Series> m_series;
    Range m_x, m_y, m_dataX, m_dataY;
    bool m_initialFit = true, m_dirty = true;
    bool m_hoverEnabled = true, m_cursorsEnabled = false;
    bool m_panning = false;
    int m_dragCursor = -1;
    QPointF m_pressPosition;
    Range m_pressX, m_pressY;
    Qt::Orientations m_dragAxes, m_axes = Qt::Horizontal | Qt::Vertical;
    Cursor m_cursors[2];
    std::optional<Sample> m_hover;
    std::optional<QPointF> m_hoverPosition;
    LineChartOverlay* m_overlay = nullptr;
    std::unique_ptr<Gpu> m_gpu;
    QMetaObject::Connection m_contextConnection;
    quint64 m_uploads = 0;
};
} // namespace ProGraphics
