#include "3DCoordinate.h"
#include <QPainter>
#include <QTimer>
#include <random>

ThreeDCoordinate::ThreeDCoordinate(QWidget *parent) : BaseGLWidget(parent) {
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  m_camera = ProGraphics::Camera(ProGraphics::CameraType::Orbit,
                                 ProGraphics::ProjectionType::Perspective);
  m_camera.setPivotPoint(QVector3D(0, 0, 0));
  m_camera.zoom(-10.0f); // 设置初始距离，相当于之前的 m_radius = 15.0f
  m_camera.setFov(55.0f);
  m_camera.orbit(-65.0f, 75.0f);

  m_controls = std::make_unique<ProGraphics::OrbitControls>(&m_camera);
  connect(m_controls.get(), &ProGraphics::OrbitControls::updated, this,
          [this]() { update(); });

  connect(&m_prpsAnimationTimer, &QTimer::timeout, this,
          &ThreeDCoordinate::updatePRPSAnimation);
  m_prpsAnimationTimer.setInterval(16); // 约60fps

  connect(&m_generationTimer, &QTimer::timeout, this,
          &ThreeDCoordinate::createNewLineGroup);
  m_generationTimer.setInterval(100); // 每2秒生成一组新数据
  m_prpsAnimationSpeed = 0.03f;
}

ThreeDCoordinate::~ThreeDCoordinate() {
  makeCurrent();
  m_lineGroups.clear();
  cleanupComponent(m_axes);
  cleanupComponent(m_xyPlane);
  cleanupComponent(m_xzPlane);
  cleanupComponent(m_yzPlane);
  doneCurrent();
}

void ThreeDCoordinate::initializeGLObjects() {
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_LINE_SMOOTH);
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glEnable(GL_PROGRAM_POINT_SIZE);
  // 创建着色器程序
  m_program = new QOpenGLShaderProgram();
  m_program->addShaderFromSourceFile(QOpenGLShader::Vertex,
                                     ":/shaders/shaders/06_prps/vertex.glsl");

  m_program->addShaderFromSourceFile(QOpenGLShader::Fragment,
                                     ":/shaders/shaders/06_prps/fragment.glsl");

  if (!m_program->link()) {
    qDebug() << "Shader Program Link Error:" << m_program->log();
  }

  initializeAxes();
  initializeTicks();
  initializeGrids();

  initializeLabels();

  initializePlanes();

  createNewLineGroup();

  m_prpsAnimationTimer.start();
  m_generationTimer.start();

  m_xyPlane.setVisible(false);
  m_xzPlane.setVisible(false);
  m_yzPlane.setVisible(false);
}

void ThreeDCoordinate::initializeAxes() {
  const float axisLength = 8.0f;
  std::vector<float> vertices = {
      // 位置                // 颜色
      // X轴（红色）
      0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,       // 起点
      axisLength, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // 终点

      // Y轴（绿色）
      0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, axisLength, 0.0f, 0.0f,
      1.0f, 0.0f, 1.0f,

      // Z轴（蓝色）
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, axisLength, 0.0f,
      0.0f, 1.0f, 1.0f};

  setupComponent(m_axes, vertices);
}

