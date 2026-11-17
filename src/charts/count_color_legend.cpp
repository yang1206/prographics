#include "prographics/charts/count_color_legend.h"
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <algorithm>

namespace ProGraphics {
CountColorLegend::CountColorLegend(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    m_ticks = m_scale.ticks();
}
void CountColorLegend::setColorScale(const CountColorScale& scale) {
    m_scale = scale;
    m_ticks = m_scale.ticks();
    updateGeometry();
    update();
}
QSize CountColorLegend::sizeHint() const {
    const QFontMetrics fm(font());
    int textWidth = fm.horizontalAdvance(m_scale.label());
    for (const auto& tick : m_ticks) textWidth = std::max(textWidth,
        fm.horizontalAdvance(QString::number(static_cast<qulonglong>(tick.count))));
    return QSize(textWidth + 46, std::max(180, fm.height() * 11));
}
void CountColorLegend::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 38, 52, 230));
    painter.setPen(Qt::white);
    const QFontMetrics fm(font());
    painter.drawText(QRect(6, 4, width() - 12, fm.height()), Qt::AlignLeft, m_scale.label());
    const QRectF bar(8, fm.height() + 12, 14, std::max(1, height() - fm.height() * 2 - 24));
    QLinearGradient gradient(bar.bottomLeft(), bar.topLeft());
    // 色带与点使用完全相同的映射；不对原始网格做平滑。
    for (int i = 0; i <= 128; ++i) {
        QColor color = m_scale.colorAt(i / 128.0);
        gradient.setColorAt(i / 128.0, color);
    }
    painter.fillRect(bar, gradient);
    for (const auto& tick : m_ticks) {
        const double y = bar.bottom() - tick.position * bar.height();
        painter.drawLine(QPointF(bar.right(), y), QPointF(bar.right() + 4, y));
        painter.drawText(QRectF(bar.right() + 7, y - fm.height() / 2.0,
            width() - bar.right() - 9, fm.height()), Qt::AlignLeft | Qt::AlignVCenter,
            QString::number(static_cast<qulonglong>(tick.count)));
    }
}
} // namespace ProGraphics
