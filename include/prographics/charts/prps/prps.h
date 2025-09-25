#pragma once

#include "prographics/charts/coordinate/coordinate3d.h"
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "prographics/core/graphics/primitive2d.h"
#include "prographics/utils/utils.h"

namespace ProGraphics {
    // PRPS数据相关的常量
    struct PRPSConstants {
        // 数据采样相关
        static constexpr int PHASE_POINTS = 200; // 相位点数
        static constexpr int CYCLES_PER_FRAME = 50; // 每帧周期数

        // OpenGL坐标系相关
        static constexpr float GL_AXIS_LENGTH = 6.0f; // OpenGL坐标系长度
        static constexpr float MAX_Z_POSITION = 6.0f; // 最大Z轴位置 一定要与GL_AXIS_LENGTH一致
        static constexpr float MIN_Z_POSITION = 0.0f; // 最小Z轴位置

        // 逻辑坐标相关
        static constexpr float PHASE_MAX = 360.0f; // 相位最大值
        static constexpr float PHASE_MIN = 0.0f; // 相位最小值
        //最大线组数量限制
        static constexpr size_t MAX_LINE_GROUPS = 80;
    };


    class UpdateThread : public QThread {
        Q_OBJECT

    public:
        explicit UpdateThread(QObject *parent = nullptr);

        ~UpdateThread() override;

        void stop();

        void setPaused(bool paused);

        void setUpdateInterval(int intervalMs);

    signals:
        void updateAnimation(); // 触发动画更新

    protected:
        void run() override;

    private:
        QMutex m_mutex;
        QWaitCondition m_condition;
        bool m_abort{false};
        bool m_paused{false};
        int m_updateInterval{20}; // 默认约60fps
    };

    class PRPSChart : public Coordinate3D {
        Q_OBJECT

    public:
        explicit PRPSChart(QWidget *parent = nullptr);

        ~PRPSChart() override;

        // 数据接口
        void addCycleData(const std::vector<float> &cycleData);

        // 配置接口
        void setThreshold(float threshold) { m_threshold = threshold; }

        void setPhaseRange(float min, float max);

        void setDynamicRangeConfig(const DynamicRange::DynamicRangeConfig &config);

        void setAmplitudeRange(float min, float max);

        void setDynamicRangeEnabled(bool enabled);

        void setPhasePoint(int phasePoint);

        bool isDynamicRangeEnabled() const;

        void setUpdateInterval(int intervalMs) {
            m_updateThread.setUpdateInterval(intervalMs);
        };
        /**
    * @brief 设置初始显示范围（基准范围）
    * @param min 初始最小值
    * @param max 初始最大值
    */
        void setInitialRange(float min, float max) {
            m_dynamicRange.setInitialRange(min, max);
            update();
        }

        /**
         * @brief 获取初始显示范围
         */
        std::pair<float, float> getInitialRange() const {
            return m_dynamicRange.getInitialRange();
        }

        /**
         * @brief 设置硬限制范围（防止异常数据影响显示）
         * @param min 硬限制最小值
         * @param max 硬限制最大值
         * @param enabled 是否启用硬限制
         */
        void setHardLimits(float min, float max, bool enabled = true) {
            m_dynamicRange.setHardLimits(min, max, enabled);
            update();
        }

        /**
            * @brief 获取硬限制范围
            */
        std::pair<float, float> getHardLimits() const {
            return m_dynamicRange.getHardLimits();
        }

        /**
         * @brief 启用或禁用硬限制
         * @param enabled 是否启用
         */
        void enableHardLimits(bool enabled) {
            m_dynamicRange.enableHardLimits(enabled);
            update();
        }

        /**
         * @brief 检查硬限制是否启用
         */
        bool isHardLimitsEnabled() const {
            return m_dynamicRange.isHardLimitsEnabled();
        }

        void resetData() {
            m_currentCycles.clear();
            m_threshold = 0.1f;
            update();
        };

    protected:
        void initializeGLObjects() override;

        void paintGLObjects() override;

    private slots:
        void updatePRPSAnimation(); // 更新动画状态

    private:
        struct LineGroup {
            float zPosition = PRPSConstants::MAX_Z_POSITION;
            bool isActive = true;
            std::vector<float> amplitudes; // 直接存储幅值数据，不再需要 PhasePoint
            std::vector<Transform2D> transforms;
            std::unique_ptr<Line2D> instancedLine;
        };

        // 数据处理
        std::vector<std::vector<float> > m_currentCycles;
        float m_threshold = 0.1f;

        // 线组管理
        std::vector<std::unique_ptr<LineGroup> > m_lineGroups;
        UpdateThread m_updateThread;
        float m_prpsAnimationSpeed = 0.1f;

        // 动态量程管理
        DynamicRange m_dynamicRange{0.0f, 50.0f, DynamicRange::DynamicRangeConfig()};
        bool m_dynamicRangeEnabled = true;

        // 坐标范围
        float m_phaseMin = PRPSConstants::PHASE_MIN;
        float m_phaseMax = PRPSConstants::PHASE_MAX;

        int m_phasePoints = PRPSConstants::PHASE_POINTS;

        // 处理方法
        void processCurrentCycles();

        void cleanupInactiveGroups();

        // 坐标映射方法
        float mapPhaseToGL(float phase) const;

        float mapAmplitudeToGL(float amplitude) const;

        float mapGLToPhase(float glX) const;

        float mapGLToAmplitude(float glY) const;

        // 重新计算所有线组的方法
        void recalculateLineGroups();
    };
}
