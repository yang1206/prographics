#pragma once

#include <QMatrix4x4>
#include <QPainter>
#include <vector>

namespace ProGraphics {
  /**
   * @brief 文本渲染器类
   * 
   * 负责在3D场景中渲染2D文本标签，支持：
   * - 文本样式自定义
   * - 3D空间定位
   * - 对齐方式控制
   * - 像素级偏移调整
   */
  class TextRenderer {
  public:
    /**
     * @brief 文本样式结构
     * 
     * 定义文本的外观属性
     */
    struct TextStyle {
      QString fontFamily;    ///< 字体族名称
      int fontSize;         ///< 字体大小（像素）
      QColor color;        ///< 文本颜色
      bool bold;           ///< 是否加粗
      bool italic;         ///< 是否斜体

      /**
       * @brief 默认构造函数
       * 
       * 初始化默认样式：
       * - Arial字体
       * - 10像素大小
       * - 白色
       * - 非加粗
       * - 非斜体
       */
      TextStyle()
        : fontFamily("Arial"), fontSize(10), color(Qt::white), bold(false),
          italic(false) {
      }

      /**
       * @brief 拷贝赋值运算符
       * @param other 要拷贝的样式对象
       * @return 当前对象的引用
       */
      TextStyle &operator=(const TextStyle &other) {
        if (this != &other) {
          fontFamily = other.fontFamily;
          fontSize = other.fontSize;
          color = other.color;
          bold = other.bold;
          italic = other.italic;
        }
        return *this;
      }
    };

    /**
     * @brief 文本标签结构
     * 
     * 定义单个文本标签的属性和位置
     */
    struct Label {
      QString text;                    ///< 标签文本内容
      QVector3D position;              ///< 3D世界坐标位置
      TextStyle style;                 ///< 文本样式
      bool visible;                    ///< 是否可见
      float offsetX;                   ///< X轴像素偏移
      float offsetY;                   ///< Y轴像素偏移
      Qt::Alignment alignment;         ///< 对齐方式

      /**
       * @brief 默认构造函数
       * 
       * 初始化为可见状态，无偏移，居中对齐
       */
      Label() : visible(true), offsetX(0.0f), offsetY(0.0f) {
      }
    };

    /**
     * @brief 默认构造函数
     */
    TextRenderer() = default;

    /**
     * @brief 添加文本标签
     * @param text 标签文本
     * @param position 3D位置
     * @param style 文本样式（可选）
     * @return 新创建的标签指针
     */
    Label *addLabel(const QString &text, const QVector3D &position,
                   const TextStyle &style = TextStyle());

    /**
     * @brief 移除文本标签
     * @param label 要移除的标签指针
     */
    void removeLabel(Label *label);

    /**
     * @brief 清除所有标签
     */
    void clear();

    /**
     * @brief 渲染所有标签
     * @param painter Qt绘制器
     * @param viewMatrix 视图矩阵
     * @param projectionMatrix 投影矩阵
     * @param width 视口宽度
     * @param height 视口高度
     */
    void render(QPainter &painter, const QMatrix4x4 &viewMatrix,
               const QMatrix4x4 &projectionMatrix, int width, int height);

    /**
     * @brief 更新标签文本
     * @param label 要更新的标签
     * @param text 新文本内容
     */
    void updateLabel(Label *label, const QString &text);

    /**
     * @brief 更新标签位置
     * @param label 要更新的标签
     * @param position 新的3D位置
     */
    void updateLabel(Label *label, const QVector3D &position);

    /**
     * @brief 更新标签样式
     * @param label 要更新的标签
     * @param style 新的样式
     */
    void updateLabel(Label *label, const TextStyle &style);

    /**
     * @brief 设置标签对齐方式
     * @param label 要设置的标签
     * @param alignment 对齐方式
     */
    void setAlignment(Label *label, Qt::Alignment alignment);

  private:
    std::vector<std::unique_ptr<Label>> m_labels;  ///< 标签容器

    /**
     * @brief 世界坐标转屏幕坐标
     * @param worldPos 世界坐标
     * @param viewMatrix 视图矩阵
     * @param projectionMatrix 投影矩阵
     * @param width 视口宽度
     * @param height 视口高度
     * @return 屏幕坐标（像素）
     */
    QVector2D worldToScreen(const QVector3D &worldPos,
                          const QMatrix4x4 &viewMatrix,
                          const QMatrix4x4 &projectionMatrix,
                          int width, int height);
  };
} // namespace ProGraphics
