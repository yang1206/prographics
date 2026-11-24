#include "prographics/charts/coordinate/coordinate2d.h"

#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
#include <QToolTip>
#include <algorithm>
#include <cmath>

namespace ProGraphics {
  namespace {
    constexpr double minDragPixels = 3.0;
    constexpr double minLassoArea = 4.0;

    bool isFinite(const QPointF& point) {
      return std::isfinite(point.x()) && std::isfinite(point.y());
    }

    bool isFiniteRect(const QRectF& rect) {
      return rect.isValid() && isFinite(rect.topLeft()) && isFinite(rect.bottomRight()) &&
             std::isfinite(rect.width()) && std::isfinite(rect.height());
    }

    QPointF toData(const QPointF& point, const QRectF& plot, const QRectF& data) {
      return {
        std::lerp(data.left(), data.right(), (point.x() - plot.left()) / plot.width()),
        std::lerp(data.top(), data.bottom(), (plot.bottom() - point.y()) / plot.height())
      };
    }

    QPointF toWidget(const QPointF& point, const QRectF& plot, const QRectF& data) {
      return {
        std::lerp(plot.left(), plot.right(), (point.x() - data.left()) / data.width()),
        std::lerp(plot.bottom(), plot.top(), (point.y() - data.top()) / data.height())
      };
    }

    QPainterPath lassoPath(const QPolygonF& points) {
      QPainterPath path;
      path.setFillRule(Qt::OddEvenFill);
      path.addPolygon(points);
      if (points.size() > 2) path.closeSubpath();
      return path;
    }

    double lassoArea(const QPolygonF& points) {
      double area = 0.0;
      // 与显示一致，按奇偶规则解析交叉轮廓，再计算各子路径面积。
      const auto outlines = lassoPath(points).simplified().toSubpathPolygons();
      for (const auto& outline : outlines) {
        double twiceArea = 0.0;
        for (qsizetype i = 1; i < outline.size(); ++i) {
          const auto& a = outline[i - 1];
          const auto& b = outline[i];
          twiceArea += a.x() * b.y() - b.x() * a.y();
        }
        area += std::abs(twiceArea) * 0.5;
      }
      return area;
    }

    QPainterPath selectionPath(Coordinate2D::SelectionMode mode, const QPolygonF& points) {
      if (points.size() < 2) return {};
      if (mode == Coordinate2D::SelectionMode::Polygon) return lassoPath(points);
      QPainterPath path;
      path.addRect(QRectF(points.first(), points.last()).normalized());
      return path;
    }
  } // namespace

  // 普通的栅格子控件由 Qt 与现有 GL 纹理合成，拖动时仅使覆盖层局部失效。
  class SelectionOverlay final : public QWidget {
  public:
    explicit SelectionOverlay(QWidget* parent) : QWidget(parent) {
      setAttribute(Qt::WA_TransparentForMouseEvents);
      setFocusPolicy(Qt::NoFocus);
    }

    bool setPlotRect(const QRectF& plot) {
      if (plot == m_plot) return false;
      const auto oldBounds = QRegion(damage(m_completed)).united(damage(m_draft));
      m_plot = plot;
      update(oldBounds.united(damage(m_completed)).united(damage(m_draft)));
      return true;
    }

    void setDraft(QPainterPath path) { setPath(m_draft, std::move(path)); }
    void setCompleted(QPainterPath path) { setPath(m_completed, std::move(path)); }

  protected:
    void paintEvent(QPaintEvent*) override {
      QPainter painter(this);
      painter.setRenderHint(QPainter::Antialiasing);
      painter.setClipRect(m_plot);
      if (!m_completed.isEmpty()) {
        painter.setPen(QPen(QColor(104, 197, 255), 1.5));
        painter.setBrush(QColor(104, 197, 255, 42));
        painter.drawPath(m_completed);
      }
      if (!m_draft.isEmpty()) {
        painter.setPen(QPen(QColor(235, 248, 255), 1.5, Qt::DashLine));
        painter.setBrush(QColor(104, 197, 255, 54));
        painter.drawPath(m_draft);
      }
    }

  private:
    QRect damage(const QPainterPath& path) const {
      if (path.isEmpty()) return {};
      // 留出轮廓和抗锯齿覆盖的逻辑像素。
      return path.controlPointRect().adjusted(-2, -2, 2, 2)
        .intersected(m_plot).intersected(QRectF(rect())).toAlignedRect();
    }

    void setPath(QPainterPath& current, QPainterPath next) {
      if (current == next) return;
      const auto dirty = QRegion(damage(current)).united(damage(next));
      current = std::move(next);
      update(dirty);
    }

    QRectF m_plot;
    QPainterPath m_completed, m_draft;
  };

  QRectF Coordinate2D::chartDataRect() const {
    const auto& x = m_config.ticks.x.range;
    const auto& y = m_config.ticks.y.range;
    return QRectF(QPointF(x.min, y.min), QPointF(x.max, y.max));
  }

