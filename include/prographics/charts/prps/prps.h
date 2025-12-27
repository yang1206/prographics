#pragma once

#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "prographics/charts/coordinate/coordinate3d.h"
#include "prographics/core/graphics/primitive2d.h"
#include "prographics/utils/utils.h"

namespace ProGraphics {

/**
 * @brief PRPS 图表相关常量
 */
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

/**
 * @brief 动画更新线程
 */
class UpdateThread : public QThread {
    Q_OBJECT

  public:
    explicit UpdateThread(QObject* parent = nullptr);
    ~UpdateThread() override;

    void stop();
    void setPaused(bool paused);
    void setUpdateInterval(int intervalMs);

  signals:
    void updateAnimation();

  protected:
    void run() override;

  private:
    QMutex         m_mutex;
    QWaitCondition m_condition;
    bool           m_abort{false};
    bool           m_paused{false};
    int            m_updateInterval{20};
};

/**
 * @brief PRPS (Phase Resolved Pulse Sequence) 局部放电脉冲序列图
 * 
 * 用于显示局部放电信号的三维时序演变。
 * 支持三种量程模式：固定、自动、自适应。
 */
class PRPSChart : public Coordinate3D {
    Q_OBJECT

  public:
    /**
     * @brief 量程模式
     */
    enum class RangeMode {
        Fixed,   ///< 固定模式 - 范围不随数据变化
        Auto,    ///< 自动模式 - 完全根据数据动态调整
        Adaptive ///< 自适应模式 - 在初始范围基础上智能调整
    };

    explicit PRPSChart(QWidget* parent = nullptr);
    ~PRPSChart() override;

    // ==================== 量程设置 API ====================

    /**
     * @brief 设置固定量程
     * @param min 最小值
     * @param max 最大值
     */
    void setFixedRange(float min, float max);

    /**
     * @brief 设置自动量程
     * @param config 动态量程配置参数
     */
    void setAutoRange(const DynamicRange::DynamicRangeConfig& config = DynamicRange::DynamicRangeConfig{});

    /**
     * @brief 设置自适应量程
     * @param initialMin 初始最小值
     * @param initialMax 初始最大值
     * @param config 动态量程配置参数
     */
    void setAdaptiveRange(float                                   initialMin,
                          float                                   initialMax,
                          const DynamicRange::DynamicRangeConfig& config = DynamicRange::DynamicRangeConfig{});

    // ==================== 量程查询 API ====================

    /**
     * @brief 获取当前量程模式
     */
    RangeMode getRangeMode() const { return m_rangeMode; }

    /**
     * @brief 获取当前显示范围
     * @return {最小值, 最大值}
     */
    std::pair<float, float> getCurrentRange() const;

    /**
     * @brief 获取配置的范围（用于 UI 显示）
     * @return {配置最小值, 配置最大值}
     */
    std::pair<float, float> getConfiguredRange() const;

    // ==================== 运行时调整 API ====================

    /**
     * @brief 更新自动量程配置（仅在 Auto/Adaptive 模式下有效）
     * @param config 新的配置参数
     */
    void updateAutoRangeConfig(const DynamicRange::DynamicRangeConfig& config);

    /**
     * @brief 切换到固定量程
     */
    void switchToFixedRange(float min, float max);

    /**
     * @brief 切换到自动量程
     */
    void switchToAutoRange();

    // ==================== 硬限制 API ====================

    /**
     * @brief 设置硬限制范围（防止异常数据导致量程过大）
     * @param min 硬限制最小值
     * @param max 硬限制最大值
     * @param enabled 是否启用
     */
    void setHardLimits(float min, float max, bool enabled = true);

    /**
     * @brief 获取硬限制范围
     */
    std::pair<float, float> getHardLimits() const;

    /**
     * @brief 启用/禁用硬限制
     */
    void enableHardLimits(bool enabled);

    /**
     * @brief 检查硬限制是否启用
     */
    bool isHardLimitsEnabled() const;

    // ==================== 数据接口 ====================

    /**
     * @brief 添加一个周期的放电数据
     * @param cycleData 幅值数组，长度必须等于 PHASE_POINTS
     */
    void addCycleData(const std::vector<float>& cycleData);

    /**
     * @brief 设置阈值
     */
    void setThreshold(float threshold) { m_threshold = threshold; }

    /**
     * @brief 设置相位范围
     */
    void setPhaseRange(float min, float max);

    /**
     * @brief 设置相位采样点数
     */
    void setPhasePoint(int phasePoint);

    /**
     * @brief 设置动画更新间隔
     */
    void setUpdateInterval(int intervalMs) { m_updateThread.setUpdateInterval(intervalMs); }

    /**
     * @brief 重置所有数据
     */
    void resetData() {
        m_currentCycles.clear();
        m_threshold = 0.1f;
        if (m_rangeMode != RangeMode::Fixed) {
            m_dynamicRange.reset();
        }
        update();
    }

  protected:
    void initializeGLObjects() override;
    void paintGLObjects() override;

  private slots:
    void updatePRPSAnimation();

  private:
    // ==================== 内部数据结构 ====================

    struct LineGroup {
        float                    zPosition = PRPSConstants::MAX_Z_POSITION;
        bool                     isActive  = true;
        std::vector<float>       amplitudes;
        std::vector<Transform2D> transforms;
        std::unique_ptr<Line2D>  instancedLine;
    };

    // ==================== 成员变量 ====================

    std::vector<std::vector<float>>         m_currentCycles;
    float                                   m_threshold = 0.1f;
    std::vector<std::unique_ptr<LineGroup>> m_lineGroups;
    UpdateThread                            m_updateThread;
    float                                   m_prpsAnimationSpeed = 0.1f;

    DynamicRange m_dynamicRange{-75.0f, -30.0f, DynamicRange::DynamicRangeConfig()};

    RangeMode m_rangeMode     = RangeMode::Fixed;
    float     m_fixedMin      = -75.0f;
    float     m_fixedMax      = -30.0f;
    float     m_configuredMin = -75.0f;
    float     m_configuredMax = -30.0f;

    float m_phaseMin    = PRPSConstants::PHASE_MIN;
    float m_phaseMax    = PRPSConstants::PHASE_MAX;
    int   m_phasePoints = PRPSConstants::PHASE_POINTS;

    // ==================== 私有方法 ====================

    void processCurrentCycles();
    void cleanupInactiveGroups();

    float mapPhaseToGL(float phase) const;
    float mapAmplitudeToGL(float amplitude) const;
    float mapGLToPhase(float glX) const;
    float mapGLToAmplitude(float glY) const;

    void recalculateLineGroups();
    void forceUpdateRange();
    void updateAxisTicks(float min, float max);
};

} // namespace ProGraphics
