#pragma once

#include "prographics/charts/coordinate/coordinate2d.h"
#include "prographics/prographics_export.h"
#include "prographics/utils/utils.h"
#include <memory>
#include <vector>

class QMouseEvent;

namespace ProGraphics {

    class Line2D;
    class Point2D;

    /**
     * @brief PRPD 幅值轴方向的水平参考线（纯几何：给定幅值处的横线，与业务语义无关）。
     *
     * 相位横跨当前相位范围；量程变化时幅值位置按当前映射更新。
     */
    struct PROGRAPHICS_EXPORT AmplitudeLine {
        float     amplitudeDbm = 0.0f; ///< 幅值（与纵轴物理单位一致，如 dBm）
        QVector4D color{1.0f, 0.0f, 0.0f, 1.0f}; ///< RGBA
        float     lineWidth = 2.0f; ///< 线宽（像素）
        bool      visible   = true; ///< 为 false 时不绘制
    };

    /**
     * @brief PRPD 图表相关常量
     */
    struct PRPDConstants {
        static constexpr float GL_AXIS_LENGTH = 5.0f; ///< OpenGL 坐标轴长度
        static constexpr float POINT_SIZE = 8.0f; ///< 点渲染大小
        static constexpr int PHASE_POINTS = 200; ///< 相位采样点数
        static constexpr int MAX_CYCLES = 500; ///< 最大周期缓存数
        static constexpr float PHASE_MAX = 360.0f; ///< 相位最大值
        static constexpr float PHASE_MIN = 0.0f; ///< 相位最小值
        static constexpr int AMPLITUDE_BINS = 100; ///< 幅值划分格子数
    };

    /**
     * @brief PRPD (Phase Resolved Partial Discharge) 局部放电相位分布图
     *
     * 用于显示局部放电信号的相位-幅值-频次三维分布关系。
     * 支持三种量程模式：固定、自动、自适应。
     */
    class PROGRAPHICS_EXPORT PRPDChart : public Coordinate2D {
        Q_OBJECT

    public:
        /**
         * @brief 量程模式
         */
        enum class RangeMode {
            Fixed, ///< 固定模式 - 范围不随数据变化
            Auto, ///< 自动模式 - 完全根据数据动态调整
            Adaptive ///< 自适应模式 - 在初始范围基础上智能调整
        };

        explicit PRPDChart(QWidget *parent = nullptr);

        ~PRPDChart() override;

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
        void setAutoRange(const DynamicRange::DynamicRangeConfig &config = DynamicRange::DynamicRangeConfig{});

        /**
         * @brief 设置自适应量程
         * @param initialMin 初始最小值
         * @param initialMax 初始最大值
         * @param config 动态量程配置参数
         */
        void setAdaptiveRange(float initialMin,
                              float initialMax,
                              const DynamicRange::DynamicRangeConfig &config = DynamicRange::DynamicRangeConfig{});

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
        void updateAutoRangeConfig(const DynamicRange::DynamicRangeConfig &config);

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
        void addCycleData(const std::vector<float> &cycleData);

        /**
         * @brief 设置相位范围
         */
        void setPhaseRange(float min, float max);

        /**
         * @brief 设置相位采样点数
         */
        void setPhasePoint(int phasePoint);

        /**
         * @brief 设置幅值方向水平参考线（替换整条列表）。
         *
         * 与散点数据无关：列表非空、内部 drawers 已同步且存在 visible==true 的条目即绘制；
         * 幅值位置随当前量程映射更新。
         */
        void setAmplitudeLines(const std::vector<AmplitudeLine> &lines);

        /**
         * @brief 移除所有幅值水平参考线。
         */
        void clearAmplitudeLines();

        /**
         * @brief 当前水平参考线配置（只读）。
         */
        const std::vector<AmplitudeLine> &amplitudeLines() const { return m_amplitudeLineSpecs; }

        /**
         * @brief 是否允许拖动参考线以修改幅值（左键）。默认关闭以免干扰现有手势。
         */
        void setAmplitudeReferenceLinesDraggable(bool enabled);

        /**
         * @brief @see setAmplitudeReferenceLinesDraggable
         */
        bool amplitudeReferenceLinesDraggable() const { return m_amplitudeRefDragEnabled; }

        /**
         * @brief 拖动时幅值量化步长（例如 1 表示按整数步长对齐）。
         */
        void setAmplitudeReferenceLineDragSnapStep(float step);

        float amplitudeReferenceLineDragSnapStep() const { return m_dragSnapStep; }

        /**
         * @brief 竖直方向命中参考线的像素容差。
         */
        void setAmplitudeReferenceLinePickTolerancePx(float pixels);

        float amplitudeReferenceLinePickTolerancePx() const { return m_dragPickTolerancePx; }

        /**
         * @brief 重置所有数据
         */
        void resetData() {
            m_cycleBuffer.data.clear();
            m_cycleBuffer.binIndices.clear();
            m_cycleBuffer.currentIndex = 0;
            m_cycleBuffer.isFull = false;
            m_renderBatchMap.clear();
            clearFrequencyTable();

            float displayMin, displayMax;
            if (m_rangeMode == RangeMode::Fixed) {
                displayMin = m_fixedMin;
                displayMax = m_fixedMax;
            } else {
                displayMin = m_configuredMin;
                displayMax = m_configuredMax;
                m_dynamicRange.setInitialRange(displayMin, displayMax);
            }

            updateAxisTicks(displayMin, displayMax);
            update();
        }

