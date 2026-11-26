# LineChart 使用说明

`ProGraphics::LineChart` 用于显示一条或多条折线，支持等间隔和非等间隔 X、独立的 X/Y 可视范围、平移、缩放、悬浮读数及双游标。

## 创建图表

```cpp
#include <prographics/charts/line/line_chart.h>

auto* chart = new ProGraphics::LineChart(parent);
layout->addWidget(chart);
chart->setAxisName('x', "时间", "μs");
chart->setAxisName('y', "幅值", "mV");
```

图表随绘图区宽高独立拉伸。轴、网格、背景和文字样式可使用继承自 `Coordinate2D` 的相应设置方法。

四边留白随刻度、标题和字体自动调整，无需按窗口尺寸设置边距。刻度和标题放在绘图区之外；空间不足时自动减少刻度或暂时隐藏放不下的文字。

刻度自动选择规整间隔，网格与刻度位置一致。同一轴统一小数位数；整数量级显示整数，小量级使用必要的小数位，极大或极小量级使用科学记数法。

## 提交多条序列

等间隔 X 由 `xStart + index * xStep` 确定：

```cpp
ProGraphics::LineChart::Series a;
a.id = "A";
a.color = QColor("#59d8ff");
a.xStart = 0;
a.xStep = 0.01;
a.y = {0, 0.2, 1.8, -0.1, 0};

ProGraphics::LineChart::Series b;
b.id = "B";
b.color = QColor("#f4c95d");
b.x = {0, 0.015, 0.04, 0.1};
b.y = {1, 1.3, 0.8, 1};

if (!chart->setSeries({a, b})) {
    // 检查数据格式，原有快照仍然保留。
}
```

`setSeries()` 一次替换全部序列，两种 X 格式可以混用。`x` 为空时采用等间隔格式；非空时采用显式 X，忽略 `xStart/xStep`。

- `id` 必须非空且在批次内唯一。
- 显式 X 与 Y 数量相同，X 按非递减顺序排列，允许重复 X。
- 等间隔格式的 `xStep` 必须为正。
- 所有坐标必须是有限数值。空序列和空批次合法。
- 无效批次整体返回 `false`，保留原有数据。
- 所有图表接口在 GUI 线程调用；数据可以在工作线程准备，再通过排队连接提交。

首次收到非空数据时自动适配。后续换帧保留当前视图，包括用户的平移、缩放或程序设置的范围。

每次提交保留完整原始采样数组，用于绘制与读数。需要逐帧显示所有输入帧时，可按示例在上一帧 `frameSwapped` 后提交下一帧，以显示完成通知衔接连续供数。

## 可见性、颜色与清空

```cpp
chart->setSeriesVisible("A", false);
chart->setSeriesColor("B", QColor("#a6e39d"));
chart->clear();
```

隐藏序列不参与显示、悬浮命中和全幅适配。颜色和可见性修改作用于当前快照；连续提交新快照时，应在新 `Series` 中保留所需的颜色和可见性。`clear()` 清空数据及游标，保留可视范围。

## 范围与交互

```cpp
chart->setViewRange({0, 20}, {-5, 5});
chart->setXRange({2, 8});
chart->setYRange({-1, 1});
chart->fitToData();                  // 显示所有可见序列
chart->fitToData(Qt::Horizontal);    // 仅适配 X
chart->setInteractionAxes(Qt::Horizontal); // 默认仅操作 X
```

范围要求 `max > min` 且端点、跨度均有限，无效设置返回 `false`。`viewRangeChanged` 通知可视范围变化。`mapToChart()` / `mapFromChart()` 在控件逻辑坐标与数据坐标之间转换，`plotRect()` 返回逻辑像素绘图区。

| 操作 | 行为 |
| --- | --- |
| 滚轮 | 以鼠标所在数据位置为锚点缩放 |
| 左键拖动 | 平移当前作用轴 |
| Shift + 滚轮 / 左键拖动 | 只改变 X |
| Ctrl + 滚轮 / 左键拖动 | 只改变 Y |
| 双击绘图区 / Home | 恢复所有可见数据的全幅 |
| Alt + 左键点击或拖动 | 放置或移动游标 A，需启用双游标 |
| Alt + 右键点击或拖动 | 放置或移动游标 B，需启用双游标 |
| Esc | 清除游标并结束当前拖动 |

默认操作双轴。`setInteractionAxes()` 可以选择双轴、仅 X 或仅 Y；Shift/Ctrl 临时覆盖该设置。绘图区外的滚轮交由外层控件处理。

## 悬浮读数与双游标

```cpp
chart->setHoverEnabled(true);       // 默认启用
chart->setCursorsEnabled(true);     // 默认关闭
chart->setCursor(0, "A", 2);        // A 游标定位到原始采样点
chart->setCursor(1, "B", 1);        // B 游标
auto a = chart->cursor(0);
auto b = chart->cursor(1);
if (a && b) {
    double deltaX = b->x - a->x;
    double deltaY = b->y - a->y;
}
```

悬浮读数仅在真实采样点附近显示，返回序列 ID、原始下标、X 和 Y，不对折线插值。`sampleAt(widgetPosition, radius)` 可直接查询命中点，半径使用逻辑像素，默认 8。

双游标吸附实际可见采样点，显示 A/B 的 X/Y 以及 `B − A` 的 ΔX/ΔY。换帧时按序列 ID 和原始下标更新读数；序列隐藏、移除或下标超出新数据时，相应读数暂时无效。`cursorsChanged` 通知游标或其读数变化。关闭双游标保留位置，`clearCursors()` 清除位置。

## 运行示例

编译 `ProGraphicsExamples` 后，在主窗口点击“折线图示例”打开弹窗，也可运行：

```text
ProGraphicsExamples.exe --line
```

示例提供八路各 2,048 点的随机噪声与随机窄尖峰波形，支持等间隔与非等间隔 X、静态查看和持续换帧。每次载入或供数都会重新生成随机波形。顶部可选择作用轴并控制悬浮、双游标和全幅，底部各路复选框直接控制可见性。

顶部布局选项可切换普通布局、宽且较矮的布局和大字号宽图。调节窗口大小、字体或可视范围后，坐标文字会重新安排空间。

勾选“持续供数”后连续提交完整波形。“生成速度上限”可在运行时调整，范围为 1～1,000 帧/秒，默认“不限速”；例如 5 帧/秒约每 200 ms 生成一帧，30 帧/秒约每 33.3 ms 生成一帧。

当前帧完成显示后才提交下一帧，全部采样点原样交给图表，显示中的帧不会被下一帧覆盖。实际速度同时受设定上限和绘制能力约束。“不限速”表示不额外等待，不保证超过显示器刷新率。切换数据格式或手动载入数据前先停止供数。
