# PRPD/PRPS API 迁移指南

## 概述

PRPD 和 PRPS 组件的 API 已经重构，提供了更直观和易用的接口。本文档将帮助你从旧 API 迁移到新 API。

## 核心变化

### 新的模式概念

新 API 基于三种明确的模式：

```cpp
enum class RangeMode {
    Fixed,      // 固定范围模式 - 永不改变
    Auto,       // 自动调整模式 - 根据数据动态调整  
    Adaptive    // 自适应模式 - 智能切换固定/自动
};
```

## 迁移映射表

| 旧 API | 新 API | 说明 |
|--------|--------|------|
| `setAmplitudeRange(min, max)` | `setFixedRange(min, max)` | 设置固定范围 |
| `setDynamicRangeEnabled(true)` | `setAutoRange()` | 启用自动调整 |
| `setDynamicRangeEnabled(false)` | `setFixedRange(min, max)` | 禁用动态调整，使用固定范围 |
| `setInitialRange(min, max)` | `setAdaptiveRange(min, max)` | 设置自适应模式的初始范围 |
| `setDynamicRangeConfig(config)` | `updateAutoRangeConfig(config)` | 更新自动/自适应模式的配置 |
| `isDynamicRangeEnabled()` | `getRangeMode() != RangeMode::Fixed` | 检查是否为动态模式 |
| `getDisplayRange()` | `getCurrentRange()` | 获取当前显示范围 |
| `getInitialRange()` | `getConfiguredRange()` | 获取配置的范围 |

## 具体迁移示例

### 1. 设置固定范围

#### 旧代码
```cpp
// 旧方式 - 需要两步，顺序不明确
chart->setAmplitudeRange(-60, -30);
chart->setDynamicRangeEnabled(false);
```

#### 新代码
```cpp
// 新方式 - 一步到位，语义清晰
chart->setFixedRange(-60, -30);
```

### 2. 启用自动调整

#### 旧代码
```cpp
// 旧方式 - 复杂的配置过程
DynamicRange::DynamicRangeConfig config;
config.bufferRatio = 0.3f;
config.responseSpeed = 0.7f;
chart->setDynamicRangeConfig(config);
chart->setDynamicRangeEnabled(true);
```

#### 新代码
```cpp
// 新方式 - 简洁明了
DynamicRange::DynamicRangeConfig config;
config.bufferRatio = 0.3f;
config.responseSpeed = 0.7f;
chart->setAutoRange(config);

// 或使用默认配置
chart->setAutoRange();
```

### 3. 设置自适应模式

#### 旧代码
```cpp
// 旧方式 - 概念不清晰
chart->setInitialRange(-75, -30);
chart->setDynamicRangeEnabled(true);
DynamicRange::DynamicRangeConfig config;
config.enableRangeRecovery = true;
chart->setDynamicRangeConfig(config);
```

#### 新代码
```cpp
// 新方式 - 概念明确
DynamicRange::DynamicRangeConfig config;
config.enableRangeRecovery = true;
chart->setAdaptiveRange(-75, -30, config);
```

### 4. 运行时切换模式

#### 旧代码
```cpp
// 旧方式 - 状态不明确
if (chart->isDynamicRangeEnabled()) {
    chart->setDynamicRangeEnabled(false);
    // 不知道当前范围是多少
} else {
    chart->setDynamicRangeEnabled(true);
}
```

#### 新代码
```cpp
// 新方式 - 状态透明
switch (chart->getRangeMode()) {
    case RangeMode::Fixed:
        chart->switchToAutoRange();
        break;
    case RangeMode::Auto:
    case RangeMode::Adaptive:
        auto [min, max] = chart->getCurrentRange();
        chart->switchToFixedRange(min, max);
        break;
}
```

### 5. 查询状态信息

#### 旧代码
```cpp
// 旧方式 - 信息不完整
bool enabled = chart->isDynamicRangeEnabled();
auto [min, max] = chart->getDisplayRange();
auto [initMin, initMax] = chart->getInitialRange();
```

#### 新代码
```cpp
// 新方式 - 完整的状态信息
RangeMode mode = chart->getRangeMode();
auto [currentMin, currentMax] = chart->getCurrentRange();
auto [configMin, configMax] = chart->getConfiguredRange();

// 可以根据模式做不同处理
switch (mode) {
    case RangeMode::Fixed:
        // 固定模式处理
        break;
    case RangeMode::Auto:
        // 自动模式处理
        break;
    case RangeMode::Adaptive:
        // 自适应模式处理
        break;
}
```

## 完整的迁移示例

### 示例 1：简单的固定范围应用

#### 旧代码
```cpp
void setupChart() {
    PRPDChart* chart = new PRPDChart();
    
    // 设置固定范围
    chart->setAmplitudeRange(-70, -20);
    chart->setDynamicRangeEnabled(false);
    
    // 检查状态
    if (!chart->isDynamicRangeEnabled()) {
        auto [min, max] = chart->getDisplayRange();
        qDebug() << "Fixed range:" << min << "to" << max;
    }
}
```

#### 新代码
```cpp
void setupChart() {
    PRPDChart* chart = new PRPDChart();
    
    // 设置固定范围 - 一步到位
    chart->setFixedRange(-70, -20);
    
    // 检查状态 - 信息更完整
    if (chart->getRangeMode() == PRPDChart::RangeMode::Fixed) {
        auto [min, max] = chart->getCurrentRange();
        qDebug() << "Fixed range:" << min << "to" << max;
    }
}
```

