#include "prographics/charts/prps/prps.h"
#include <random>
#include "prographics/utils/utils.h"

namespace ProGraphics {
// UpdateThread 实现
UpdateThread::UpdateThread(QObject* parent) : QThread(parent) {}

UpdateThread::~UpdateThread() { stop(); }

void UpdateThread::stop() {
    QMutexLocker locker(&m_mutex);
    m_abort = true;
    m_condition.wakeAll();
    locker.unlock();
    wait();
}

void UpdateThread::setPaused(bool paused) {
    QMutexLocker locker(&m_mutex);
    m_paused = paused;
    if (!paused) {
        m_condition.wakeAll();
    }
}

void UpdateThread::setUpdateInterval(int intervalMs) {
    QMutexLocker locker(&m_mutex);
    m_updateInterval = intervalMs;
}

void UpdateThread::run() {
    while (true) {
        {
            QMutexLocker locker(&m_mutex);
            if (m_abort) {
                return;
            }
            if (m_paused) {
                m_condition.wait(&m_mutex);
                continue;
            }
        }

        // 发送更新信号
        emit updateAnimation();

        // 等待指定的时间间隔
        msleep(m_updateInterval);
    }
}

// PRPSChart 实现
PRPSChart::PRPSChart(QWidget* parent) : Coordinate3D(parent), m_updateThread(this) {
    setSize(PRPSConstants::GL_AXIS_LENGTH);

    // 设置各轴的名称和单位
    setAxisName('x', "相位", "°");
    setAxisName('y', "幅值", "dBm");
    setAxisEnabled(false);

    // 初始化动态量程
    DynamicRange::DynamicRangeConfig config;

    m_dynamicRange = DynamicRange(0.0f, 5.0f, config);

    // 设置各轴的刻度范围
    setTicksRange('x', PRPSConstants::PHASE_MIN, PRPSConstants::PHASE_MAX, 90);
    setTicksRange('y', 0.0f, 5.0f, 0.2f);
    setAxisVisible('z', false);

    // 连接更新线程的信号到动画更新槽
    connect(
        &m_updateThread, &UpdateThread::updateAnimation, this, &PRPSChart::updatePRPSAnimation, Qt::QueuedConnection);

    // 启动更新线程
    m_updateThread.start();
}

PRPSChart::~PRPSChart() {
    // 停止更新线程
    m_updateThread.stop();

    // 清理资源
    makeCurrent();
    m_lineGroups.clear();
    doneCurrent();
}

void PRPSChart::initializeGLObjects() { Coordinate3D::initializeGLObjects(); }

void PRPSChart::paintGLObjects() {
    Coordinate3D::paintGLObjects();
    if (m_lineGroups.empty()) {
        return;
    }

    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.0f);
    for (const auto& group : m_lineGroups) {
        if (group->instancedLine && group->isActive) {
            // 更新Z位置的模型矩阵
            QMatrix4x4 model;
            model.translate(0, 0, group->zPosition);

            // 使用实例化渲染绘制线段
            group->instancedLine->drawInstanced(camera().getProjectionMatrix(),
                                                camera().getViewMatrix() * model, // 组合视图和模型矩阵
                                                group->transforms);
        }
    }

    glLineWidth(1.0f);
    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
}

void PRPSChart::addCycleData(const std::vector<float>& cycleData) {
    if (cycleData.size() != PRPSConstants::PHASE_POINTS) {
        qWarning() << "Invalid cycle data size:" << cycleData.size() << "expected:" << PRPSConstants::PHASE_POINTS;
        return;
    }

    if (m_lineGroups.size() >= PRPSConstants::MAX_LINE_GROUPS) {
        // 当达到最大数量时，移除最老的一组数据
        m_lineGroups.erase(m_lineGroups.begin());
    }

    // 更新动态量程
    bool rangeChanged = false;
    if (m_dynamicRangeEnabled) {
        rangeChanged = m_dynamicRange.updateRange(cycleData);
    }

    // 如果范围改变，更新坐标轴并重新计算所有线组
    if (rangeChanged) {
        auto [newDisplayMin, newDisplayMax] = m_dynamicRange.getDisplayRange();
        // qDebug() << "动态范围:" << newDisplayMin << "到" << newDisplayMax;
        // 更新坐标轴刻度
        float step = calculateNiceTickStep(newDisplayMax - newDisplayMin);
        // qDebug() << "计算步长:" << step;
        setTicksRange('y', newDisplayMin, newDisplayMax, step);
        // qDebug() << "PRPS- 量程更新:" << newDisplayMin << "-" << newDisplayMax;

        // 重新计算所有现有的线组
        recalculateLineGroups();
    }

    m_currentCycles.push_back(cycleData);
    processCurrentCycles();
    m_currentCycles.clear();
}

