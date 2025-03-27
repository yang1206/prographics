#pragma once

#include <QTimer>
#include "prographics/charts/coordinate/coordinate2d.h"
#include "prographics/utils/utils.h"

namespace ProGraphics {
    // PRPD图表相关常量定义
    struct PRPDConstants {
        // OpenGL渲染相关
        static constexpr float GL_AXIS_LENGTH = 5.0f; // OpenGL坐标系长度
        static constexpr float POINT_SIZE = 8.0f; // 点的渲染大小

        // 数据采样相关
        static constexpr int PHASE_POINTS = 200; // 相位采样点数
        static constexpr int MAX_CYCLES = 100; // 最大周期缓存数

        // 坐标范围
        static constexpr float PHASE_MAX = 360.0f; // 相位最大值
        static constexpr float PHASE_MIN = 0.0f; // 相位最小值

        static constexpr int AMPLITUDE_BINS = 100; // 幅值划分的格子数
    };

    class PRPDChart : public Coordinate2D {
        Q_OBJECT

    public:
        explicit PRPDChart(QWidget *parent = nullptr);

        ~PRPDChart() override;

        // 公共接口
        void addCycleData(const std::vector<float> &cycleData); ///< 添加一个周期的数据
        void setAmplitudeRange(float min, float max); ///< 设置幅值范围
        void setPhaseRange(float min, float max); ///< 设置相位范围
        void setDynamicRangeEnabled(bool enabled); ///< 设置是否启用动态量程
        bool isDynamicRangeEnabled() const; ///< 获取动态量程是否启用
        void setDynamicRangeConfig(const DynamicRange::DynamicRangeConfig &config); ///< 设置动态量程配置
        void resetData() {
            m_cycleBuffer.data.clear();
            m_cycleBuffer.binIndices.clear();
            m_cycleBuffer.currentIndex = 0;
            m_cycleBuffer.isFull = false;
            update();
        };

    protected:
        // OpenGL渲染相关
        void initializeGLObjects() override; ///< 初始化OpenGL对象
        void paintGLObjects() override; ///< 渲染OpenGL对象

    private:
        // ===== 数据结构定义 =====

        // 渲染批次：相同频次的点一起渲染
        struct RenderBatch {
            std::vector<Transform2D> transforms; // 变换矩阵数组
            int frequency; // 该批次的频次
        };

        using BinIndex = uint16_t; // 压缩后的幅值类型
        using FrequencyTable = std::array<std::array<int, PRPDConstants::AMPLITUDE_BINS>, PRPDConstants::PHASE_POINTS>;

        // 环形缓冲区：存储原始周期数据
        struct CycleBuffer {
            std::vector<std::vector<float> > data; // 周期数据数组
            std::vector<std::vector<BinIndex> > binIndices; // 每个点对应的bin索引
            int currentIndex = 0; // 当前写入位置
            bool isFull = false; // 缓冲区是否已满
        };

        // ===== 数据成员 =====
        // 数据存储
        CycleBuffer m_cycleBuffer; // 周期数据缓冲区
        FrequencyTable m_frequencyTable; // 频次统计表
        std::vector<RenderBatch> m_renderBatches; // 渲染批次
        int m_maxFrequency = 0; // 当前最大频次

        // 渲染器
        std::unique_ptr<Point2D> m_pointRenderer; // 点渲染器

        // 坐标范围
        float m_amplitudeMin = -75.0f; // 幅值最小值
        float m_amplitudeMax = -30.0f; // 幅值最大值
        float m_displayMin = -75.0f; // 显示最小值
        float m_displayMax = -30.0f; // 显示最大值
        float m_phaseMin = PRPDConstants::PHASE_MIN; // 相位最小值
        float m_phaseMax = PRPDConstants::PHASE_MAX; // 相位最大值

        // 动态量程管理器
        DynamicRange m_dynamicRange;
        bool m_dynamicRangeEnabled = true;

        // ===== 私有方法 =====
        // 数据处理

        void updatePointTransformsFromFrequencyTable();

        void rebuildFrequencyTable();

        void clearFrequencyTable();

        // 坐标转换
        float mapPhaseToGL(float phase) const;

        float mapAmplitudeToGL(float amplitude) const;

        BinIndex getAmplitudeBinIndex(float amplitude) const;

        float getBinCenterAmplitude(BinIndex binIndex) const;

        // 颜色计算
        QVector4D calculateColor(int frequency) const;
    };
} // namespace ProGraphics
