#define GL_SILENCE_DEPRECATION
#include "prographics/core/graphics/primitive2d.h"
#include <QDebug>
#include <cmath>
#include <cstddef>
#include <limits>

namespace ProGraphics {
  QOpenGLBuffer *VertexBufferPool::acquire() {
    if (m_availableBuffers.empty()) {
      // 扩展缓冲池
      size_t oldSize = m_buffers.size();
      size_t newSize = oldSize == 0 ? INITIAL_POOL_SIZE : oldSize + GROWTH_FACTOR;
      for (size_t i = oldSize; i < newSize; ++i) {
        m_buffers.emplace_back(QOpenGLBuffer::VertexBuffer);
        m_buffers.back().create();
        m_availableBuffers.push(i);
      }
    }

    size_t index = m_availableBuffers.front();
    m_availableBuffers.pop();
    return &m_buffers[index];
  }

  void VertexBufferPool::release(QOpenGLBuffer *buffer) {
    auto it =
        std::find_if(m_buffers.begin(), m_buffers.end(),
                     [buffer](const QOpenGLBuffer &b) { return &b == buffer; });
    if (it != m_buffers.end()) {
      size_t index = std::distance(m_buffers.begin(), it);
      m_availableBuffers.push(index);
    }
  }

  void VertexBufferPool::cleanup() {
    for (auto &buffer: m_buffers) {
      if (buffer.isCreated()) {
        buffer.destroy();
      }
    }
    m_buffers.clear();
    std::queue<size_t>().swap(m_availableBuffers);
  }

  VertexBufferPool::~VertexBufferPool() { cleanup(); }

  // 静态成员初始化
  std::unique_ptr<QOpenGLShaderProgram> Primitive2D::s_shaderProgram;
  int Primitive2D::s_shaderUsers = 0;

  void Primitive2D::initializeShader() {
    if (!s_shaderProgram) {
      s_shaderProgram = std::make_unique<QOpenGLShaderProgram>();

      // 顶点着色器
      const char *vertexShaderSource = R"(
           #version 410 core
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec4 aColor;
            layout (location = 2) in vec4 instancePositionScale;
            layout (location = 3) in vec2 instanceRotation;
            layout (location = 4) in vec4 instanceColor;

            uniform mat4 projection;
            uniform mat4 view;
            uniform bool useInstancing;
            uniform float pointSize;
            uniform float uAlphaReplace;

            out vec4 vertexColor;

            void main() {
                vec3 position = aPos;
                if (useInstancing) {
                    vec2 scaled = aPos.xy * instancePositionScale.zw;
                    position.xy = vec2(
                        instanceRotation.x * scaled.x - instanceRotation.y * scaled.y,
                        instanceRotation.y * scaled.x + instanceRotation.x * scaled.y
                    ) + instancePositionScale.xy;
                }
                gl_Position = projection * view * vec4(position, 1.0);
                gl_PointSize = pointSize;
                vec4 c = useInstancing ? instanceColor : aColor;
                if (uAlphaReplace >= 0.0) {
                    c.a = uAlphaReplace;
                }
                vertexColor = c;
            }
        )";

      // 片段着色器
      const char *fragmentShaderSource = R"(
            #version 410 core
            in vec4 vertexColor;
            out vec4 FragColor;

            void main() {
                FragColor = vertexColor;
            }
        )";

