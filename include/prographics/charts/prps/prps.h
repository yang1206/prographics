#pragma once

#include "prographics/charts/coordinate/coordinate3d.h"
#include <QTimer>

#include "prographics/core/graphics/primitive2D.h"

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


    class PRPSChart : public Coordinate3D {
        Q_OBJECT

    public:
        explicit PRPSChart(QWidget *parent = nullptr);

        ~PRPSChart() override;

        // 数据接口
        void addCycleData(const std::vector<float> &cycleData);

        // 配置接口
        void setThreshold(float threshold) { m_threshold = threshold; }

        void setAmplitudeRange(float min, float max);

        void setPhaseRange(float min, float max);

    protected:
        void initializeGLObjects() override;

        void paintGLObjects() override;

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
        QTimer m_prpsAnimationTimer;
        float m_prpsAnimationSpeed = 0.1f;

        // 实际数据范围（用于渲染）
        float m_amplitudeMin = -75.0f;
        float m_amplitudeMax = -30.0f;

        // 显示范围（用于坐标轴）
        float m_displayMin = -75.0f;
        float m_displayMax = -30.0f;

        // 平滑过渡参数
        float m_rangeTransitionSpeed = 0.1f; // 范围过渡速度
        float m_rangeUpdateThreshold = 10.0f; // 更新阈值（dBm）


        // 坐标范围
        float m_phaseMin = PRPSConstants::PHASE_MIN;
        float m_phaseMax = PRPSConstants::PHASE_MAX;

        // 处理方法
        void processCurrentCycles();

        void updatePRPSAnimation();

        void cleanupInactiveGroups();

        // 坐标映射方法
        float mapPhaseToGL(float phase) const;

        float mapAmplitudeToGL(float amplitude) const;

        float mapGLToPhase(float glX) const;

        float mapGLToAmplitude(float glY) const;

        // 添加自适应范围的方法
        void updateAmplitudeRange(const std::vector<float> &newData);

        // 添加重新计算所有线组的方法
        void recalculateLineGroups();
    };
}
