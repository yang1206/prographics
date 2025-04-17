# 动态量程计算过程与参数影响分析

## 配置参数作用与影响

| 配置参数 | 默认值 | 作用 | 影响 |
|---------|--------|------|------|
| `bufferRatio` | 0.3 | 控制数据最大值之上的额外空间比例 | 增大会使量程上限比数据最大值更高，留有更多空白区域 |
| `responseSpeed` | 0.7 | 控制量程变化的速度 | 值越大，量程变化越快；值越小，变化越平滑缓慢 |
| `smartAdjustment` | true | 自动调整扩展和收缩的速度差异 | 启用时扩展快收缩慢，提供更稳定的视觉体验 |
| `targetTickCount` | 6 | 期望的刻度数量 | 影响刻度间隔和美观范围计算 |
| `enforceMinimumRange` | true | 是否使用最小量程功能 | 当数据较小时固定使用最小量程 |
| `minimumRangeMax` | 5.0 | 最小量程的上限值 | 当数据小于阈值时使用的固定上限 |
| `minimumThreshold` | 2.0 | 激活最小量程的数据阈值 | 当数据最大值小于此值时启用最小量程 |
| `useFixedRange` | false | 是否完全禁用动态调整 | 启用时将固定使用设置的范围 |

## 动态量程计算完整流程

### 1. 数据范围确定

1. `updateRange`方法接收新的数据集合
2. 使用`std::minmax_element`计算数据的最小值`dataMin`和最大值`dataMax`
3. 记录当前的显示范围`oldMin`和`oldMax`用于后续判断变化
4. 将新的数据范围`{dataMin, dataMax}`添加到历史数据队列`m_recentDataRanges`
5. 如果历史数据超过20个点，移除最旧的数据点

### 2. 首次初始化处理

1. 检查`m_initialized`标志
2. 如果为false，调用`initializeRange(dataMin, dataMax)`
3. 在`initializeRange`中:
   a. 计算数据范围`range = max(0.001, dataMax - dataMin)`
   b. 计算缓冲区大小`buffer = range * m_config.bufferRatio`
   c. 设置最小值为`min = dataMin`（不添加缓冲区）
   d. 设置最大值为`max = dataMax + buffer`（添加缓冲区）
   e. 调用`calculateNiceRange`计算美观范围
   f. 将计算结果设置为目标范围和当前范围
   g. 输出调试信息
4. 设置`m_initialized = true`
5. 返回`true`表示需要重建绘图数据

### 3. 数据超出范围处理

1. 检查数据是否超出当前范围`dataExceedsRange = (dataMin < m_currentMin || dataMax > m_currentMax)`
2. 如果超出，立即响应:
   a. 调用`updateTargetRange(dataMin, dataMax)`更新目标范围
   b. 计算平滑因子，传入`isExpanding=true`表示是扩展操作
   c. 调用`smoothUpdateCurrentRange`平滑过渡到新范围
   d. 返回`true`表示需要重建绘图数据

### 4. 定期检查范围收缩

1. 每5帧执行一次收缩检查（`m_frameCounter % 5 == 0`）
2. 计算数据使用率`usageRatio = dataRange / currentRange`
3. 如果使用率低于30%:
   a. 计算历史数据的平均最大值`avgMax = calculateAverageMax()`
   b. 使用平均最大值或当前最大值（取较大者）调用`updateTargetRange`
   c. 计算平滑因子，传入`isExpanding=false`表示是收缩操作
   d. 调用`smoothUpdateCurrentRange`平滑过渡到新范围
   e. 调用`isSignificantChange`判断是否发生显著变化
   f. 根据显著变化判断结果返回是否需要重建绘图数据

### 5. 目标范围更新详细流程

1. 在`updateTargetRange`方法中:
   a. 计算数据范围`range = max(0.001, dataMax - dataMin)`
   b. 计算缓冲区大小`buffer = range * m_config.bufferRatio`
   c. 设置原始最小值`rawMin = dataMin`
   d. 设置原始最大值`rawMax = dataMax + buffer`
   e. 调用`calculateNiceRange`计算美观范围
   f. 进行抽搐检查:
      i. 计算相对变化`minChange`和`maxChange`
      ii. 如果变化小于5%，保持当前目标不变，直接返回
   g. 更新目标范围`m_targetMin`和`m_targetMax`
   h. 输出调试信息

### 6. 美观范围计算详细流程

1. 在`calculateNiceRange`方法中:
   a. 确保`min <= max`，否则交换两值
   b. 确保范围有最小宽度（至少0.001）
   c. 记录原始数据值和范围
   d. 调用`calculateNiceTickStep`计算美观的刻度步长
   e. 计算最小值:
      i. 向下取整到最接近的步长倍数`niceMin = floor(originalMin / niceStep) * niceStep`
      ii. 如果原始值距离过远（超过步长的70%），调整到下一个步长
   f. 计算最大值:
      i. 计算需要的步长数量`minSteps = ceil((originalMax - niceMin) / niceStep)`
      ii. 确保至少有`targetTickCount - 1`个步长
      iii. 计算最终的美观最大值`niceMax = niceMin + minSteps * niceStep`
   g. 缓冲区处理:
      i. 计算当前缓冲区`currentBuffer = niceMax - dataMax`
      ii. 计算目标缓冲区`targetBuffer = originalRange * bufferSize`
      iii. 根据缓冲区大小调整最大值
   h. 返回计算结果`{niceMin, niceMax}`

