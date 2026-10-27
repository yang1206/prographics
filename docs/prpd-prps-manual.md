# PRPD 和 PRPS 图表使用手册

本文介绍图表的接入、数据输入、量程配置与交互。库的构建与依赖接入见 [README](../README.md)。

## 目录

- [选择图表](#选择图表)
- [快速开始](#快速开始)
- [输入数据](#输入数据)
- [PRPD 图表](#prpd-图表)
- [PRPS 图表](#prps-图表)
- [量程配置](#量程配置)
- [暂停与恢复](#暂停与恢复)
- [参考线与刻度](#参考线与刻度)
- [示例与压力测试](#示例与压力测试)
- [常见问题](#常见问题)

## 选择图表

| 图表 | 观察内容 | 保留范围 | 颜色含义 |
|---|---|---|---|
| PRPD | 相位与幅值的统计分布 | 最近最多 500 个周期 | 同一相位、幅值格中的累计频次，蓝色低、红色高 |
| PRPS | 各周期的幅值及先后顺序 | 最多 80 个周期，旧周期随滚动移出 | 幅值在当前显示量程中的相对高度 |

PRPD 适合观察分布是否集中、哪些区域反复出现；PRPS 适合观察连续周期的变化。

## 快速开始

应用目标链接 `ProGraphics::ProGraphics`。下面的示例创建两张图，显示同一个周期，并提供暂停按钮；实际项目可将图表放入自己的布局。

```cpp
#include <QApplication>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSurfaceFormat>
#include <QVBoxLayout>
#include <QWidget>
#include <vector>
#include <prographics/charts/prpd/prpd.h>
#include <prographics/charts/prps/prps.h>

int main(int argc, char* argv[]) {
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QWidget window;
    auto* layout = new QVBoxLayout(&window);
    auto* charts = new QHBoxLayout;
    auto* prpd = new ProGraphics::PRPDChart(&window);
    auto* prps = new ProGraphics::PRPSChart(&window);
    charts->addWidget(prpd);
    charts->addWidget(prps);
    layout->addLayout(charts);

    prpd->setFixedRange(-75.0f, -30.0f);
    prps->setFixedRange(-75.0f, -30.0f);
    prps->setUpdateInterval(20);

    // 示例周期；实际接入时替换为采集数据，并持续调用 addCycleData。
    std::vector<float> cycle(200, -65.0f);
    cycle[50] = -40.0f;
    cycle[150] = -45.0f;
    prpd->addCycleData(cycle);
    prps->addCycleData(cycle);

    auto* pauseButton = new QPushButton("暂停", &window);
    layout->addWidget(pauseButton);
    QObject::connect(pauseButton, &QPushButton::clicked, &window,
        [prpd, prps, pauseButton] {
            if (prpd->isPaused()) {
                prpd->resume();
                prps->resume();
                pauseButton->setText("暂停");
            } else {
                prpd->pause();
                prps->pause();
                pauseButton->setText("继续");
            }
        });

    window.resize(1200, 600);
    window.show();
    return app.exec();
}
```

## 输入数据

所有图表的数据和配置方法都应在 GUI 线程调用。后台采集的数据应通过队列信号或其他 GUI 线程调度方式交给图表。

| API（两种图表通用） | 用法 |
|---|---|
| `addCycleData(const std::vector<float>& cycleData)` | 输入一个周期，数组按相位顺序存放幅值，长度必须等于配置的采样点数 |
| `setPhasePoint(int phasePoint)` | 设置每周期采样点数，默认 200，必须大于 0；改变点数会清空历史，同值设置不清空 |
| `setPhaseRange(float min, float max)` | 设置相位显示范围，默认 0～360° |
| `resetData()` | 清空历史数据并重置量程状态，保留暂停及接收开关 |

幅值单位由调用方约定，输入数据与量程、参考线必须使用相同单位。修改轴单位文字不会进行数值换算。

长度不匹配的周期会被拒绝。NaN 和 Inf 不参与幅值统计或绘制；全无效周期仍占用一个周期位置。需要表示无采样值时，可使用 NaN，不能假定某个有限底值会被自动忽略。

## PRPD 图表

PRPD 将当前幅值范围分为 100 个格子，按相位和幅值累计频次。超过 500 个周期后，最老周期退出统计；停止输入不会自动清除已有周期。

颜色使用每格频次与全图最大频次的比值。多个周期落在同一个幅值格，会形成高频红带；其他区域频次相对较低时仍可能呈蓝色。颜色表示相对频次，不直接表示幅值大小。

有限幅值低于量程下限时计入最底部的格子，高于上限时计入最顶部的格子。量程变化后，保留周期按新的范围重新分箱，因此分布外观和颜色可能随量程改变。需要跨时段比较分布时，使用一致的固定量程。

## PRPS 图表

每个输入周期形成一组脉冲竖线，新周期从后方加入，旧周期向前滚动并移出。最多保留 80 个周期，超出后移除最老周期。

| API | 用法 |
|---|---|
| `setUpdateInterval(int intervalMs)` | 设置动画更新间隔，默认 20ms，必须大于 0；改变间隔不改变滚动速度 |
| `setDisplayLineCount(int count)` | 设置每周期的显示竖线数，默认 50；少于采样点数时按相位分组并取组内最大幅值 |
| `displayLineCount() const` | 查询显示竖线数配置 |
| `setThreshold(float threshold)` | 当前不参与竖线过滤，不能用它设置有效幅值门限 |

`setDisplayLineCount(0)` 或设置为不小于采样点数的值时，按每个采样点绘制。位于量程下限及以下的值不生成竖线；高于上限的值显示到顶部。

隐藏图表会冻结已有周期的位置，再次显示后继续滚动，不补算隐藏期间的位移。隐藏不会自动拒绝输入；如需同时停止接收，调用 `pause()`。

## 量程配置

以下 API 两种图表均支持。示例中的 `RangeConfig` 是配置类型的简写：

```cpp
using RangeConfig = ProGraphics::DynamicRange::DynamicRangeConfig;
```

### 选择量程模式

| 模式与设置方法 | 行为 | 适用场景 |
|---|---|---|
| `setFixedRange(min, max)` | 始终使用指定范围 | 范围已知，或需要比较多张图 |
| `setAutoRange(config)` | 根据保留数据拟合范围，允许扩展和收缩 | 数据范围未知 |
| `setAdaptiveRange(initialMin, initialMax, config)` | 至少显示初始范围，需要时扩展，之后可恢复 | 有常用范围，又需要显示越界峰值 |

Auto 和 Adaptive 首次收到有效数据时拟合量程。之后出现越界峰值会立即扩展；数据回落后，只有收缩条件持续满足才缩小范围。Adaptive 的初始范围也受硬限制约束。

```cpp
RangeConfig config;
config.bufferRatio = 0.3f;       // 数据上方留出数据跨度的 30%
config.shrinkDelayMs = 3000;     // 收缩条件持续满足 3 秒
config.recoveryRangeRatio = 0.8f;
config.enableHardLimits = true;
config.hardLimitMin = -100.0f;
config.hardLimitMax = 0.0f;

prpd->setAdaptiveRange(-75.0f, -30.0f, config);
prps->setAdaptiveRange(-75.0f, -30.0f, config);
```

### 动态量程配置参数

| 参数 | 默认值 | 作用 |
|---|---|---|
| `bufferRatio` | 0.3 | 上方留白占数据跨度的比例 |
| `lowerBufferRatio` | 0 | 下方留白占数据跨度的比例 |
| `targetTickCount` | 6 | 期望的主刻度数量，实际数量可能不同 |
| `minimumSpan` | 1 | 自动量程的最小跨度，硬限制优先 |
| `sameValueRangeRatio` | 0.1 | 数据全部相同时，两侧扩展的比例，与最小跨度共同生效 |
| `shrinkDelayMs` | 3000 | 收缩条件需要持续满足的毫秒数；0 表示不等待 |
| `recoveryRangeRatio` | 0.8 | 候选跨度不超过当前跨度的该比例时，开始等待收缩 |
| `enableRangeRecovery` | true | 是否允许收缩；false 时首次拟合后只扩展 |
| `enableHardLimits` | false | 是否限制自动量程边界 |
| `hardLimitMin / hardLimitMax` | -1000 / 1000 | 自动量程允许的最小、最大边界 |

`responseSpeed`、`smartAdjustment`、`recoveryFrameThreshold` 为兼容字段，设置它们不会改变当前量程行为。调整收缩时使用 `shrinkDelayMs`、`recoveryRangeRatio` 和 `enableRangeRecovery`。

### 查询与运行时调整

| API | 用法 |
|---|---|
| `getRangeMode() const` | 查询 Fixed、Auto 或 Adaptive 模式 |
| `getCurrentRange() const` | 获取实际显示的最小值、最大值 |
| `getConfiguredRange() const` | 获取配置范围；Auto 为进入该模式时的起始范围，不代表当前显示范围 |
| `updateAutoRangeConfig(const RangeConfig& config)` | 更新 Auto 或 Adaptive 配置；Fixed 下不生效 |
| `switchToFixedRange(float min, float max)` | 切换为指定的固定范围 |
| `switchToAutoRange()` | 沿用已有动态量程配置，切换为 Auto |
| `rangeChanged(float min, float max)`（信号） | 实际显示端点改变时发出，可用于同步外部读数 |

### 硬限制

硬限制只约束 Auto 和 Adaptive 的显示范围，不修改输入值，也不自动丢弃越界数据。Fixed 使用显式设置的范围。

```cpp
prpd->setHardLimits(-100.0f, 0.0f);
prps->setHardLimits(-100.0f, 0.0f);
```

使用 `getHardLimits()` 查询边界，`enableHardLimits(bool)` 切换开关，`isHardLimitsEnabled()` 查询是否启用。边界应为有限数且最小值小于最大值。

调用 `setAutoRange(config)`、`setAdaptiveRange(..., config)` 或 `updateAutoRangeConfig(config)` 会应用整份配置，包括硬限制。需要保留硬限制时，应在传入配置中一并设置。

## 暂停与恢复

| 调用 | PRPD | PRPS |
|---|---|---|
| `pause()` 或 `pause(true)` | 停止接收，统计分布冻结 | 停止接收和滚动，位置冻结 |
| `pause(false)` | 仍接收并更新统计，只改变暂停标记 | 停止滚动，仍接收新周期；新周期叠加在末端，超过容量仍移除最老周期 |
| `resume()` | 恢复接收 | 恢复接收和滚动 |
| `togglePause()` | 在默认暂停与恢复之间切换 | 在默认暂停与恢复之间切换 |

`isPaused()` 查询暂停状态。`setAcceptData(bool)` 可独立控制是否接收，`isAcceptingData()` 查询接收开关；`resume()` 会重新开启接收。

拒绝接收期间提交的数据会被丢弃，不会在恢复后补入。两张图可分别暂停，互不影响。

## 参考线与刻度

参考线幅值与输入数据使用相同单位，超出当前显示范围时隐藏。

```cpp
ProGraphics::AmplitudeLine pdLine;
pdLine.amplitudeDbm = -50.0f;
pdLine.color = QVector4D(1.0f, 0.0f, 0.0f, 1.0f);
pdLine.lineWidth = 2.0f;
prpd->setAmplitudeLines({pdLine});

ProGraphics::PhaseAmplitudePlaneLine psLine;
psLine.amplitudeDbm = -50.0f;
psLine.color = pdLine.color;
psLine.lineWidth = 2.0f;
prps->setPhaseAmplitudePlaneLines({psLine});
```

上述设置会替换整份参考线列表。分别调用 `clearAmplitudeLines()` 和 `clearPhaseAmplitudePlaneLines()` 清除。

PRPD 支持拖动参考线：

```cpp
prpd->setAmplitudeReferenceLinesDraggable(true);
prpd->setAmplitudeReferenceLineDragSnapStep(1.0f);
prpd->setAmplitudeReferenceLinePickTolerancePx(6.0f);
```

可监听 `amplitudeReferenceLineDragStarted`、`amplitudeReferenceLineDragged` 和 `amplitudeReferenceLineDragEnded`，信号参数均为参考线索引及幅值。拖动 PRPD 参考线不会自动更新 PRPS，联动由调用方处理。

两种图表均可设置轴名称及刻度格式：

```cpp
prpd->setAxisName('y', "幅值", "dBm");
prpd->setTicksFormatter('y', [](float value) {
    return QString::number(value, 'f', 1);
});
```

默认刻度格式根据步长选择小数位数，至少保留一位。自定义格式优先；显示小范围数据时，应保留足够的小数位以区分相邻刻度。

## 示例与压力测试

启用 `PROGRAPHICS_BUILD_EXAMPLES` 后可构建 `ProGraphicsExamples`。示例提供数据生成、量程切换、参考线及暂停控制。

在示例左侧选择 **调试工具 → 性能压力测试**，设置通道数、相位点数、周期速率和测试时长，开始测试后可导出 CSV。比较性能时使用 Release，并保持设备、窗口尺寸、屏幕缩放和负载一致。

也可以从可执行文件所在目录运行：

```powershell
.\ProGraphicsExamples.exe --stress
.\ProGraphicsExamples.exe --stress-auto daily.csv --stress-preset daily --stress-warmup 10 --stress-duration 60
```

预设支持 `daily`（日常负载）、`dense`（稠密高压）、`steady`（重复周期）和 `range`（幅值变化）。CSV 父目录需已存在；自动运行正常完成并保存报告返回 0，参数错误、取消或保存失败返回 2。

阅读报告时关注：

- 实际周期速率与最终缺口：是否能处理目标输入负载。
- 更新、绘制耗时和界面延迟的 P95/P99：高负载下是否出现明显卡顿。
- CPU、内存及各绘制层耗时：资源占用是否符合应用要求。

绘制耗时是 CPU 提交耗时，不代表 GPU 完成时间；FPS 是图表绘制调用率。测试完成或返回 0 只表示报告生成成功，不表示性能达标。

## 常见问题

### 为什么数据已经降低，量程还没收缩？

量程考虑所有保留周期。PRPD 中的历史峰值需要随新周期输入退出最近 500 个周期；PRPS 中的峰值需要滚出或被新周期替换。峰值退出后，还要满足跨度比例和等待时间条件，默认是候选跨度不超过当前的 80%，持续 3 秒。

PRPD 停止输入后不再推进统计窗口和量程更新。需要立即清空历史时调用 `resetData()`；需要指定显示范围时调用 `setFixedRange()`。

### 为什么 PRPD 底部有一条红带？

多个周期在同一个低幅值格内累积会形成高频红带。有限底值及低于量程下限的数据也会计入底部格子。先检查输入含义和量程，不能仅凭红带判断数据异常。

### 相位采样点数必须是 200 吗？

不是。先调用 `setPhasePoint(N)`，再输入长度为 N 的周期。改变点数会清空历史，零和负数不接受。PRPD 的幅值分箱数固定为 100，不随相位点数改变。

### PRPS 没显示所有采样点，是数据丢了吗？

默认每周期按相位分成 50 组，显示各组最大幅值。需要逐采样点显示时调用 `setDisplayLineCount(0)`。位于量程下限及以下的值仍不绘制竖线。

## 许可证

本文档与 ProGraphics 库一样，基于 LGPL-3.0 许可证。
