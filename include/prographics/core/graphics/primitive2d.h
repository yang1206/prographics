#pragma once
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <queue>

namespace ProGraphics {
  /**
 * @brief OpenGL顶点缓冲区池
 *
 * 管理和复用OpenGL顶点缓冲区对象(VBO)，避免频繁创建和销毁
 */
  class VertexBufferPool {
  public:
    /**
    * @brief 获取单例实例
    */
    static VertexBufferPool &getInstance() {
      static VertexBufferPool instance;
      return instance;
    }

    /**
   * @brief 获取一个可用的缓冲区
   * @return 可用的OpenGL缓冲区指针
   */
    QOpenGLBuffer *acquire();

    /**
  * @brief 释放一个缓冲区回池中
  * @param buffer 要释放的缓冲区指针
  */
    void release(QOpenGLBuffer *buffer);

    /**
   * @brief 清理所有缓冲区
   */
    void cleanup(); // 清理所有缓冲区

  private:
    VertexBufferPool() = default;

    ~VertexBufferPool();

    // 禁用拷贝
    VertexBufferPool(const VertexBufferPool &) = delete;

    VertexBufferPool &operator=(const VertexBufferPool &) = delete;

    std::vector<QOpenGLBuffer> m_buffers;
    std::queue<size_t> m_availableBuffers;
    static constexpr size_t INITIAL_POOL_SIZE = 32;
    static constexpr size_t GROWTH_FACTOR = 16;
  }; // namespace class VertexBufferPool

  /**
 * @brief 2D图元的顶点属性结构
 */
  struct Primitive2DAttribute {
    QVector3D position; ///< 顶点位置
    QVector4D color; ///< 顶点颜色（RGBA）
  };

  /**
 * @brief 2D图元的样式结构
 */
  struct Primitive2DStyle {
    float lineWidth = 1.0f; ///< 线宽
    float pointSize = 1.0f; ///< 点大小
    Qt::PenStyle lineStyle = Qt::SolidLine; ///< 线型
    bool filled = true; ///< 是否填充
    QVector4D borderColor = QVector4D(0.0f, 0.0f, 0.0f, 1.0f); ///< 边框颜色
    float borderWidth = 0.0f; ///< 边框宽度
  };


  /**
 * @brief 2D图元批量渲染管理器
 *
 * 通过批处理技术优化渲染性能
 */
  class Primitive2DBatch {
  public:
    Primitive2DBatch() = default;

    ~Primitive2DBatch() {
      if (m_batchVBO.isCreated()) {
        m_batchVBO.destroy();
      }
      if (m_batchVAO.isCreated()) {
        m_batchVAO.destroy();
      }
    }

    /**
     * @brief 开始批处理
     */
    void begin();

    /**
     * @brief 添加图元到批处理
     * @param vertices 顶点数据
     * @param vertexCount 顶点数量
     * @param primitiveType 图元类型
     */
    void add(const std::vector<float> &vertices, int vertexCount,
             GLenum primitiveType);

    /**
     * @brief 结束批处理
     */
    void end();

    /**
     * @brief 绘制批处理中的所有图元
     * @param projection 投影矩阵
     * @param view 视图矩阵
     */
    void draw(const QMatrix4x4 &projection, const QMatrix4x4 &view);

    void setStyle(const Primitive2DStyle &style) { m_style = style; }
    const Primitive2DStyle &style() const { return m_style; }

  private:
    struct BatchItem {
      std::vector<float> vertices;
      int vertexCount;
      GLenum primitiveType;
    };

    std::vector<BatchItem> m_items;
    QOpenGLBuffer m_batchVBO;
    QOpenGLVertexArrayObject m_batchVAO;
    Primitive2DStyle m_style;
  };


  /**
 * @brief 2D图形的变换结构
 */
  struct Transform2D {
    QVector2D position{0.0f, 0.0f}; // 位置
    float rotation = 0.0f; // 旋转角度(弧度)
    QVector2D scale{1.0f, 1.0f}; // 缩放
    QVector4D color{1.0f, 1.0f, 1.0f, 1.0f};

    QMatrix4x4 getMatrix() const {
      QMatrix4x4 matrix;
      matrix.translate(QVector3D(position.x(), position.y(), 0.0f));
      matrix.rotate(qRadiansToDegrees(rotation), QVector3D(0.0f, 0.0f, 1.0f));
      matrix.scale(QVector3D(scale.x(), scale.y(), 1.0f));
      return matrix;
    }
  };

  /**
 * @brief 2D图元基类
 *
 * 提供基础的2D图元功能，包括：
 * - 顶点数据管理
 * - OpenGL渲染
 * - 批处理支持
 * - 样式配置
 */
  class Primitive2D : protected QOpenGLExtraFunctions {
  public:
    Primitive2D();