void PRPSChart::processCurrentCycles() {
    makeCurrent();

    auto newGroup = std::make_unique<LineGroup>();
    // 直接使用当前周期的数据
    const auto& cycleData = m_currentCycles.front();
    newGroup->amplitudes  = cycleData;

    // 预计算变换数量
    int validCount = 0;
    for (float amplitude : cycleData) {
        // 稍微调整判断逻辑：只要幅值大于等于显示下限就可能需要绘制
        if (amplitude >= m_dynamicRange.getDisplayRange().first) {
            validCount++;
        }
    }

    // 创建基础线段(从底部到顶部的竖线)
    newGroup->instancedLine = std::make_unique<Line2D>(QVector3D(0.0f, 0.0f, 0.0f),      // 起点在底部
                                                       QVector3D(0.0f, 1.0f, 0.0f),      // 终点在顶部 (单位长度)
                                                       QVector4D(1.0f, 1.0f, 1.0f, 1.0f) // 白色，实际颜色将在实例中设置
    );
    // 初始化实例化渲染器，确保VAO/VBO等已创建
    newGroup->instancedLine->initialize();

    newGroup->transforms.reserve(validCount); // 使用预计算的数量

    auto [displayMin, displayMax] = m_dynamicRange.getDisplayRange(); // 获取一次钳位后的范围

    for (int i = 0; i < PRPSConstants::PHASE_POINTS; ++i) {
        float amplitude = cycleData[i]; // 原始幅值

        // 如果幅值低于显示下限，则跳过
        if (amplitude < displayMin)
            continue;

        float phase = (float) i / (PRPSConstants::PHASE_POINTS - 1) * m_phaseMax;
        float glX   = mapPhaseToGL(phase);
        float glY   = mapAmplitudeToGL(amplitude); // 计算原始GL Y值

        // --- 新增：限制线段高度 ---
        // 将计算出的 GL Y 坐标限制在 [0, GL_AXIS_LENGTH] 范围内
        // 注意：mapAmplitudeToGL 内部已经处理了 amplitude < displayMin 的情况（返回 <= 0）
        // 我们这里主要限制上限
        glY = std::max(0.0f, std::min(glY, PRPSConstants::GL_AXIS_LENGTH));
        // --------------------------

        // 如果限制后的高度仍然 <= 0 (理论上只有 amplitude <= displayMin 时发生)
        if (glY <= 0.0f)
            continue;

        // 创建变换
        Transform2D transform;
        transform.position = QVector2D(glX, 0.0f); // 设置X位置
        transform.scale    = QVector2D(1.0f, glY); // 使用限制后的 glY 作为线段高度

        // --- 修改：颜色计算基于原始幅值比例 ---
        // 计算原始幅值在显示范围内的相对强度 (0到1，超出1的部分也算1)
        float intensity_ratio = (displayMax > displayMin) ? (amplitude - displayMin) / (displayMax - displayMin) : 1.0f;
        // 确保强度在[0, 1]范围内用于颜色计算 (超出上限的点显示为最亮色)
        float intensity_for_color = std::max(0.0f, std::min(1.0f, intensity_ratio));
        transform.color           = calculateColor(intensity_for_color);
        // -------------------------------------

        newGroup->transforms.push_back(transform);
    }

    m_lineGroups.push_back(std::move(newGroup));
    doneCurrent();
}

void PRPSChart::updatePRPSAnimation() {
    bool needCleanup = false;

    // 更新所有线组的位置和状态
    for (auto& group : m_lineGroups) {
        group->zPosition -= m_prpsAnimationSpeed;

        // 更新透明度
        if (group->zPosition < 2.0f) {
            float alpha = group->zPosition / 2.0f;
            for (auto& transform : group->transforms) {
                transform.color.setW(alpha);
            }
        }

        // 标记需要清理的线组
        if (group->zPosition <= PRPSConstants::MIN_Z_POSITION) {
            group->isActive = false;
            needCleanup     = true;
        }
    }

    // 如果有需要清理的线组，立即进行清理
    if (needCleanup) {
        cleanupInactiveGroups();
    }

    update();
}

void PRPSChart::cleanupInactiveGroups() {
    makeCurrent();

    // 使用移除-擦除习语删除不活跃的线组
    auto it = std::remove_if(m_lineGroups.begin(), m_lineGroups.end(), [](const std::unique_ptr<LineGroup>& group) {
        return !group->isActive;
    });

    // 实际删除不活跃的线组
    if (it != m_lineGroups.end()) {
        m_lineGroups.erase(it, m_lineGroups.end());
    }

    doneCurrent();
}

void PRPSChart::setDynamicRangeConfig(const DynamicRange::DynamicRangeConfig& config) {
    // 保存当前显示范围
    auto [oldMin, oldMax] = m_dynamicRange.getDisplayRange();

    // 更新配置
    m_dynamicRange = DynamicRange(oldMin, oldMax, config);

    // 更新坐标轴刻度
    auto [newMin, newMax] = m_dynamicRange.getDisplayRange();
    setTicksRange('y', newMin, newMax, calculateNiceTickStep(newMax - newMin, config.targetTickCount));

    // 重新计算所有线组
    recalculateLineGroups();
    update();
}

