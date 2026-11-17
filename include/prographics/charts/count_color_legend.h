#pragma once
#include "prographics/charts/offline_data.h"
#include <QWidget>

namespace ProGraphics {
/** 逻辑像素绘制的计数色标，可独立放入应用布局；显示 uint64 真值而非对数值。 */
class PROGRAPHICS_EXPORT CountColorLegend : public QWidget {
public:
    explicit CountColorLegend(QWidget* parent = nullptr);
    void setColorScale(const CountColorScale& scale);
    const CountColorScale& colorScale() const { return m_scale; }
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent*) override;
private:
    CountColorScale m_scale;
    std::vector<CountColorTick> m_ticks;
};
} // namespace ProGraphics