void ThreeDCoordinate::initializeGrids() {
  const float size = 5.0f;
  const float step = 1.0f;         // 网格间距
  const float alpha_major = 0.5f;  // 主网格线透明度
  const float alpha_minor = 0.15f; // 次网格线透明度

  // 生成网格线顶点数据的辅助函数
  auto generateGridVertices = [alpha_major, alpha_minor](float size, float step,
                                                         const QVector3D &color,
                                                         const QVector3D &right,
                                                         const QVector3D &up) {
    std::vector<float> vertices;

    // 生成平行于right方向的线
    for (float i = 0; i <= size; i += step) {
      float alpha = (std::fmod(i, 1.0f) < 0.01f) ? alpha_major : alpha_minor;
      QVector3D start = up * i;
      QVector3D end = start + right * size;

      // 添加线段顶点
      vertices.insert(vertices.end(),
                      {start.x(), start.y(), start.z(), color.x(), color.y(),
                       color.z(), alpha, end.x(), end.y(), end.z(), color.x(),
                       color.y(), color.z(), alpha});
    }

    // 生成平行于up方向的线
    for (float i = 0; i <= size; i += step) {
      float alpha = (std::fmod(i, 1.0f) < 0.01f) ? alpha_major : alpha_minor;
      QVector3D start = right * i;
      QVector3D end = start + up * size;

      vertices.insert(vertices.end(),
                      {start.x(), start.y(), start.z(), color.x(), color.y(),
                       color.z(), alpha, end.x(), end.y(), end.z(), color.x(),
                       color.y(), color.z(), alpha});
    }

    return vertices;
  };

  // XY平面网格（蓝色）
  std::vector<float> xyVertices =
      generateGridVertices(size, step / 2, QVector3D(0.0f, 0.0f, 1.0f), // 蓝色
                           QVector3D(1.0f, 0.0f, 0.0f), // right方向
                           QVector3D(0.0f, 1.0f, 0.0f)  // up方向
      );
  setupComponent(m_xyGrid, xyVertices);

  // XZ平面网格（绿色）
  std::vector<float> xzVertices =
      generateGridVertices(size, step / 2, QVector3D(0.0f, 1.0f, 0.0f), // 绿色
                           QVector3D(1.0f, 0.0f, 0.0f), // right方向
                           QVector3D(0.0f, 0.0f, 1.0f)  // up方向
      );
  setupComponent(m_xzGrid, xzVertices);

  // YZ平面网格（红色）
  std::vector<float> yzVertices =
      generateGridVertices(size, step / 2, QVector3D(1.0f, 0.0f, 0.0f), // 红色
                           QVector3D(0.0f, 1.0f, 0.0f), // right方向
                           QVector3D(0.0f, 0.0f, 1.0f)  // up方向
      );
  setupComponent(m_yzGrid, yzVertices);
}

void ThreeDCoordinate::initializeTicks() {
  const float axisLength = 5.0f;
  const float tickSize = 0.1f; // 刻度线长度
  const float step = 1.0f;     // 刻度间距
  const float alpha = 0.8f;    // 刻度线透明度

  // 生成刻度线顶点的辅助函数
  auto generateTickVertices = [alpha](float axisLength, float tickSize,
                                      float step, const QVector3D &axisDir,
                                      const QVector3D &tickDir1,
                                      const QVector3D &tickDir2,
                                      const QVector3D &color) {
    std::vector<float> vertices;

    // 生成主刻度和子刻度
    for (float pos = 0; pos <= axisLength; pos += step) {
      // 确定刻度大小（整数刻度较大，小数刻度较小）
      float currentTickSize =
          (std::fmod(pos, 1.0f) < 0.01f) ? tickSize : tickSize * 0.5f;
      float currentAlpha =
          (std::fmod(pos, 1.0f) < 0.01f) ? alpha : alpha * 0.5f;

      // 计算刻度线的起点和终点
      QVector3D basePos = axisDir * pos;

      // 添加第一个方向的刻度线
      QVector3D start1 = basePos;
      QVector3D end1 = basePos + tickDir1 * currentTickSize;
      vertices.insert(vertices.end(),
                      {start1.x(), start1.y(), start1.z(), color.x(), color.y(),
                       color.z(), currentAlpha, end1.x(), end1.y(), end1.z(),
                       color.x(), color.y(), color.z(), currentAlpha});

      // 添加第二个方向的刻度线
      QVector3D start2 = basePos;
      QVector3D end2 = basePos + tickDir2 * currentTickSize;
      vertices.insert(vertices.end(),
                      {start2.x(), start2.y(), start2.z(), color.x(), color.y(),
                       color.z(), currentAlpha, end2.x(), end2.y(), end2.z(),
                       color.x(), color.y(), color.z(), currentAlpha});
    }

    return vertices;
  };

  // X轴刻度（红色）
  std::vector<float> xTickVertices = generateTickVertices(
      axisLength, tickSize, step / 2, QVector3D(1, 0, 0), // 轴方向
      QVector3D(0, 1, 0),                                 // 刻度方向1
      QVector3D(0, 0, 1),                                 // 刻度方向2
      QVector3D(1, 0, 0)                                  // 颜色
  );
  setupComponent(m_xTicks, xTickVertices);

  // Y轴刻度（绿色）
  std::vector<float> yTickVertices = generateTickVertices(
      axisLength, tickSize, step / 2, QVector3D(0, 1, 0), // 轴方向
      QVector3D(1, 0, 0),                                 // 刻度方向1
      QVector3D(0, 0, 1),                                 // 刻度方向2
      QVector3D(0, 1, 0)                                  // 颜色
  );
  setupComponent(m_yTicks, yTickVertices);

  // Z轴刻度（蓝色）
  std::vector<float> zTickVertices = generateTickVertices(
      axisLength, tickSize, step / 2, QVector3D(0, 0, 1), // 轴方向
      QVector3D(1, 0, 0),                                 // 刻度方向1
      QVector3D(0, 1, 0),                                 // 刻度方向2
      QVector3D(0, 0, 1)                                  // 颜色
  );
  setupComponent(m_zTicks, zTickVertices);
}