### 示例 2：动态范围应用

#### 旧代码
```cpp
void setupDynamicChart() {
    PRPSChart* chart = new PRPSChart();
    
    // 配置动态范围
    chart->setInitialRange(-80, -10);
    
    DynamicRange::DynamicRangeConfig config;
    config.bufferRatio = 0.2f;
    config.responseSpeed = 0.8f;
    config.enableRangeRecovery = true;
    chart->setDynamicRangeConfig(config);
    
    chart->setDynamicRangeEnabled(true);
}

void updateConfig() {
    // 运行时更新配置
    DynamicRange::DynamicRangeConfig newConfig;
    newConfig.bufferRatio = 0.3f;
    chart->setDynamicRangeConfig(newConfig);
}
```

#### 新代码
```cpp
void setupDynamicChart() {
    PRPSChart* chart = new PRPSChart();
    
    // 配置自适应范围 - 一步完成
    DynamicRange::DynamicRangeConfig config;
    config.bufferRatio = 0.2f;
    config.responseSpeed = 0.8f;
    config.enableRangeRecovery = true;
    
    chart->setAdaptiveRange(-80, -10, config);
}

void updateConfig() {
    // 运行时更新配置 - 方法名更清晰
    DynamicRange::DynamicRangeConfig newConfig;
    newConfig.bufferRatio = 0.3f;
    chart->updateAutoRangeConfig(newConfig);
}
```

### 示例 3：模式切换应用

#### 旧代码
```cpp
void toggleMode() {
    if (chart->isDynamicRangeEnabled()) {
        // 切换到固定模式，但不知道当前范围
        chart->setDynamicRangeEnabled(false);
        // 可能需要重新设置范围
        chart->setAmplitudeRange(-60, -30);
    } else {
        // 切换到动态模式
        chart->setDynamicRangeEnabled(true);
    }
}
```

#### 新代码
```cpp
void toggleMode() {
    switch (chart->getRangeMode()) {
        case PRPSChart::RangeMode::Fixed:
            // 从固定切换到自动
            chart->switchToAutoRange();
            break;
            
        case PRPSChart::RangeMode::Auto:
        case PRPSChart::RangeMode::Adaptive:
            // 从动态切换到固定，保持当前范围
            auto [min, max] = chart->getCurrentRange();
            chart->switchToFixedRange(min, max);
            break;
    }
}
```

## 迁移检查清单

### 1. 搜索和替换
- [ ] 将 `setAmplitudeRange` 替换为 `setFixedRange`
- [ ] 将 `setDynamicRangeEnabled(true)` 替换为 `setAutoRange()`
- [ ] 将 `setDynamicRangeEnabled(false)` 替换为 `setFixedRange(min, max)`
- [ ] 将 `setInitialRange` 替换为 `setAdaptiveRange`
- [ ] 将 `setDynamicRangeConfig` 替换为 `updateAutoRangeConfig`
- [ ] 将 `getDisplayRange` 替换为 `getCurrentRange`
- [ ] 将 `getInitialRange` 替换为 `getConfiguredRange`

### 2. 逻辑检查
- [ ] 确认所有固定范围的设置都使用 `setFixedRange`
- [ ] 确认所有动态范围的设置都明确选择了 `Auto` 或 `Adaptive` 模式
- [ ] 更新状态查询逻辑，使用 `getRangeMode()` 而不是 `isDynamicRangeEnabled()`
- [ ] 检查模式切换逻辑，确保正确处理当前范围

### 3. 测试验证
- [ ] 测试固定范围模式的功能
- [ ] 测试自动调整模式的功能
- [ ] 测试自适应模式的功能
- [ ] 测试运行时模式切换
- [ ] 测试配置更新功能

## 新功能优势

### 1. 更直观的 API
- 方法名直接表达意图
- 一个方法完成一个完整功能
- 减少了用户的猜测和试错

### 2. 更好的状态管理
- 明确的模式概念
- 完整的状态查询接口
- 透明的配置信息

### 3. 更强的功能
- 便捷的运行时切换
- 灵活的配置更新
- 更好的错误处理

## 常见问题

### Q: 为什么要删除旧 API 而不是保持兼容？
A: 旧 API 设计存在根本性问题，保持兼容会让用户继续使用有问题的接口。直接删除并提供迁移指南能确保所有用户都使用更好的新接口。

### Q: 新 API 的性能如何？
A: 新 API 在固定模式下性能更好，因为避免了不必要的动态量程计算。自动和自适应模式的性能与旧版本相当。

### Q: 如何快速迁移大量代码？
A: 建议使用 IDE 的全局搜索替换功能，按照迁移映射表逐步替换。对于复杂的逻辑，建议手动检查和测试。

### Q: 新 API 是否向后兼容？
A: 不兼容。这是一次破坏性更新，但提供了完整的迁移路径和更好的用户体验。

## 总结

新的 API 设计解决了旧版本中的所有问题：
- ✅ 直观的方法命名
- ✅ 清晰的概念模型
- ✅ 一步到位的操作
- ✅ 完整的状态查询
- ✅ 灵活的运行时控制

虽然需要一些迁移工作，但新 API 将大大提升开发体验和代码质量。