    virtual ~Primitive2D();

    /**
     * @brief 初始化图元
     * 必须在OpenGL上下文中调用
     */
    virtual void initialize() { updateVertexData(); }

    /**
     * @brief 绘制图元
     * @param projection 投影矩阵
     * @param view 视图矩阵
     */
    virtual void draw(const QMatrix4x4 &projection, const QMatrix4x4 &view);

    /**
     * @brief 使用实例化渲染绘制多个实例
     * @param projection 投影矩阵
     * @param view 视图矩阵
     * @param transforms 变换数组
     */
    void drawInstanced(const QMatrix4x4 &projection, const QMatrix4x4 &view,
                       const std::vector<Transform2D> &transforms);

    /**
     * @brief 销毁图元
     */
    virtual void destroy();

    /**
     * @brief 添加到批处理渲染器
     * @param batch 批处理渲染器
     */
    virtual void addToRenderBatch(Primitive2DBatch &batch);

    // 设置和获取属性
    void setVisible(bool visible) { m_visible = visible; }
    bool isVisible() const { return m_visible; }

    void setColor(const QVector4D &color) {
      m_color = color;
      markDirty();
    }

    QVector4D color() const { return m_color; }

    void setStyle(const Primitive2DStyle &style) {
      m_style = style;
      markDirty();
    }

    const Primitive2DStyle &style() const { return m_style; }

    /**
     * @brief 获取顶点数据
     * @param vertices 用于存储顶点数据的向量
     */
    virtual void getVertexData(std::vector<float> &vertices) {
      generateVertices(vertices);
    }

  protected:
    /**
     * @brief 生成图元的顶点数据
     * @param vertices 用于存储生成的顶点数据的向量
     */
    virtual void generateVertices(std::vector<float> &vertices) = 0;

    /**
     * @brief 生成图元的索引数据
     * @param indices 用于存储生成的索引数据的向量
     */
    virtual void generateIndices(std::vector<GLuint> &indices) {
        (void)indices;
    }

    /**
     * @brief 获取图元类型
     * @return OpenGL图元类型
     */
    virtual GLenum getPrimitiveType() const = 0;

    void updateVertexData();

    // 设置顶点缓冲
    void setupBuffer(const std::vector<float> &vertices, int stride);

    void setupIndexBuffer(const std::vector<GLuint> &indices);

    void markDirty() { m_isDirty = true; }

    // 顶点数据辅助方法
    void addVertex(std::vector<float> &vertices, const QVector3D &pos) const;

    void addColoredVertex(std::vector<float> &vertices, const QVector3D &pos,
                          const QVector4D &color) const;

    // 着色器管理
    void initializeShader();

    void releaseShader();

    void initializeInstanceBuffer();

    void updateInstanceData(const std::vector<Transform2D> &transforms);

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer *m_managedVBO = nullptr;
    QOpenGLBuffer m_ibo{QOpenGLBuffer::IndexBuffer};
    int m_indexCount = 0;
    bool m_useIndices = false;

    bool m_visible;
    QVector4D m_color;
    Primitive2DStyle m_style;
    int m_vertexCount;
    bool m_isDirty = true;
    std::vector<float> m_cachedVertices;

    // 共享的着色器程序
    static std::unique_ptr<QOpenGLShaderProgram> s_shaderProgram;
    static int s_shaderUsers; // 引用计数

    QOpenGLBuffer m_instanceVBO; // 实例化缓冲
    bool m_instancedMode = false; // 是否启用实例化模式

    friend class Primitive2DBatch;
  };

  /**
 * @brief 2D图元组
 *
 * 用于管理多个2D图元的组合
 */
  class Primitive2DGroup : public Primitive2D {
  public:
    Primitive2DGroup() = default;

    void addPrimitive(std::shared_ptr<Primitive2D> primitive) {
      m_primitives.push_back(primitive);
    }

    void removePrimitive(std::shared_ptr<Primitive2D> primitive) {
      auto it = std::find(m_primitives.begin(), m_primitives.end(), primitive);
      if (it != m_primitives.end()) {
        m_primitives.erase(it);
      }
    }

    void initialize() override {
      for (auto &primitive: m_primitives) {
        primitive->initialize();
      }
    }

    void draw(const QMatrix4x4 &projection, const QMatrix4x4 &view) override {
      if (!m_visible)
        return;
      for (auto &primitive: m_primitives) {
        primitive->draw(projection, view);
      }
    }

    void addToRenderBatch(Primitive2DBatch &batch) override;