void ThreeDCoordinate::initializePlanes() {
  const float size = 5.0f;
  const float alpha = 0.2f; // 平面透明度

  // XY平面（蓝色）
  std::vector<float> xyVertices = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, alpha,
                                   size, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, alpha,
                                   0.0f, size, 0.0f, 0.0f, 0.0f, 1.0f, alpha,
                                   size, size, 0.0f, 0.0f, 0.0f, 1.0f, alpha};
  setupComponent(m_xyPlane, xyVertices);

  // XZ平面（绿色）
  std::vector<float> xzVertices = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, alpha,
                                   size, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, alpha,
                                   0.0f, 0.0f, size, 0.0f, 1.0f, 0.0f, alpha,
                                   size, 0.0f, size, 0.0f, 1.0f, 0.0f, alpha};
  setupComponent(m_xzPlane, xzVertices);

  // YZ平面（红色）
  std::vector<float> yzVertices = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, alpha,
                                   0.0f, size, 0.0f, 1.0f, 0.0f, 0.0f, alpha,
                                   0.0f, 0.0f, size, 1.0f, 0.0f, 0.0f, alpha,
                                   0.0f, size, size, 1.0f, 0.0f, 0.0f, alpha};
  setupComponent(m_yzPlane, yzVertices);
}

void ThreeDCoordinate::initializeLabels() {
  const float axisLength = 5.0f;
  const float step = 1.0f;

  // 创建不同的文本样式
  ProGraphics::TextRenderer::TextStyle axisStyle;
  axisStyle.fontSize = 10;
  axisStyle.color = Qt::white;

  // X轴样式
  ProGraphics::TextRenderer::TextStyle xStyle = axisStyle;
  xStyle.color = QColor(255, 100, 100); // 偏红色

  // Y轴样式
  ProGraphics::TextRenderer::TextStyle yStyle = axisStyle;
  yStyle.color = QColor(100, 255, 100); // 偏绿色

  // Z轴样式
  ProGraphics::TextRenderer::TextStyle zStyle = axisStyle;
  zStyle.color = QColor(100, 100, 255); // 偏蓝色

  m_axisLabels.x.clear();
  m_axisLabels.y.clear();
  m_axisLabels.z.clear();

  // 创建轴标签
  auto createAxisLabel =
      [this](const QString &text, const QVector3D &pos,
             const ProGraphics::TextRenderer::TextStyle &style, float offsetX,
             float offsetY) {
        auto *label = m_textRenderer.addLabel(text, pos, style);
        label->offsetX = offsetX;
        label->offsetY = offsetY;
        return label;
      };

  // 添加坐标轴名称
  m_axisLabels.x.push_back(
      createAxisLabel("X", QVector3D(axisLength + 0.3f, 0, 0), xStyle, 0, 15));
  m_axisLabels.y.push_back(
      createAxisLabel("Y", QVector3D(0, axisLength + 0.3f, 0), yStyle, -15, 0));
  m_axisLabels.z.push_back(createAxisLabel(
      "Z", QVector3D(0, 0, axisLength + 0.3f), zStyle, -15, 15));

  // 添加刻度标签
  for (float pos = 0; pos <= axisLength; pos += step) {
    if (std::fmod(pos, 1.0f) < 0.01f) {
      QString text = QString::number(static_cast<int>(pos));

      // X轴标签
      m_axisLabels.x.push_back(
          createAxisLabel(text, QVector3D(pos, 0, 0), xStyle, 0, 15));

      // Y轴标签
      m_axisLabels.y.push_back(
          createAxisLabel(text, QVector3D(0, pos, 0), yStyle, -15, 0));

      // Z轴标签
      m_axisLabels.z.push_back(
          createAxisLabel(text, QVector3D(0, 0, pos), zStyle, -15, 15));
    }
  }
}