  QRectF Coordinate2D::plotRect() const {
    if (width() <= 0 || height() <= 0 || !std::isfinite(m_config.size) || m_config.size <= 0) return {};
    const auto viewProjection = m_camera.getProjectionMatrix() * m_camera.getViewMatrix();
    const auto project = [&](const QVector4D& point) -> QPointF {
      const auto clip = viewProjection * point;
      const double w = clip.w();
      if (w <= 0.0) return {};
      return {(1.0 + clip.x() / w) * width() * 0.5, (1.0 - clip.y() / w) * height() * 0.5};
    };
    // Coordinate2D 的相机始终以轴对齐的正交投影观察 z=0 平面。
    const auto lowerLeft = project(QVector4D(0, 0, 0, 1));
    const auto upperRight = project(QVector4D(m_config.size, m_config.size, 0, 1));
    const QRectF plot(QPointF(lowerLeft.x(), upperRight.y()), QPointF(upperRight.x(), lowerLeft.y()));
    return isFiniteRect(plot) ? plot : QRectF{};
  }

  std::optional<QPointF> Coordinate2D::mapToChart(const QPointF& widgetPosition) const {
    const auto plot = plotRect();
    const auto data = chartDataRect();
    if (!plot.isValid() || !isFiniteRect(data) || !isFinite(widgetPosition)) return {};
    const auto point = toData(widgetPosition, plot, data);
    if (!isFinite(point)) return {};
    return point;
  }

  std::optional<QPointF> Coordinate2D::mapFromChart(const QPointF& chartPosition) const {
    const auto plot = plotRect();
    const auto data = chartDataRect();
    if (!plot.isValid() || !isFiniteRect(data) || !isFinite(chartPosition)) return {};
    const auto point = toWidget(chartPosition, plot, data);
    if (!isFinite(point)) return {};
    return point;
  }

  QRectF Coordinate2D::selectionPlotRect() const {
    return plotRect().intersected(QRectF(rect()));
  }

  bool Coordinate2D::canStartSelectionAt(const QPointF&) const {
    return true;
  }

  void Coordinate2D::setSelectionMode(SelectionMode mode) {
    if (m_selectionMode == mode) return;
    m_selectionMode = mode;
    if (mode != SelectionMode::Disabled && !m_selectionOverlay) {
      // 提前创建覆盖层，避免第一次按下时的控件显示刷新。
      m_selectionOverlay = new SelectionOverlay(this);
      m_selectionOverlay->setGeometry(rect());
      m_selectionOverlay->setPlotRect(selectionPlotRect());
      m_selectionOverlay->lower();
      m_selectionOverlay->show();
    }
    cancelSelection();
  }

  bool Coordinate2D::startSelection(const QPointF& widgetPosition) {
    const auto plot = selectionPlotRect();
    if (!plot.isValid() || !plot.contains(widgetPosition) || !isFiniteRect(chartDataRect()) ||
        !canStartSelectionAt(widgetPosition)) return false;
    m_selectionOverlay->setPlotRect(plot);
    m_selectionPoints = {widgetPosition};
    m_selectionPressClaimed = true;
    QToolTip::hideText();
    setFocus(Qt::MouseFocusReason);
    return true;
  }

  bool Coordinate2D::appendSelectionPoint(const QPointF& widgetPosition, bool last) {
    const auto plot = selectionPlotRect();
    if (!plot.isValid() || !isFinite(widgetPosition)) return false;
    const QPointF point(std::clamp(widgetPosition.x(), plot.left(), plot.right()),
                        std::clamp(widgetPosition.y(), plot.top(), plot.bottom()));
    if (m_selectionPoints.last() == point) return false;
    if (m_selectionMode == SelectionMode::Rectangle && m_selectionPoints.size() == 2) {
      m_selectionPoints.last() = point;
    } else {
      if (m_selectionMode == SelectionMode::Polygon && !last &&
          QLineF(m_selectionPoints.last(), point).length() < minDragPixels) return false;
      m_selectionPoints.append(point);
    }
    return true;
  }

  std::optional<Coordinate2D::SelectionRegion> Coordinate2D::draftSelection() const {
    const auto plot = plotRect();
    const auto data = chartDataRect();
    if (m_selectionPoints.size() < 2 || !plot.isValid() || !isFiniteRect(data)) return {};
    if (m_selectionMode == SelectionMode::Rectangle) {
      const QRectF drag(m_selectionPoints.first(), m_selectionPoints.last());
      if (std::abs(drag.width()) < minDragPixels || std::abs(drag.height()) < minDragPixels) return {};
      const QRectF bounds(toData(m_selectionPoints.first(), plot, data),
                          toData(m_selectionPoints.last(), plot, data));
      if (!bounds.normalized().isValid()) return {};
      return bounds.normalized();
    }

    auto points = m_selectionPoints;
    if (points.last() == points.first()) points.removeLast();
    if (points.size() < 3 || lassoArea(points) < minLassoArea) return {};
    for (auto& point : points) point = toData(point, plot, data);
    return points;
  }