        /**
         * @brief 暂停数据更新
         * 
         * 暂停后：
         * - 不再接受新数据
         * - 当前统计分布完全冻结不变
         * - 不会覆盖老数据，保持所有当前统计信息
         * 
         * @param blockNewData 是否禁止添加新数据
         */
        void pause(bool blockNewData = true);

        /**
         * @brief 恢复数据更新
         * 
         * 恢复后：
         * - 继续接受新数据
         * - 继续累积统计和覆盖老数据
         */
        void resume();

        /**
         * @brief 检查当前是否已暂停
         * @return true 如果已暂停
         */
        bool isPaused() const { return m_paused; }

        /**
         * @brief 切换暂停/恢复状态
         */
        void togglePause();

        /**
         * @brief 设置是否接受新数据
         * 
         * 即使暂停，也可以控制是否接受新数据
         * @param enabled true 接受新数据，false 不接受
         */
        void setAcceptData(bool enabled) { m_acceptData = enabled; }

        /**
         * @brief 检查是否正在接受新数据
         * @return true 如果接受新数据
         */
        bool isAcceptingData() const { return m_acceptData; }

    signals:
        void amplitudeReferenceLineDragStarted(int index, float amplitudeDbm);
        void amplitudeReferenceLineDragged(int index, float amplitudeDbm);
        void amplitudeReferenceLineDragEnded(int index, float amplitudeDbm);

    protected:
        void initializeGLObjects() override;

        void paintGLObjects() override;

        void mousePressEvent(QMouseEvent *event) override;

        void mouseMoveEvent(QMouseEvent *event) override;

        void mouseReleaseEvent(QMouseEvent *event) override;

    private:
        // ==================== 内部数据结构 ====================

        struct PairHash {
            size_t operator()(const std::pair<int, int> &p) const {
                return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
            }
        };

        struct RenderBatch {
            std::unordered_map<std::pair<int, int>, Transform2D, PairHash> pointMap;
            int frequency;
            mutable std::vector<Transform2D> transforms;
            mutable bool needsRebuild = true;

            void rebuildTransforms(const QVector4D &color) const {
                if (!needsRebuild)
                    return;
                transforms.clear();
                transforms.reserve(pointMap.size());
                for (const auto &[_, transform]: pointMap) {
                    Transform2D t = transform;
                    t.color = color;
                    transforms.push_back(t);
                }
                needsRebuild = false;
            }
        };

        using BinIndex = uint16_t;
        using FrequencyTable = std::array<std::array<int, PRPDConstants::AMPLITUDE_BINS>, PRPDConstants::PHASE_POINTS>;

        struct CycleBuffer {
            std::vector<std::vector<float> > data;
            std::vector<std::vector<BinIndex> > binIndices;
            int currentIndex = 0;
            bool isFull = false;
        };

        // ==================== 成员变量 ====================

        CycleBuffer m_cycleBuffer;
        FrequencyTable m_frequencyTable;
        std::unordered_map<int, RenderBatch> m_renderBatchMap;
        int m_maxFrequency = 0;

        std::unique_ptr<Point2D> m_pointRenderer;

        std::vector<AmplitudeLine>              m_amplitudeLineSpecs;
        std::vector<std::unique_ptr<Line2D>> m_amplitudeLineDrawers;

        float m_amplitudeMin = -75.0f;
        float m_amplitudeMax = -30.0f;
        float m_displayMin = -75.0f;
        float m_displayMax = -30.0f;
        float m_phaseMin = PRPDConstants::PHASE_MIN;
        float m_phaseMax = PRPDConstants::PHASE_MAX;
        int m_phasePoints = PRPDConstants::PHASE_POINTS;

        DynamicRange m_dynamicRange{0.0f, 50.0f, DynamicRange::DynamicRangeConfig()};

        RangeMode m_rangeMode = RangeMode::Fixed;
        float m_fixedMin = -75.0f;
        float m_fixedMax = -30.0f;
        float m_configuredMin = -75.0f;
        float m_configuredMax = -30.0f;

        bool m_paused = false;      ///< 是否暂停数据更新
        bool m_acceptData = true;  ///< 是否接受新数据

        bool m_amplitudeRefDragEnabled = false;
        bool m_draggingAmplitudeRef    = false;
        int  m_draggingAmplitudeRefIndex = -1;
        float m_dragSnapStep           = 1.0f;
        float m_dragPickTolerancePx    = 10.0f;

        // ==================== 私有方法 ====================

        void queryDisplayedAmplitudeRange(float &displayMin, float &displayMax) const;

        QVector3D unprojectWidgetToChartPlane(const QPoint &widgetPos) const;

        QPointF projectChartToWidget(const QVector3D &chartPos) const;

        int pickAmplitudeLineAt(const QPoint &widgetPos) const;

        float snapDragAmplitude(float amplitudeDbm) const;

        float amplitudeFromWidgetPos(const QPoint &pos) const;

        void updatePointTransformsFromFrequencyTable();

        void rebuildFrequencyTable();

        void clearFrequencyTable();

        void removePointFromBatch(int phaseIdx, BinIndex binIdx, int frequency);

        void addPointToBatch(int phaseIdx, BinIndex binIdx, int frequency);

        float mapPhaseToGL(float phase) const;

        float mapAmplitudeToGL(float amplitude) const;

        BinIndex getAmplitudeBinIndex(float amplitude) const;

        float getBinCenterAmplitude(BinIndex binIndex) const;

        QVector4D calculateColor(int frequency) const;

        void forceUpdateRange();

        void updateAxisTicks(float min, float max);

        void syncAmplitudeLineDrawers();

        void paintAmplitudeLines();

        bool hasVisibleAmplitudeLines() const;
    };
} // namespace ProGraphics