void ThreeDCoordinate::setupComponent(CoordinateComponent &component,
                                      const std::vector<float> &vertices) {
  if (!component.vao.isCreated()) {
    component.vao.create();
    component.vbo.create();
  }

  component.vao.bind();
  component.vbo.bind();
  if (component.vertexCount < vertices.size() / 7) {
    component.vbo.allocate(vertices.data(), vertices.size() * sizeof(float));
  } else {
    component.vbo.write(0, vertices.data(), vertices.size() * sizeof(float));
  }

  // 位置属性
  m_program->enableAttributeArray(0);
  m_program->setAttributeBuffer(0, GL_FLOAT, 0, 3, 7 * sizeof(float));
  // 颜色属性（包含alpha通道）
  m_program->enableAttributeArray(1);
  m_program->setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(float), 4,
                                7 * sizeof(float));

  component.vertexCount = vertices.size() / 7;
  component.vao.release();
  component.vbo.release();
}

std::vector<float> ThreeDCoordinate::generateRandomData(int count, float minVal,
                                                        float maxVal) {
  std::vector<float> data;
  data.reserve(count);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dis(minVal, maxVal);

  for (int i = 0; i < count; ++i) {
    data.push_back(dis(gen));
  }

  return data;
}

// 创建新的线条组
void ThreeDCoordinate::createNewLineGroup() {
  makeCurrent(); // 确保GL上下文

  auto newGroup = std::make_unique<LineGroup>();

  // 使用与原版本相同的数据生成逻辑
  const int dataCount = 200;
  const float totalWidth = 5.0f;
  const float maxHeight = 5.0f;

  auto data = generateRandomData(dataCount, 0.0f, 1.0f);
  std::vector<float> vertices;
  vertices.reserve(dataCount * 2 * 7);

  const float xStep = totalWidth / (dataCount - 1);

  for (int i = 0; i < dataCount; ++i) {
    float x = i * xStep; // 居中
    float height = data[i] * maxHeight;
    float r = static_cast<float>(rand()) / RAND_MAX;
    float g = static_cast<float>(rand()) / RAND_MAX;
    float b = static_cast<float>(rand()) / RAND_MAX;

    // 底部顶点
    vertices.insert(vertices.end(), {
                                        x,            // x - 从0到5
                                        0.0f,         // y - 底部固定为0
                                        0.0f,         // z - 将由模型矩阵控制
                                        r, g, b, 1.0f // rgba
                                    });

    // 顶部顶点
    vertices.insert(vertices.end(), {
                                        x,            // x - 与底部相同
                                        height,       // y - 随机高度
                                        0.0f,         // z - 将由模型矩阵控制
                                        r, g, b, 1.0f // rgba
                                    });
  }

  setupComponent(newGroup->lines, vertices);
  m_lineGroups.push_back(std::move(newGroup));

  doneCurrent();
}