  void Coordinate2D::refreshSelectionProjection() {
    if (!m_selectionOverlay) return;
    const auto plot = plotRect();
    const bool layoutChanged = m_selectionOverlay->setPlotRect(plot.intersected(QRectF(rect())));

    QPainterPath path;
    const auto data = chartDataRect();
    if (m_selection && plot.isValid() && isFiniteRect(data)) {
      if (const auto* bounds = std::get_if<QRectF>(&*m_selection)) {
        const QRectF projected(toWidget(bounds->topLeft(), plot, data),
                               toWidget(bounds->bottomRight(), plot, data));
        if (isFinite(projected.topLeft()) && isFinite(projected.bottomRight()))
          path.addRect(projected.normalized());
      } else {
        auto points = std::get<QPolygonF>(*m_selection);
        for (auto& point : points) point = toWidget(point, plot, data);
        if (std::all_of(points.cbegin(), points.cend(), isFinite)) path = lassoPath(points);
      }
    }
    m_selectionOverlay->setCompleted(std::move(path));
    if (layoutChanged) cancelSelection();
  }

  bool Coordinate2D::finishSelection() {
    if (m_selectionPoints.isEmpty()) return false;
    const auto region = draftSelection();
    if (!region) {
      cancelSelection();
      return false;
    }

    m_selectionPoints.clear();
    m_selectionOverlay->setDraft({});
    m_selection = region;
    refreshSelectionProjection();
    emit selectionFinished(*region);
    return true;
  }

  bool Coordinate2D::cancelSelection() {
    if (m_selectionPoints.isEmpty()) return false;
    m_selectionPoints.clear();
    m_selectionOverlay->setDraft({});
    emit selectionCanceled();
    return true;
  }

  void Coordinate2D::clearSelection() {
    const bool hadDraft = !m_selectionPoints.isEmpty();
    const bool hadSelection = m_selection.has_value();
    m_selectionPoints.clear();
    m_selection.reset();
    if (m_selectionOverlay) {
      m_selectionOverlay->setDraft({});
      m_selectionOverlay->setCompleted({});
    }
    if (hadDraft) emit selectionCanceled();
    if (hadSelection) emit selectionCleared();
  }

  bool Coordinate2D::handleSelectionEvent(QEvent* event) {
    switch (event->type()) {
      case QEvent::MouseButtonPress:
      case QEvent::MouseButtonDblClick: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton) return false;
        if (!m_selectionPoints.isEmpty()) return true;
        // The preceding release may have gone to another widget after focus loss.
        m_selectionPressClaimed = false;
        return m_selectionMode != SelectionMode::Disabled && mouse->modifiers() == Qt::NoModifier &&
          (mouse->buttons() & ~Qt::LeftButton) == Qt::NoButton && startSelection(mouse->position());
      }
      case QEvent::MouseMove: {
        if (!m_selectionPressClaimed) return false;
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (!(mouse->buttons() & Qt::LeftButton)) {
          m_selectionPressClaimed = false;
          cancelSelection();
          return false;
        }
        if (!m_selectionPoints.isEmpty() && appendSelectionPoint(mouse->position(), false))
          m_selectionOverlay->setDraft(selectionPath(m_selectionMode, m_selectionPoints));
        return true;
      }
      case QEvent::MouseButtonRelease: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (!m_selectionPressClaimed || mouse->button() != Qt::LeftButton) return false;
        m_selectionPressClaimed = false;
        if (!m_selectionPoints.isEmpty()) {
          appendSelectionPoint(mouse->position(), true);
          finishSelection();
        }
        return true;
      }
      case QEvent::ShortcutOverride:
      case QEvent::KeyPress:
        if (!m_selectionPoints.isEmpty() && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
          if (event->type() == QEvent::KeyPress) cancelSelection();
          return true;
        }
        return false;
      case QEvent::ToolTip:
        return !m_selectionPoints.isEmpty();
      case QEvent::Hide:
      case QEvent::UngrabMouse:
        m_selectionPressClaimed = false;
        cancelSelection();
        return false;
      case QEvent::FocusOut:
      case QEvent::WindowDeactivate:
        cancelSelection();
        return false;
      default:
        return false;
    }
  }

  bool Coordinate2D::event(QEvent* event) {
    if ((m_selectionMode != SelectionMode::Disabled || m_selectionPressClaimed) &&
        handleSelectionEvent(event)) {
      event->accept();
      return true;
    }
    return BaseGLWidget::event(event);
  }

  void Coordinate2D::focusInEvent(QFocusEvent* event) {
    // 选区只借用键盘焦点以接收 Esc，不改变 GL 层的外观。
    if (!m_selectionPoints.isEmpty() && event->reason() == Qt::MouseFocusReason) return;
    BaseGLWidget::focusInEvent(event);
  }

  void Coordinate2D::resizeEvent(QResizeEvent* event) {
    setupCamera();
    if (m_selectionOverlay) {
      m_selectionOverlay->setGeometry(rect());
      refreshSelectionProjection();
      cancelSelection();
    }
    BaseGLWidget::resizeEvent(event);
  }
} // namespace ProGraphics