      s_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                               vertexShaderSource);
      s_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                               fragmentShaderSource);
      s_shaderProgram->link();
    }
    s_shaderUsers++;
  }

  void Primitive2D::releaseShader() {
    s_shaderUsers--;
    if (s_shaderUsers == 0) {
      s_shaderProgram.reset();
    }
  }

  Primitive2D::Primitive2D()
    : m_visible(true), m_color(1.0f, 1.0f, 1.0f, 1.0f), m_vertexCount(0) {
    initializeOpenGLFunctions();
    initializeShader();
  }

  Primitive2D::~Primitive2D() {
    if (m_managedVBO) {
      VertexBufferPool::getInstance().release(m_managedVBO);
      m_managedVBO = nullptr;
    }
    destroy();
    releaseShader();
  }

  void Primitive2D::draw(const QMatrix4x4 &projection, const QMatrix4x4 &view) {
    if (!m_visible || !s_shaderProgram)
      return;
    if (m_isDirty) {
      updateVertexData();
    }
    s_shaderProgram->bind();
    s_shaderProgram->setUniformValue("projection", projection);
    s_shaderProgram->setUniformValue("view", view);
    s_shaderProgram->setUniformValue("pointSize", m_style.pointSize);
    s_shaderProgram->setUniformValue("useInstancing", false);
    s_shaderProgram->setUniformValue("uAlphaReplace", -1.0f);
    glEnable(GL_PROGRAM_POINT_SIZE);
    m_vao.bind();
    if (m_useIndices) {
      glDrawElements(getPrimitiveType(), m_indexCount, GL_UNSIGNED_INT, 0);
    } else {
      glDrawArrays(getPrimitiveType(), 0, m_vertexCount);
    }
    glDisable(GL_PROGRAM_POINT_SIZE);
    m_vao.release();
  }


  void Primitive2D::initializeInstanceBuffer() {
    initializeOpenGLFunctions();

    if (!m_vao.isCreated()) {
      m_vao.create();
    }
    m_vao.bind();

    // 创建实例化缓冲
    if (!m_instanceVBO.isCreated()) {
      m_instanceVBO.create();
      m_instanceVBO.setUsagePattern(QOpenGLBuffer::StreamDraw);
    }
    m_instanceVBO.bind();

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                          reinterpret_cast<void *>(offsetof(InstanceData, positionScale)));
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                          reinterpret_cast<void *>(offsetof(InstanceData, rotation)));
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                          reinterpret_cast<void *>(offsetof(InstanceData, color)));
    glVertexAttribDivisor(4, 1);

    m_instanceVBO.release();
    m_vao.release();
    m_instancedMode = true;
  }

  bool Primitive2D::updateInstanceData(const std::vector<Transform2D> &transforms) {
    if (!m_instancedMode) return false;

    m_instanceData.resize(transforms.size());
    for (size_t i = 0; i < transforms.size(); ++i) {
      const auto &transform = transforms[i];
      auto &data = m_instanceData[i];
      data.positionScale[0] = transform.position.x();
      data.positionScale[1] = transform.position.y();
      data.positionScale[2] = transform.scale.x();
      data.positionScale[3] = transform.scale.y();
      data.rotation[0] = transform.rotation == 0.0f ? 1.0f : std::cos(transform.rotation);
      data.rotation[1] = transform.rotation == 0.0f ? 0.0f : std::sin(transform.rotation);
      data.color[0] = transform.color.x();
      data.color[1] = transform.color.y();
      data.color[2] = transform.color.z();
      data.color[3] = transform.color.w();
    }

    if (!m_instanceVBO.bind()) return false;
    m_instanceVBO.allocate(m_instanceData.data(), static_cast<int>(m_instanceData.size() * sizeof(InstanceData)));
    m_instanceVBO.release();
    return true;
  }

  void Primitive2D::drawInstanced(const QMatrix4x4 &projection, const QMatrix4x4 &view,
                                  const std::vector<Transform2D> &transforms,
                                  float alphaReplace,
                                  bool uploadInstanceData) {
    if (!m_visible || !s_shaderProgram || transforms.empty()) return;
    if (transforms.size() > static_cast<size_t>(std::numeric_limits<int>::max()) / sizeof(InstanceData)) {
      qWarning() << "2D 实例缓冲区超出支持范围:" << transforms.size();
      return;
    }

    if (!m_instancedMode) {
      initializeInstanceBuffer();
      uploadInstanceData = true;
    }
    if (uploadInstanceData && !updateInstanceData(transforms)) {
      return;
    }

    s_shaderProgram->bind();
    s_shaderProgram->setUniformValue("projection", projection);
    s_shaderProgram->setUniformValue("view", view);
    s_shaderProgram->setUniformValue("useInstancing", true);
    s_shaderProgram->setUniformValue("pointSize", m_style.pointSize);
    s_shaderProgram->setUniformValue("uAlphaReplace", alphaReplace);

    m_vao.bind();
    glEnable(GL_PROGRAM_POINT_SIZE);
    glDrawArraysInstanced(getPrimitiveType(), 0, m_vertexCount, static_cast<GLsizei>(transforms.size()));
    glDisable(GL_PROGRAM_POINT_SIZE);
    m_vao.release();
    s_shaderProgram->setUniformValue("useInstancing", false);
    s_shaderProgram->setUniformValue("uAlphaReplace", -1.0f);

    s_shaderProgram->release();
  }

  void Primitive2D::updateVertexData() {
    if (!m_isDirty)
      return;

    std::vector<float> vertices;
    generateVertices(vertices);
    setupBuffer(vertices, 7);
    m_isDirty = false;
  }

  void Primitive2D::destroy() {
    if (m_managedVBO) {
      VertexBufferPool::getInstance().release(m_managedVBO);
      m_managedVBO = nullptr;
    }
    if (m_vao.isCreated()) {
      m_vao.destroy();
    }
  }

  void Primitive2D::setupBuffer(const std::vector<float> &vertices, int stride) {
    if (!m_vao.isCreated()) {
      m_vao.create();
    }
    m_vao.bind();

    // 使用缓冲池获取VBO
    if (!m_managedVBO) {
      m_managedVBO = VertexBufferPool::getInstance().acquire();
    }

    m_managedVBO->bind();
    m_managedVBO->allocate(vertices.data(), vertices.size() * sizeof(float));

    // 设置顶点属性
    // 位置
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), 0);
    f->glEnableVertexAttribArray(0);

    // 颜色
    f->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride * sizeof(float),
                             reinterpret_cast<void *>(3 * sizeof(float)));
    f->glEnableVertexAttribArray(1);

    m_managedVBO->release();
    m_vao.release();

    m_cachedVertices = vertices;
    m_isDirty = false;
  }

  void Primitive2D::setupIndexBuffer(const std::vector<GLuint> &indices) {
    if (!m_vao.isCreated()) {
      m_vao.create();
    }
    m_vao.bind();

    if (!m_ibo.isCreated()) {
      m_ibo.create();
    }
    m_ibo.bind();
    m_ibo.allocate(indices.data(), indices.size() * sizeof(GLuint));
    m_indexCount = indices.size();
    m_useIndices = true;

    m_ibo.release();
    m_vao.release();
  }

  void Primitive2D::addVertex(std::vector<float> &vertices,
                              const QVector3D &pos) const {
    vertices.insert(vertices.end(), {pos.x(), pos.y(), pos.z()});
  }

  void Primitive2D::addColoredVertex(std::vector<float> &vertices,
                                     const QVector3D &pos,
                                     const QVector4D &color) const {
    vertices.insert(vertices.end(), {
                      pos.x(), pos.y(), pos.z(), color.x(),
                      color.y(), color.z(), color.w()
                    });
  }

  void Primitive2D::addToRenderBatch(Primitive2DBatch &batch) {
    std::vector<float> vertices;
    generateVertices(vertices);
    batch.add(vertices, m_vertexCount, getPrimitiveType());
  }

  // 批量渲染实现
  void Primitive2DBatch::begin() {
    m_items.clear();
    m_drawRanges.clear();
    // 启用深度测试但禁用深度写入，解决透明度问题
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    // 设置混合模式
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }

  void Primitive2DBatch::add(const std::vector<float> &vertices, int vertexCount,
                             GLenum primitiveType) {
    BatchItem item;
    item.vertices = vertices;
    item.vertexCount = vertexCount;
    item.primitiveType = primitiveType;
    m_items.push_back(item);
  }

  void Primitive2DBatch::end() {
    m_drawRanges.clear();
    // 计算总顶点数
    size_t totalVertices = 0;
    for (const auto &item: m_items) {
      totalVertices += item.vertices.size();
    }

    // 合并所有顶点数据
    std::vector<float> batchedVertices;
    batchedVertices.reserve(totalVertices);
    GLint first = 0;
    for (const auto &item: m_items) {
      batchedVertices.insert(batchedVertices.end(), item.vertices.begin(),
                             item.vertices.end());
      // 只合并相邻、完整的独立图元，保持透明叠色顺序；条带和环不能跨条目连接。
      const int groupSize = item.primitiveType == GL_POINTS ? 1 :
          item.primitiveType == GL_LINES && m_style.lineStyle == Qt::SolidLine ? 2 :
          item.primitiveType == GL_TRIANGLES ? 3 : 0;
      if (item.vertexCount > 0) {
        if (groupSize && item.vertexCount % groupSize == 0 && !m_drawRanges.empty() &&
            m_drawRanges.back().primitiveType == item.primitiveType &&
            m_drawRanges.back().count % groupSize == 0) {
          m_drawRanges.back().count += item.vertexCount;
        } else {
          m_drawRanges.push_back({item.primitiveType, first, item.vertexCount});
        }
      }
      first += item.vertexCount;
    }

    // 创建并填充VBO
    if (!m_batchVAO.isCreated()) {
      m_batchVAO.create();
    }
    m_batchVAO.bind();

    if (!m_batchVBO.isCreated()) {
      m_batchVBO.create();
    }
    m_batchVBO.bind();
    m_batchVBO.allocate(batchedVertices.data(),
                        batchedVertices.size() * sizeof(float));

    // 设置顶点属性
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), 0);
    f->glEnableVertexAttribArray(0);
    f->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float),
                             reinterpret_cast<void *>(3 * sizeof(float)));
    f->glEnableVertexAttribArray(1);

    m_batchVBO.release();
    m_batchVAO.release();
    // 恢复深度写入
    glDepthMask(GL_TRUE);
  }

  void Primitive2DBatch::draw(const QMatrix4x4 &projection,
                              const QMatrix4x4 &view) {
    if (m_drawRanges.empty())
      return;

    // 确保正确的深度测试和混合设置
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Primitive2D::s_shaderProgram->bind();
    Primitive2D::s_shaderProgram->setUniformValue("projection", projection);
    Primitive2D::s_shaderProgram->setUniformValue("view", view);
    Primitive2D::s_shaderProgram->setUniformValue("pointSize", m_style.pointSize);
    Primitive2D::s_shaderProgram->setUniformValue("useInstancing", false);
    Primitive2D::s_shaderProgram->setUniformValue("uAlphaReplace", -1.0f);

    m_batchVAO.bind();
    for (const auto &item: m_drawRanges) {
      // 设置线宽（如果是线段）
      if (item.primitiveType == GL_LINES) {
        if (m_style.lineStyle != Qt::SolidLine) {
          GLint factor = 1;
          GLushort pattern = 0;
          switch (m_style.lineStyle) {
            case Qt::DashLine:
              pattern = 0x00FF;
              break;
            case Qt::DotLine:
              pattern = 0x0101;
              break;
            case Qt::DashDotLine:
              pattern = 0x1C47;
              break;
            default:
              break;
          }
          if (pattern != 0) {
            glEnable(GL_LINE_STIPPLE);
            glLineStipple(factor, pattern);
          }
        }
        glLineWidth(m_style.lineWidth);
      }

      glDrawArrays(item.primitiveType, item.first, item.count);

      // 恢复默认线宽
      if (item.primitiveType == GL_LINES) {
        glLineWidth(1.0f);
      }
    }
    m_batchVAO.release();
    Primitive2D::s_shaderProgram->release();

    // 恢复状态
    glDepthMask(GL_TRUE);
  }

  void Primitive2DGroup::addToRenderBatch(Primitive2DBatch &batch) {
    for (auto &primitive: m_primitives) {
      primitive->addToRenderBatch(batch);
    }
  }

  // Line实现
  Line2D::Line2D(const QVector3D &start, const QVector3D &end,
                 const QVector4D &color)
    : m_start(start), m_end(end) {
    m_color = color;
    m_vertexCount = 2;
  }

  void Line2D::generateVertices(std::vector<float> &vertices) {
    vertices.clear();
    if (!m_points.empty()) {
      // 批量线段模式
      vertices.reserve(m_points.size() * 7); // 每个点7个浮点数
      for (const auto &point: m_points) {
        addColoredVertex(vertices, point, m_color);
      }
      m_vertexCount = m_points.size();
    } else {
      // 单条线段模式
      vertices.reserve(14); // 2个顶点 * 7个浮点数
      addColoredVertex(vertices, m_start, m_color);
      addColoredVertex(vertices, m_end, m_color);
      m_vertexCount = 2;
    }
  }

  void Line2D::generateIndices(std::vector<GLuint> &indices) {
    indices.clear();
    if (!m_points.empty()) {
      // 为每对点生成索引
      indices.reserve((m_points.size() / 2) * 2);
      for (GLuint i = 0; i < m_points.size(); i += 2) {
        indices.push_back(i);
        indices.push_back(i + 1);
      }
    }
  }

  void Line2D::draw(const QMatrix4x4 &projection, const QMatrix4x4 &view) {
    if (!m_visible || !s_shaderProgram)
      return;
    if (m_isDirty) {
      updateVertexData();
    }

    glLineWidth(m_style.lineWidth);

    s_shaderProgram->bind();
    s_shaderProgram->setUniformValue("projection", projection);
    s_shaderProgram->setUniformValue("view", view);
    s_shaderProgram->setUniformValue("pointSize", m_style.pointSize);
    s_shaderProgram->setUniformValue("useInstancing", false);
    s_shaderProgram->setUniformValue("uAlphaReplace", -1.0f);

    m_vao.bind();
    glDrawArrays(GL_LINES, 0, m_vertexCount);
    m_vao.release();

    glLineWidth(1.0f);
    s_shaderProgram->release();
  }

  void Line2D::setPoints(const QVector3D &start, const QVector3D &end) {
    if (m_start == start && m_end == end) return;
    m_start = start;
    m_end = end;
    markDirty();
  }

  void Line2D::setLines(const std::vector<QVector3D> &points) {
    m_points = points;
    markDirty();
  }

  void Line2D::addToRenderBatch(Primitive2DBatch &batch) {
    std::vector<float> vertices;
    generateVertices(vertices);
    batch.add(vertices, 2, GL_LINES);
  }

  // Point2D实现
  Point2D::Point2D(const QVector3D &position, const QVector4D &color, float size)
    : m_position(position), m_size(size) {
    m_color = color;
    m_vertexCount = 1;
  }

  void Point2D::generateVertices(std::vector<float> &vertices) {
    vertices.clear();
    vertices.reserve(7); // 1 vertex * 7 floats
    addColoredVertex(vertices, m_position, m_color);
  }

  void Point2D::draw(const QMatrix4x4 &projection, const QMatrix4x4 &view) {
    if (!m_visible || !s_shaderProgram)
      return;

    if (m_isDirty) {
      updateVertexData();
    }

    s_shaderProgram->bind();
    s_shaderProgram->setUniformValue("projection", projection);
    s_shaderProgram->setUniformValue("view", view);
    s_shaderProgram->setUniformValue("pointSize", m_size); // 使用特定的点大小
    s_shaderProgram->setUniformValue("useInstancing", false);
    s_shaderProgram->setUniformValue("uAlphaReplace", -1.0f);
    glEnable(GL_PROGRAM_POINT_SIZE);

    m_vao.bind();
    glDrawArrays(GL_POINTS, 0, m_vertexCount);
    glDisable(GL_PROGRAM_POINT_SIZE);
    m_vao.release();

    s_shaderProgram->release();
  }

  void Point2D::setPosition(const QVector3D &position) {
    m_position = position;
    markDirty();
  }

  void Point2D::setSize(float size) { m_size = size; }

  void Point2D::addToRenderBatch(Primitive2DBatch &batch) {
    Primitive2DStyle batchStyle = batch.style();
    batchStyle.pointSize = m_size; // 使用点的特定大小
    batch.setStyle(batchStyle);
    std::vector<float> vertices = {
      m_position.x(), m_position.y(), m_position.z(),
      m_color.x(), m_color.y(), m_color.z(),
      m_color.w()
    };
    batch.add(vertices, 1, GL_POINTS);
  }

  Triangle2D::Triangle2D(const QVector3D &p1, const QVector3D &p2,
                         const QVector3D &p3, const QVector4D &color)
    : m_p1(p1), m_p2(p2), m_p3(p3) {
    m_color = color;
    m_vertexCount = 3;
  }

  void Triangle2D::generateVertices(std::vector<float> &vertices) {
    vertices.clear();
    vertices.reserve(21); // 3 vertices * 7 floats each
    addColoredVertex(vertices, m_p1, m_color);
    addColoredVertex(vertices, m_p2, m_color);
    addColoredVertex(vertices, m_p3, m_color);
  }

  void Triangle2D::setPoints(const QVector3D &p1, const QVector3D &p2,
                             const QVector3D &p3) {
    m_p1 = p1;
    m_p2 = p2;
    m_p3 = p3;
    markDirty();
  }

  void Triangle2D::addToRenderBatch(Primitive2DBatch &batch) {
    std::vector<float> vertices = {
      m_p1.x(), m_p1.y(), m_p1.z(), m_color.x(), m_color.y(),
      m_color.z(), m_color.w(), m_p2.x(), m_p2.y(), m_p2.z(),
      m_color.x(), m_color.y(), m_color.z(), m_color.w(), m_p3.x(),
      m_p3.y(), m_p3.z(), m_color.x(), m_color.y(), m_color.z(),
      m_color.w()
    };
    batch.add(vertices, 3, GL_TRIANGLES);
  }

  // Rectangle2D实现
  Rectangle2D::Rectangle2D(const QVector3D &center, float width, float height,
                           const QVector4D &color)
    : m_center(center), m_width(width), m_height(height) {
    m_color = color;
    m_vertexCount = 6;
  }

  void Rectangle2D::generateVertices(std::vector<float> &vertices) {
    vertices.clear();
    vertices.reserve(42); // 6 vertices * 7 floats each

    float halfWidth = m_width / 2.0f;
    float halfHeight = m_height / 2.0f;

    // 计算四个角点
    QVector3D bottomLeft(m_center.x() - halfWidth, m_center.y() - halfHeight,
                         m_center.z());
    QVector3D bottomRight(m_center.x() + halfWidth, m_center.y() - halfHeight,
                          m_center.z());
    QVector3D topRight(m_center.x() + halfWidth, m_center.y() + halfHeight,
                       m_center.z());
    QVector3D topLeft(m_center.x() - halfWidth, m_center.y() + halfHeight,
                      m_center.z());

    // 第一个三角形
    addColoredVertex(vertices, bottomLeft, m_color);
    addColoredVertex(vertices, bottomRight, m_color);
    addColoredVertex(vertices, topRight, m_color);

    // 第二个三角形
    addColoredVertex(vertices, bottomLeft, m_color);
    addColoredVertex(vertices, topRight, m_color);
    addColoredVertex(vertices, topLeft, m_color);
  }

  void Rectangle2D::setDimensions(float width, float height) {
    m_width = width;
    m_height = height;
    markDirty();
  }

  void Rectangle2D::setCenter(const QVector3D &center) {
    m_center = center;
    markDirty();
  }

  void Rectangle2D::addToRenderBatch(Primitive2DBatch &batch) {
    std::vector<float> vertices;
    generateVertices(vertices);
    batch.add(vertices, 6, GL_TRIANGLES);
  }

  // Circle2D实现
  Circle2D::Circle2D(const QVector3D &center, float radius, int segments,
                     const QVector4D &color)
    : m_center(center), m_radius(radius), m_segments(segments) {
    m_color = color;
    m_vertexCount = m_segments + 2;
  }

  void Circle2D::generateVertices(std::vector<float> &vertices) {
    vertices.clear();
    vertices.reserve((m_segments + 2) * 7);

    // 添加中心点
    addColoredVertex(vertices, m_center, m_color);

    // 添加圆周点
    for (int i = 0; i <= m_segments; ++i) {
      float angle = 2.0f * M_PI * i / m_segments;
      QVector3D point(m_center.x() + m_radius * cos(angle),
                      m_center.y() + m_radius * sin(angle), m_center.z());
      addColoredVertex(vertices, point, m_color);
    }
  }

  void Circle2D::setRadius(float radius) {
    m_radius = radius;
    markDirty();
  }

  void Circle2D::setCenter(const QVector3D &center) {
    m_center = center;
    markDirty();
  }

  void Circle2D::setSegments(int segments) {
    m_segments = segments;
    markDirty();
  }

  void Circle2D::addToRenderBatch(Primitive2DBatch &batch) {
    std::vector<float> vertices;
    vertices.reserve((m_segments + 2) * 7);

    // 添加中心点
    vertices.insert(vertices.end(),
                    {
                      m_center.x(), m_center.y(), m_center.z(), m_color.x(),
                      m_color.y(), m_color.z(), m_color.w()
                    });

    // 添加圆周点
    for (int i = 0; i <= m_segments; ++i) {
      float angle = 2.0f * M_PI * i / m_segments;
      float x = m_center.x() + m_radius * cos(angle);
      float y = m_center.y() + m_radius * sin(angle);

      vertices.insert(vertices.end(), {
                        x, y, m_center.z(), m_color.x(),
                        m_color.y(), m_color.z(), m_color.w()
                      });
    }
    batch.add(vertices, m_segments + 2, GL_TRIANGLE_FAN);
  }
} // namespace ProGraphics