void PRPSChart::setAmplitudeRange(float min, float max) {
    // 设置固定范围
    m_dynamicRange.setDisplayRange(min, max);

    // 更新坐标轴
    float step = calculateNiceTickStep(max - min, m_dynamicRange.getConfig().targetTickCount);
    setTicksRange('y', min, max, step);

    // 重新计算所有线组
    recalculateLineGroups();
    update();
}

void PRPSChart::setPhaseRange(float min, float max) {
    m_phaseMin = min;
    m_phaseMax = max;
    setTicksRange('x', min, max, 85);
    update();
}

void PRPSChart::setDynamicRangeEnabled(bool enabled) { m_dynamicRangeEnabled = enabled; }

bool PRPSChart::isDynamicRangeEnabled() const { return m_dynamicRangeEnabled; }

float PRPSChart::mapPhaseToGL(float phase) const {
    return (phase - m_phaseMin) / (m_phaseMax - m_phaseMin) * PRPSConstants::GL_AXIS_LENGTH;
}

float PRPSChart::mapAmplitudeToGL(float amplitude) const {
    auto [displayMin, displayMax] = m_dynamicRange.getDisplayRange();
    return (amplitude - displayMin) / (displayMax - displayMin) * PRPSConstants::GL_AXIS_LENGTH;
}

float PRPSChart::mapGLToPhase(float glX) const {
    return glX / PRPSConstants::GL_AXIS_LENGTH * (m_phaseMax - m_phaseMin) + m_phaseMin;
}

float PRPSChart::mapGLToAmplitude(float glY) const {
    auto [displayMin, displayMax] = m_dynamicRange.getDisplayRange();
    return glY / PRPSConstants::GL_AXIS_LENGTH * (displayMax - displayMin) + displayMin;
}

void PRPSChart::recalculateLineGroups() {
    makeCurrent();
    auto [displayMin, displayMax] = m_dynamicRange.getDisplayRange(); // 获取一次钳位后的范围

    for (auto& group : m_lineGroups) {
        group->transforms.clear();

        // 预计算有效数据数量
        int validCount = 0;
        for (float amplitude : group->amplitudes) {
            if (amplitude >= displayMin) { // 检查是否在显示范围内（或以上）
                validCount++;
            }
        }
        group->transforms.reserve(validCount);

        for (int i = 0; i < PRPSConstants::PHASE_POINTS; ++i) {
            float amplitude = group->amplitudes[i]; // 原始幅值

            // 如果幅值低于显示下限，则跳过
            if (amplitude < displayMin)
                continue;

            float phase = (float) i / (PRPSConstants::PHASE_POINTS - 1) * m_phaseMax;
            float glX   = mapPhaseToGL(phase);
            float glY   = mapAmplitudeToGL(amplitude); // 计算原始GL Y值

            // --- 新增：限制线段高度 ---
            glY = std::max(0.0f, std::min(glY, PRPSConstants::GL_AXIS_LENGTH));
            // --------------------------

            // 如果限制后的高度仍然 <= 0
            if (glY <= 0.0f)
                continue;

            Transform2D transform;
            transform.position = QVector2D(glX, 0.0f);
            transform.scale    = QVector2D(1.0f, glY); // 使用限制后的 glY

            // --- 修改：颜色计算基于原始幅值比例 ---
            float intensity_ratio =
                (displayMax > displayMin) ? (amplitude - displayMin) / (displayMax - displayMin) : 1.0f;
            float intensity_for_color = std::max(0.0f, std::min(1.0f, intensity_ratio));
            transform.color           = calculateColor(intensity_for_color);
            // -------------------------------------

            // 更新透明度（如果需要，基于 zPosition）
            if (group->zPosition < 2.0f && group->zPosition >= PRPSConstants::MIN_Z_POSITION) {
                float alpha =
                    (group->zPosition - PRPSConstants::MIN_Z_POSITION) / (2.0f - PRPSConstants::MIN_Z_POSITION);
                transform.color.setW(alpha *
                                     intensity_for_color); // 透明度也受强度影响？或者只受Z影响？这里假设只受Z影响
                // 如果希望保持原始计算的alpha，需要修改
                // float base_alpha = calculateColor(intensity_for_color).w(); // 获取原始alpha
                // float z_alpha = (group->zPosition - PRPSConstants::MIN_Z_POSITION) / (2.0f -
                // PRPSConstants::MIN_Z_POSITION); transform.color.setW(base_alpha * z_alpha);
                // --> 为了简化，我们先用上面的简单方式：alpha = z_based_alpha
                float z_alpha =
                    (group->zPosition - PRPSConstants::MIN_Z_POSITION) / (2.0f - PRPSConstants::MIN_Z_POSITION);
                transform.color.setW(std::max(0.0f, z_alpha)); // 确保 alpha >= 0
            } else if (group->zPosition < PRPSConstants::MIN_Z_POSITION) {
                transform.color.setW(0.0f); // 完全透明
            } // else zPosition >= 2.0f, use alpha from calculateColor

            group->transforms.push_back(transform);
        }
    }
    doneCurrent();
    update(); // Trigger redraw
}
} // namespace ProGraphics
