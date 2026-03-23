# PRPD 和 PRPS 图表使用手册

本文档详细介绍 ProGraphics 库中 **PRPD (Phase Resolved Partial Discharge)** 和 **PRPS (Phase Resolved Pulse Sequence)** 两种局部放电可视化图表的使用方法。

## 目录

- [概述](#概述)
- [PRPD 图表](#prpd-图表)
  - [工作原理](#prpd-工作原理)
  - [API 参考](#prpd-api-参考)
  - [量程模式](#prpd-量程模式)
  - [配置参数](#prpd-配置参数)
  - [暂停/恢复](#prpd-暂停恢复)
- [PRPS 图表](#prps-图表)
  - [工作原理](#prps-工作原理)
  - [API 参考](#prps-api-参考)
  - [量程模式](#prps-量程模式)
  - [配置参数](#prps-配置参数)
  - [暂停/恢复](#prps-暂停恢复)
- [通用配置](#通用配置)
  - [硬限制](#硬限制)
  - [动态量程算法说明](#动态量程算法说明)
- [使用示例](#使用示例)
- [常见问题](#常见问题)

---

## 概述

ProGraphics 提供两种专门用于局部放电 (Partial Discharge) 数据可视化的图表：

| 图表 | 维度 | 用途 | 数据处理方式 |
|------|------|------|-------------|
| **PRPD** | 2D (相位 × 幅值) + 颜色频次 | 统计分布观察 | 累积统计，环形缓冲 |
| **PRPS** | 3D (相位 × 幅值 × 时间) | 时序演变观察 | 滚动更新，Z轴推移 |

---

## PRPD 图表

### PRPD 工作原理

PRPD (Phase Resolved Partial Discharge) 是**局部放电相位分布图**，用于显示放电信号在相位-幅值平面上的统计分布。

**工作流程：**
1. 每个周期数据包含 `PHASE_POINTS` (默认 200) 个幅值采样点
2. 将幅值范围划分为 `AMPLITUDE_BINS` (默认 100) 个格子
3. 每个采样点根据相位和幅值落在对应的格子中，频次计数加 1
4. 使用颜色表示频次，颜色从蓝 (低频次) 到红 (高频次) 渐变
5. 环形缓冲最多保存 `MAX_CYCLES` (默认 500) 个周期，满了之后覆盖最老的数据

### PRPD 常量

```cpp
struct PRPDConstants {
    static constexpr float GL_AXIS_LENGTH = 5.0f;    ///< OpenGL 坐标轴长度
    static constexpr float POINT_SIZE = 8.0f;         ///< 点渲染大小
    static constexpr int PHASE_POINTS = 200;          ///< 相位采样点数
    static constexpr int MAX_CYCLES = 500;            ///< 最大周期缓存数
    static constexpr float PHASE_MAX = 360.0f;        ///< 相位最大值
    static constexpr float PHASE_MIN = 0.0f;          ///< 相位最小值
    static constexpr int AMPLITUDE_BINS = 100;        ///< 幅值划分格子数
};
```

### PRPD API 参考

#### 量程设置

| API | 说明 |
|-----|------|
| `void setFixedRange(float min, float max)` | 设置固定量程，范围不随数据变化 |
| `void setAutoRange(const DynamicRangeConfig &config = {})` | 设置自动量程，完全根据数据动态调整 |
| `void setAdaptiveRange(float initialMin, float initialMax, const DynamicRangeConfig &config = {})` | 设置自适应量程，在初始范围基础上智能调整 |

#### 量程查询

| API | 说明 |
|-----|------|
| `RangeMode getRangeMode() const` | 获取当前量程模式 |
| `std::pair<float, float> getCurrentRange() const` | 获取当前实际显示范围 |
| `std::pair<float, float> getConfiguredRange() const` | 获取配置的范围（用于 UI 显示） |

#### 运行时调整

| API | 说明 |
|-----|------|
| `void updateAutoRangeConfig(const DynamicRangeConfig &config)` | 更新动态量程配置（仅 Auto/Adaptive 模式有效）|
| `void switchToFixedRange(float min, float max)` | 切换到固定量程 |
| `void switchToAutoRange()` | 切换到自动量程 |

#### 硬限制

| API | 说明 |
|-----|------|
| `void setHardLimits(float min, float max, bool enabled = true)` | 设置硬限制范围，防止异常数据导致量程过大 |
| `std::pair<float, float> getHardLimits() const` | 获取硬限制范围 |
| `void enableHardLimits(bool enabled)` | 启用/禁用硬限制 |
| `bool isHardLimitsEnabled() const` | 检查硬限制是否启用 |

#### 数据接口

| API | 说明 |
|-----|------|
| `void addCycleData(const std::vector<float> &cycleData)` | 添加一个周期的放电数据，长度必须等于相位采样点数 |
| `void setPhaseRange(float min, float max)` | 设置相位范围，默认 0-360° |
| `void setPhasePoint(int phasePoint)` | 设置相位采样点数，默认 200 |
| `void resetData()` | 清空所有数据，重置到初始状态 |

### PRPD 量程模式

| 模式 | 说明 | 适用场景 |
|------|------|---------|
| `Fixed` | 范围固定不变，不随数据调整 | 已知数据范围，不需要调整 |
| `Auto` | 完全根据数据自动调整范围 | 数据范围未知，需要完全自动适应 |
| `Adaptive` | 在初始范围基础上智能扩展 | 有一个大概的初始范围，允许自动微调 |

### PRPD 配置参数

参见 [动态量程配置参数](#动态量程算法说明)

### PRPD 暂停/恢复

PRPD 支持暂停数据更新，用于冻结当前统计分布进行观察。

| API | 说明 |
|-----|------|
| `void pause(bool blockNewData = true)` | 暂停数据更新。`blockNewData = true` 时禁止接收新数据，新数据直接丢弃 |
| `void resume()` | 恢复数据更新，继续接受新数据和统计累积 |
| `bool isPaused() const` | 检查当前是否已暂停 |
| `void togglePause()` | 切换暂停/恢复状态 |
| `void setAcceptData(bool enabled)` | 设置是否接受新数据（即使暂停也可单独控制） |
| `bool isAcceptingData() const` | 检查是否正在接受新数据 |

**默认行为** (`pause()` 不带参数):
- ✅ 不再接受新数据，新来的数据直接丢弃
- ✅ 当前统计分布完全冻结，不会变化
- ✅ 环形缓冲不会覆盖老数据
- ✅ 保持所有当前统计信息不变

**`pause(false)` (允许接收新数据):**
- ✅ 仍然接受新数据
- ✅ 统计正常更新，满了正常覆盖老数据
- ✅ 只是标记为暂停状态，不影响内部处理

---

## PRPS 图表

### PRPS 工作原理

PRPS (Phase Resolved Pulse Sequence) 是**局部放电脉冲序列图**，用于显示放电信号的三维时序演变。

**工作流程：**
1. 每个周期数据生成一条水平线，位于 Z 轴最大位置 `MAX_Z_POSITION`
2. 后台定时动画，所有线组向 Z 轴负方向移动
3. 当线组 Z 位置小于 `MIN_Z_POSITION` 时，标记为不活跃并清理
4. 这样就形成了**新数据从后方进来，旧数据从前方出去**的滚动效果
5. Z 轴代表时间，展示数据随时间的演变过程

### PRPS 常量

```cpp
struct PRPSConstants {
    static constexpr int   PHASE_POINTS     = 200;  ///< 相位采样点数
    static constexpr int   CYCLES_PER_FRAME = 50;   ///< 每帧周期数
    static constexpr float GL_AXIS_LENGTH   = 6.0f; ///< OpenGL 坐标轴长度
    static constexpr float MAX_Z_POSITION   = 6.0f; ///< 最大 Z 轴位置
    static constexpr float MIN_Z_POSITION   = 0.0f; ///< 最小 Z 轴位置
    static constexpr float PHASE_MAX        = 360.0f;
    static constexpr float PHASE_MIN        = 0.0f;
    static constexpr size_t MAX_LINE_GROUPS = 80;   ///< 最大线组数量
};
```

### PRPS API 参考

#### 量程设置

API 和 [PRPD](#prpd-api-参考) 完全一致。

#### 量程查询

API 和 [PRPD](#prpd-api-参考) 完全一致。

#### 运行时调整

API 和 [PRPD](#prpd-api-参考) 完全一致。

#### 硬限制

API 和 [PRPD](#prpd-api-参考) 完全一致。

#### 数据接口

| API | 说明 |
|-----|------|
| `void addCycleData(const std::vector<float>& cycleData)` | 添加一个周期的放电数据，长度必须等于相位采样点数 |
| `void setThreshold(float threshold)` | 设置幅值阈值，低于阈值的不显示，默认 0.1 |
| `void setPhaseRange(float min, float max)` | 设置相位范围，默认 0-360° |
| `void setPhasePoint(int phasePoint)` | 设置相位采样点数，默认 200 |
| `void setUpdateInterval(int intervalMs)` | 设置动画更新间隔，默认 20ms |
| `void resetData()` | 清空所有数据，重置到初始状态 |

### PRPS 量程模式

和 [PRPD 量程模式](#prpd-量程模式) 完全一致。

### PRPS 配置参数

参见 [动态量程配置参数](#动态量程算法说明)

### PRPS 暂停/恢复

PRPS 支持暂停动画更新，停止滚动，保持当前数据不消失。

| API | 说明 |
|-----|------|
| `void pause(bool blockNewData = true)` | 暂停动画更新。`blockNewData = true` 时禁止接收新数据 |
| `void resume()` | 恢复动画更新，继续滚动和数据接收 |
| `bool isPaused() const` | 检查当前是否已暂停 |
| `void togglePause()` | 切换暂停/恢复状态 |
| `void setAcceptData(bool enabled)` | 设置是否接受新数据（即使暂停也可单独控制） |
| `bool isAcceptingData() const` | 检查是否正在接受新数据 |

**默认行为** (`pause()` 不带参数):
- ✅ 停止滚动动画，已有的线组不再向 Z 负方向移动
- ✅ 不再接受新数据，新来的数据直接丢弃
- ✅ 所有数据保持当前位置不变，不会消失
- ✅ 不会自动清理移出范围的线组

**`pause(false)` (允许接收新数据):**
- ✅ 停止滚动动画，已有线组位置保持不变
- ✅ 仍然接受新数据，新线组添加到 Z 轴末尾
- ✅ 结果就是：图像慢慢变长，已有的位置不动，可以观察累积过程

**恢复行为** (`resume()`):
- ✅ 继续滚动动画，从暂停的位置继续
- ✅ 恢复接受新数据
- ✅ 恢复自动清理不活跃线组
- ✅ 不需要"追更"，暂停期间添加的数据已经在正确位置了

---

## 通用配置

### 硬限制

硬限制用于防止异常数据导致量程过大，对 PRPD 和 PRPS 都有效。

当启用硬限制后：
- 动态量程计算出来的范围不会超出硬限制的 `[min, max]`
- 即使数据超出硬限制，显示范围也会被裁剪
- 可以有效防止偶尔的异常脉冲导致量程变得很大

API:
```cpp
void setHardLimits(float min, float max, bool enabled = true);
```

### 动态量程配置参数

`DynamicRange::DynamicRangeConfig` 配置结构，用于 Auto 和 Adaptive 模式：

| 参数 | 默认值 | 作用 | 影响 |
|------|--------|------|------|
| `bufferRatio` | 0.3 | 控制数据最大值之上的额外空间比例 | 增大会使量程上限比数据最大值更高，留有更多空白区域 |
| `responseSpeed` | 0.7 | 控制量程变化的平滑响应速度 | 值越大，量程变化越快；值越小，变化越平滑缓慢 |
| `smartAdjustment` | true | 自动调整扩展和收缩的速度差异 | 启用时扩展快收缩慢，提供更稳定的视觉体验 |
| `targetTickCount` | 6 | 期望的刻度数量 | 影响刻度间隔和美观范围计算 |
| `enforceMinimumRange` | true | 是否使用最小量程功能 | 当数据较小时固定使用最小量程 |
| `minimumRangeMax` | 5.0 | 最小量程的上限值 | 当数据小于阈值时使用的固定上限 |
| `minimumThreshold` | 2.0 | 激活最小量程的数据阈值 | 当数据最大值小于此值时启用最小量程 |

### 动态量程算法流程

简要流程：

1. **数据范围确定** - 计算当前数据的最小最大值
2. **添加缓冲区** - 最大值加上 `bufferRatio` 比例的缓冲区
3. **计算美观范围** - 根据期望刻度数计算美观的刻度步长和范围
4. **平滑更新** - 使用 `responseSpeed` 控制的平滑因子从当前范围过渡到目标范围
5. **智能调整** - 扩展快、收缩慢，减少视觉抖动
6. **抗抽搐** - 微小变化不更新，避免频繁跳动

详细算法分析请参考 [dev.md](../dev.md)。

---

## 使用示例

### 基本创建和使用

```cpp
#include <QApplication>
#include <prographics/charts/prpd/prpd.h>
#include <prographics/charts/prps/prps.h>

int main(int argc, char *argv[]) {
    QSurfaceFormat format;
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    
    QApplication app(argc, argv);
    
    // 创建 PRPD 图表
    ProGraphics::PRPDChart *prpd = new ProGraphics::PRPDChart();
    prpd->setFixedRange(-75, -30);  // 固定量程
    prpd->setPhaseRange(0, 360);     // 相位范围
    
    // 创建 PRPS 图表
    ProGraphics::PRPSChart *prps = new ProGraphics::PRPSChart();
    prps->setFixedRange(-75, -30);   // 固定量程
    prps->setUpdateInterval(20);     // 动画更新间隔 20ms
    
    // 添加数据
    std::vector<float> cycleData = generateYourData();
    prpd->addCycleData(cycleData);
    prps->addCycleData(cycleData);
    
    // 暂停/继续（同时控制两个）
    void onPauseButtonClicked() {
        if (!prpd->isPaused()) {
            prpd->pause(true);  // 暂停，丢弃新数据
            prps->pause(true);  // 暂停，丢弃新数据
            // 更新按钮文字...
        } else {
            prpd->resume();
            prps->resume();
            // 更新按钮文字...
        }
    }
    
    return app.exec();
}
```

### 使用自适应量程

```cpp
ProGraphics::DynamicRange::DynamicRangeConfig config;
config.bufferRatio = 0.3;       // 30% 缓冲区
config.responseSpeed = 0.7;     // 响应速度 0.7
config.smartAdjustment = true;  // 智能调整开启

prpd->setAdaptiveRange(-80, -20, config);
prps->setAdaptiveRange(-80, -20, config);
```

### 启用硬限制

```cpp
prpd->setHardLimits(-100, 0, true);  // 幅值不会超出 -100 ~ 0
prps->setHardLimits(-100, 0, true);
```

### 示例程序

项目 `examples/` 目录下有完整的示例程序，包含：
- 随机数据生成
- 量程模式切换
- 动态量程参数调整
- 硬限制设置
- **全局暂停/继续控制**
- 状态显示

可以参考示例程序进行集成。

---

## 常见问题

### Q: 暂停之后新数据去哪里了？

**A:** 默认 `pause(true)` 情况下，新数据直接在 `addCycleData` 入口被拒绝，直接返回，不会做任何处理，**直接丢掉**。这样保证画面完全不变。

如果 `pause(false)`，新数据会被正常处理，PRPD 正常累积，PRPS 正常添加到末尾，但 PRPS 不滚动。

### Q: 暂停恢复后，暂停期间的数据会补播放吗？

**A:** 不会。当前设计是：
- 如果允许接收，数据添加完就一直在那里了
- 恢复后直接继续滚动，不需要补
- 如果不允许接收，数据已经丢了，也不用补

这样设计避免了数据延迟和缓存累积，更符合实时可视化的需求。

### Q: PRPD 最多能存多少个周期？

**A:** 默认 `MAX_CYCLES = 500`，编译时常量，如需修改可以在头文件中改。

### Q: PRPS 最多能显示多少条线？

**A:** 默认 `MAX_LINE_GROUPS = 80`，编译时常量。当超过这个数量，最老的线会被删除。

### Q: 可以只暂停 PRPS 不暂停 PRPD 吗？

**A:** API 支持分别暂停，示例程序用了一个按钮同时控制两个，这是示例程序的选择。如果你需要分别控制，可以分开加按钮调用各自的 `pause()` 和 `resume()`。

### Q: 相位采样点数必须是 200 吗？

**A:** 不是，可以通过 `setPhasePoint(int)` 修改。但 PRPD 的幅值分箱数 `AMPLITUDE_BINS` 是编译时常量 100，如需修改需要改头文件重新编译。

### Q: 动态量程什么时候会收缩？

**A:** 每 5 帧检查一次，如果数据范围使用率低于 30%，会尝试收缩。收缩速度比扩展慢，提供更稳定的视觉体验。

---

## 许可证

本文档和 ProGraphics 库一样，基于 LGPL-3.0 许可证。