// 更新动画
void ThreeDCoordinate::updatePRPSAnimation() {
  bool hasActiveGroups = false;

  for (auto &group : m_lineGroups) {
    if (group->isActive) {
      group->zPosition -= m_prpsAnimationSpeed;

      if (group->zPosition <= 0.0f) {
        group->zPosition = 0.0f;
        group->isActive = false;
      }
      hasActiveGroups = true;
    }
  }

  // 清理不活跃的组
  cleanupInactiveGroups();

  // 如果没有活跃的组并且生成定时器没有运行，则停止动画定时器
  if (!hasActiveGroups && !m_generationTimer.isActive()) {
    m_prpsAnimationTimer.stop();
  }

  update();
}

// 清理不活跃的组
void ThreeDCoordinate::cleanupInactiveGroups() {
  makeCurrent();
  m_lineGroups.erase(
      std::remove_if(m_lineGroups.begin(), m_lineGroups.end(),
                     [](const std::unique_ptr<LineGroup> &group) {
                       return !group->isActive;
                     }),
      m_lineGroups.end());
  doneCurrent();
}

void ThreeDCoordinate::paintGLObjects() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glClearColor(0.18f, 0.23f, 0.33f, 1.0f);

  m_program->bind();
  m_program->setUniformValue("projection", m_camera.getProjectionMatrix());
  m_program->setUniformValue("view", m_camera.getViewMatrix());
  QMatrix4x4 defaultModel;
  m_program->setUniformValue("model", defaultModel);
  // 先绘制半透明平面
  glEnable(GL_BLEND);

  drawComponent(m_xyGrid, GL_LINES);
  drawComponent(m_xzGrid, GL_LINES);
  drawComponent(m_yzGrid, GL_LINES);

  drawComponent(m_xTicks, GL_LINES);
  drawComponent(m_yTicks, GL_LINES);
  drawComponent(m_zTicks, GL_LINES);

  glDisable(GL_BLEND);

  // 后绘制坐标轴
  drawComponent(m_axes, GL_LINES);

  // 2. 然后绘制动态的线条组
  glEnable(GL_LINE_SMOOTH);
  glLineWidth(2.0f);

  for (const auto &group : m_lineGroups) {
    if (group->lines.isVisible()) {
      QMatrix4x4 prpsModel;
      prpsModel.translate(0, 0, group->zPosition);
      m_program->setUniformValue("model", prpsModel);
      drawComponent(group->lines, GL_LINES);
    }
  }

  glLineWidth(1.0f);
  glDisable(GL_LINE_SMOOTH);

  m_program->setUniformValue("model", defaultModel);
  m_program->release();

  QPainter painter(this);
  m_textRenderer.render(painter, m_camera.getViewMatrix(),
                        m_camera.getProjectionMatrix(), width(), height());
}

void ThreeDCoordinate::drawComponent(CoordinateComponent &component,
                                     GLenum mode) {
  if (component.isVisible()) {
    component.vao.bind();
    glDrawArrays(mode, 0, component.vertexCount);
    component.vao.release();
  }
}

void ThreeDCoordinate::resizeGL(int w, int h) {
  m_camera.setAspectRatio(float(w) / float(h));
}

void ThreeDCoordinate::mousePressEvent(QMouseEvent *event) {
  m_controls->handleMousePress(event->pos(), Qt::LeftButton);
}

void ThreeDCoordinate::mouseMoveEvent(QMouseEvent *event) {
  m_controls->handleMouseMove(event->pos(), event->buttons());
}

void ThreeDCoordinate::mouseReleaseEvent(QMouseEvent *event) {
  m_controls->handleMouseRelease(event->button());
}

void ThreeDCoordinate::wheelEvent(QWheelEvent *event) {
  m_controls->handleWheel(event->angleDelta().y() / 120.0f);
}

void ThreeDCoordinate::cleanupComponent(CoordinateComponent &component) {
  if (component.vbo.isCreated()) {
    component.vbo.destroy();
  }
  if (component.vao.isCreated()) {
    component.vao.destroy();
  }
}