### 7. 刻度步长计算详细流程

1. 在`calculateNiceTickStep`方法中:
   a. 确保范围和目标刻度数有效
   b. 计算粗略的步长`roughStep = range / targetTicks`
   c. 计算步长的数量级`magnitude = 10^floor(log10(roughStep))`
   d. 标准化步长`normalizedStep = roughStep / magnitude`
   e. 选择美观的步长倍数:
      i. < 1.2: 使用1.0
      ii. < 2.5: 使用2.0
      iii. < 4.0: 使用2.5
      iv. < 7.0: 使用5.0
      v. 其他: 使用10.0
   f. 抗抽搐处理:
      i. 如果接近某个关键边界，调整到稳定的步长值
   g. 返回最终步长`niceStep * magnitude`

### 8. 平滑更新详细流程

1. 在`smoothUpdateCurrentRange`方法中:
   a. 如果未指定平滑因子，根据是扩张还是收缩计算平滑因子
   b. 限制平滑因子不超过0.5，避免过快变化
   c. 使用平滑系数更新当前范围:
      i. `m_currentMin += (m_targetMin - m_currentMin) * smoothFactor`
      ii. `m_currentMax += (m_targetMax - m_currentMax) * smoothFactor`
   d. 调用`calculateNiceRange`确保当前范围仍是美观范围
   e. 更新当前显示范围`m_currentMin`和`m_currentMax`
   f. 输出调试信息

### 9. 平滑因子计算详细流程

1. 在`calculateSmoothFactor`方法中:
   a. 基础因子为`baseFactor = m_config.responseSpeed`
   b. 如果启用智能调整:
      i. 扩展时: 返回`baseFactor * 1.5`（更快）
      ii. 显著收缩时: 返回`baseFactor * 1.2`（较快）
      iii. 普通收缩时: 返回`baseFactor * 1.0`（正常）
   c. 未启用智能调整时: 直接返回`baseFactor`

### 10. 显著变化判断详细流程

1. 在`isSignificantChange`方法中:
   a. 如果强制更新，直接返回true
   b. 检查是否完全相同（考虑浮点误差，使用EPSILON=0.0001）
   c. 计算变化比例:
      i. `minDiff = |m_currentMin - oldMin| / oldRange`
      ii. `maxDiff = |m_currentMax - oldMax| / oldRange`
   d. 根据响应速度计算阈值`threshold = 0.1 * (1.0 - m_config.responseSpeed * 0.5)`
   e. 返回是否发生显著变化`minDiff > threshold || maxDiff > threshold`

### 11. 历史数据处理

1. 平均最大值计算`calculateAverageMax`:
   a. 如果历史数据为空，返回0
   b. 计算所有历史数据最大值的总和
   c. 返回平均值`sum / size`

2. 最近最大值查找`findRecentMaxValue`:
   a. 在最近n帧中查找最大的数据最大值
   b. 用于最小量程判断

## 动态量程计算中的关键点和潜在问题

1. **缓冲区计算**：
   - 缓冲区仅应用于上限，使下限紧贴数据最小值
   - 在`calculateNiceRange`中会对缓冲区大小再次调整，可能导致实际缓冲区与`bufferRatio`设置的值不符

2. **步长计算**：
   - 美观步长通过数量级和标准化来确定，复杂度高
   - 对"不好除"的数据范围可能产生不稳定结果
   - 抗抽搐逻辑减少了边界情况下的不稳定性，但可能不够全面

3. **平滑更新**：
   - 平滑更新后会再次应用`calculateNiceRange`，可能导致最终结果与目标范围有差异
   - 智能调整提供了不同的平滑因子，但收缩速度仍可能不够理想

4. **范围判断**：
   - 数据超出当前范围会立即响应，确保数据不会超出显示区域
   - 定期检查使用率过低的情况，但只在特定帧数执行，存在滞后性

5. **抗抽搐机制**：
   - 在`updateTargetRange`中判断变化是否足够大，减少微小变化导致的抽搐
   - 但对于某些特定比例的数据范围仍可能不够稳定

6. **负数范围处理**：
   - `-100 - 0`等负数范围在计算中没有特殊处理，应能正常计算
   - 如果出现问题，可能是浮点精度或步长计算中的特殊情况

7. **空间复杂度**：
   - 保存最近20帧的数据范围，用于平均值计算和稳定性判断
   - 对于高频率数据更新可能形成内存压力

8. **最小量程机制**：
   - 代码中定义了`enforceMinimumRange`参数，但在`updateRange`方法中未完全实现
   - 最小量程功能未被充分利用