  protected:
    // 实现抽象方法
    void generateVertices(std::vector<float> &vertices) override {
      vertices.clear();
      for (auto &primitive: m_primitives) {
        std::vector<float> primitiveVertices;
        primitive->getVertexData(primitiveVertices); // 使用新的公共接口
        vertices.insert(vertices.end(), primitiveVertices.begin(),
                        primitiveVertices.end());
      }
    }

    GLenum getPrimitiveType() const override {
      return GL_TRIANGLES; // 默认类型，实际绘制时会使用各个子图元的类型
    }

  private:
    std::vector<std::shared_ptr<Primitive2D> > m_primitives;
  };


  /**
 * @brief 2D线段图元
 */
  class Line2D : public Primitive2D {
  public:
    Line2D(const QVector3D &start = QVector3D(),
           const QVector3D &end = QVector3D(),
           const QVector4D &color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    void draw(const QMatrix4x4 &projection, const QMatrix4x4 &view) override;

    void setPoints(const QVector3D &start, const QVector3D &end);

    void setLines(const std::vector<QVector3D> &points);

    const QVector3D &start() const { return m_start; }
    const QVector3D &end() const { return m_end; }

    void addToRenderBatch(Primitive2DBatch &batch) override;

  protected:
    // 实现基类虚函数
    void generateVertices(std::vector<float> &vertices) override;

    void generateIndices(std::vector<GLuint> &indices) override;

    GLenum getPrimitiveType() const override { return GL_LINES; }

  private:
    QVector3D m_start;
    QVector3D m_end;
    std::vector<QVector3D> m_points;
    QOpenGLBuffer m_ibo{QOpenGLBuffer::IndexBuffer};
  };


  /**
 * @brief 2D点图元
 */
  class Point2D : public Primitive2D {
  public:
    Point2D(const QVector3D &position = QVector3D(),
            const QVector4D &color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f),
            float size = 1.0f);

    void setPosition(const QVector3D &position);

    void setSize(float size);

    const QVector3D &position() const { return m_position; }
    float size() const { return m_size; }

    void addToRenderBatch(Primitive2DBatch &batch) override;

  protected:
    // 实现基类虚函数
    void generateVertices(std::vector<float> &vertices) override;

    GLenum getPrimitiveType() const override { return GL_POINTS; }

    void draw(const QMatrix4x4 &projection, const QMatrix4x4 &view) override;

  private:
    QVector3D m_position;
    float m_size;
  };

  class Triangle2D : public Primitive2D {
  public:
    Triangle2D(const QVector3D &p1 = QVector3D(),
               const QVector3D &p2 = QVector3D(),
               const QVector3D &p3 = QVector3D(),
               const QVector4D &color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    void setPoints(const QVector3D &p1, const QVector3D &p2, const QVector3D &p3);

    const QVector3D &p1() const { return m_p1; }
    const QVector3D &p2() const { return m_p2; }
    const QVector3D &p3() const { return m_p3; }

    void addToRenderBatch(Primitive2DBatch &batch) override;

  protected:
    // 实现基类虚函数
    void generateVertices(std::vector<float> &vertices) override;

    GLenum getPrimitiveType() const override { return GL_TRIANGLES; }

  private:
    QVector3D m_p1, m_p2, m_p3;
  };

  /**
 * @brief 2D矩形图元
 */
  class Rectangle2D : public Primitive2D {
  public:
    Rectangle2D(const QVector3D &center = QVector3D(), float width = 1.0f,
                float height = 1.0f,
                const QVector4D &color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    void setDimensions(float width, float height);

    void setCenter(const QVector3D &center);

    const QVector3D &center() const { return m_center; }
    float width() const { return m_width; }
    float height() const { return m_height; }

    void addToRenderBatch(Primitive2DBatch &batch) override;

  protected:
    // 实现基类虚函数
    void generateVertices(std::vector<float> &vertices) override;

    GLenum getPrimitiveType() const override { return GL_TRIANGLES; }

  private:
    QVector3D m_center;
    float m_width;
    float m_height;
  };


  /**
 * @brief 2D圆形图元
 */
  class Circle2D : public Primitive2D {
  public:
    Circle2D(const QVector3D &center = QVector3D(), float radius = 1.0f,
             int segments = 32,
             const QVector4D &color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    float radius() const { return m_radius; }
    const QVector3D &center() const { return m_center; }
    int segments() const { return m_segments; }

    void setRadius(float radius);

    void setCenter(const QVector3D &center);

    void setSegments(int segments);

    void addToRenderBatch(Primitive2DBatch &batch) override;

  protected:
    void generateVertices(std::vector<float> &vertices) override;

    GLenum getPrimitiveType() const override { return GL_TRIANGLE_FAN; }

  private:
    QVector3D m_center;
    float m_radius;
    int m_segments;
  };
} // namespace ProGraphics
