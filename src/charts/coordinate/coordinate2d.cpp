//
// Created by Yang1206 on 2025/2/16.
//

#include "prographics/charts/coordinate/coordinate2d.h"
#include <QMouseEvent>

namespace ProGraphics {
  const char *Coordinate2D::vertexShaderSource = R"(
    #version 410 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec4 aColor;
    uniform mat4 projection;
    uniform mat4 view;
    uniform float pointSize;
    out vec4 vertexColor;
    uniform mat4 model;
    void main() {
        vertexColor = aColor;
        gl_PointSize = pointSize;
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
)";

  const char *Coordinate2D::fragmentShaderSource = R"(
    #version 410 core
    in vec4 vertexColor;
    out vec4 FragColor;
    void main() {
        FragColor = vertexColor;
    }
)";

  Coordinate2D::Coordinate2D(QWidget *parent) : BaseGLWidget(parent) {
    m_camera = Camera(CameraType::Free, ProjectionType::Orthographic);
    setupCamera();
  }

  Coordinate2D::~Coordinate2D() {
    makeCurrent();
    m_axisSystem.reset();
    m_gridSystem.reset();
    doneCurrent();
  }

  void Coordinate2D::setupCamera() {
    float size = m_config.size;

    // 计算实际显示区域（包含边距）
    float totalWidth = size + m_config.margin.left + m_config.margin.right;
    float totalHeight = size + m_config.margin.top + m_config.margin.bottom;
    // 计算中心点偏移
    float centerX = (m_config.margin.left - m_config.margin.right) / 2.0f;
    float centerY = (m_config.margin.bottom - m_config.margin.top) / 2.0f;

    // 设置相机位置和目标点
    m_camera.setPosition(QVector3D(size / 2 + centerX, size / 2 + centerY, 10));
    m_camera.setTarget(QVector3D(size / 2 + centerX, size / 2 + centerY, 0));
    // 计算正交投影的边界
    float aspect = width() / static_cast<float>(height());
    // 计算实际显示范围，保持纵横比
    float displayWidth, displayHeight;
    if (aspect >= 1.0f) {
      displayWidth = totalWidth * aspect;
      displayHeight = totalHeight;
    } else {
      displayWidth = totalWidth;
      displayHeight = totalHeight / aspect;
    }

    // 设置正交投影参数，使坐标轴居中
    m_camera.setOrthographicParams(
      -displayWidth / 2, displayWidth / 2, // left, right
      -displayHeight / 2, displayHeight / 2, // bottom, top
      0.1f, 100.0f // near, far
    );
  }

  bool Coordinate2D::initializeShaders() {
    if (!m_program) {
      m_program = new QOpenGLShaderProgram();
    }

    // 添加着色器
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                            vertexShaderSource)) {
      qDebug() << "Vertex shader compilation failed:" << m_program->log();
      return false;
    }

    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                            fragmentShaderSource)) {
      qDebug() << "Fragment shader compilation failed:" << m_program->log();
      return false;
    }

    // 链接着色器程序
    if (!m_program->link()) {
      qDebug() << "Shader program linking failed:" << m_program->log();
      return false;
    }

    return true;
  }

  void Coordinate2D::initializeGLObjects() {
    // OpenGL 状态设置
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_PROGRAM_POINT_SIZE);

    // 初始化着色器
    if (!initializeShaders()) {
      qDebug() << "Failed to initialize shaders";
      return;
    }

    // 初始化子系统
    m_axisSystem = std::make_unique<Axis>();
    m_axisSystem->initialize();

    m_gridSystem = std::make_unique<Grid>();
    m_gridSystem->initialize();

    m_nameSystem = std::make_unique<AxisName>();
    m_nameSystem->initialize();

    m_tickSystem = std::make_unique<AxisTicks>();
    m_tickSystem->initialize();

    setConfig(m_config);
  }

  void Coordinate2D::paintGLObjects() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(m_backgroundcolor.redF(), m_backgroundcolor.greenF(), m_backgroundcolor.blueF(),
                 m_backgroundcolor.alphaF());
    glEnable(GL_BLEND);
    m_program->bind();
    m_program->setUniformValue("projection", m_camera.getProjectionMatrix());
    m_program->setUniformValue("view", m_camera.getViewMatrix());
    QMatrix4x4 defaultModel;
    m_program->setUniformValue("model", defaultModel);
    const QMatrix4x4 model = m_camera.getViewMatrix();
    const QMatrix4x4 projection = m_camera.getProjectionMatrix();

    if (m_config.axis.enabled) {
      m_axisSystem->render(projection, model);
    }
    if (m_config.grid.enabled) {
      m_gridSystem->render(projection, model);
    }

    m_program->release();

    // QPainter 渲染文本
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (m_nameSystem) {
      m_nameSystem->render(painter, model, projection, width(), height());
    }

    if (m_tickSystem) {
      m_tickSystem->render(painter, model, projection, width(), height());
    }

    painter.end();
  }

  void Coordinate2D::resizeGL(int w, int h) {
    setupCamera();
    BaseGLWidget::resizeGL(w, h);
  }

  void Coordinate2D::setConfig(const Config &config) {
    m_config = config;
    updateAxisSystem();
    updateGridSystem();
    updateNameSystem();
    updateTickSystem();
    update();
  }

  void Coordinate2D::setSize(float size) {
    m_config.size = size;
    setupCamera();
    updateAxisSystem();
    updateGridSystem();
    updateNameSystem();
    updateTickSystem();
    update();
  }

  void Coordinate2D::setEnabled(bool enabled) {
    m_config.enabled = enabled;
    update();
  }

  // 轴相关方法
  void Coordinate2D::setAxisEnabled(bool enabled) {
    m_config.axis.enabled = enabled;
    updateAxisSystem();
    update();
  }

  void Coordinate2D::setAxisVisible(char axis, bool visible) {
    if (axis == 'x') {
      m_config.axis.x.visible = visible;
    } else if (axis == 'y') {
      m_config.axis.y.visible = visible;
    }
    updateAxisSystem();
    update();
  }

  void Coordinate2D::setAxisColor(char axis, const QColor &color) {
    QVector4D vColor = QVector4D(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    if (axis == 'x') {
      m_config.axis.x.color = vColor;
    } else if (axis == 'y') {
      m_config.axis.y.color = vColor;
    }
    updateAxisSystem();
    update();
  }

  void Coordinate2D::setAxisThickness(char axis, float thickness) {
    if (axis == 'x') {
      m_config.axis.x.thickness = thickness;
    } else if (axis == 'y') {
      m_config.axis.y.thickness = thickness;
    }
    updateAxisSystem();
    update();
  }

  // 轴名称相关方法
  void Coordinate2D::setAxisNameEnabled(bool enabled) {
    m_config.names.enabled = enabled;
    updateNameSystem();
    update();
  }

  void Coordinate2D::setAxisNameVisible(char axis, bool visible) {
    if (axis == 'x') {
      m_config.names.x.visible = visible;
    } else if (axis == 'y') {
      m_config.names.y.visible = visible;
    }
    updateNameSystem();
    update();
  }

  void Coordinate2D::setAxisName(char axis, const QString &name,
                                 const QString &unit) {
    if (axis == 'x') {
      m_config.names.x.text = name;
      m_config.names.x.unit = unit;
    } else if (axis == 'y') {
      m_config.names.y.text = name;
      m_config.names.y.unit = unit;
    }
    updateNameSystem();
    update();
  }

  void Coordinate2D::setAxisNameLocation(char axis, AxisName::Location location) {
    if (axis == 'x') {
      m_config.names.x.location = location;
    } else if (axis == 'y') {
      m_config.names.y.location = location;
    }
    updateNameSystem();
    update();
  }

  void Coordinate2D::setAxisNameOffset(char axis, const QVector3D &offset) {
    if (axis == 'x') {
      m_config.names.x.offset = offset;
    } else if (axis == 'y') {
      m_config.names.y.offset = offset;
    }
    updateNameSystem();
    update();
  }

  void Coordinate2D::setAxisNameGap(char axis, float gap) {
    if (axis == 'x') {
      m_config.names.x.gap = gap;
    } else if (axis == 'y') {
      m_config.names.y.gap = gap;
    }
    updateNameSystem();
    update();
  }

  void Coordinate2D::setAxisNameStyle(char axis,
                                      const TextRenderer::TextStyle &style) {
    if (axis == 'x') {
      m_config.names.x.style = style;
    } else if (axis == 'y') {
      m_config.names.y.style = style;
    }
    updateNameSystem();
    update();
  }

  // 网格相关方法
  void Coordinate2D::setGridEnabled(bool enabled) {
    m_config.grid.enabled = enabled;
    updateGridSystem();
    update();
  }

  void Coordinate2D::setGridVisible(bool visible) {
    m_config.grid.xy.visible = visible;
    updateGridSystem();
    update();
  }

  void Coordinate2D::setGridColors(const QColor &majorColor,
                                   const QColor &minorColor) {
    QVector4D vMajorColor = QVector4D(majorColor.redF(), majorColor.greenF(),
                                      majorColor.blueF(), majorColor.alphaF());
    QVector4D vMinorColor = QVector4D(minorColor.redF(), minorColor.greenF(),
                                      minorColor.blueF(), minorColor.alphaF());
    m_config.grid.xy.majorColor = vMajorColor;
    m_config.grid.xy.minorColor = vMinorColor;
    updateGridSystem();
    update();
  }

  void Coordinate2D::setGridSpacing(float spacing) {
    m_config.grid.xy.spacing = spacing;
    updateGridSystem();
    update();
  }

  void Coordinate2D::setGridThickness(float thickness) {
    m_config.grid.xy.thickness = thickness;
    updateGridSystem();
    update();
  }

  void Coordinate2D::setGridSineWaveConfig(Grid::SineWaveConfig &config) {
    m_config.grid.xy.sineWave = config;
    updateGridSystem();
    update();
  }

  // 刻度相关方法
  void Coordinate2D::setTicksEnabled(bool enabled) {
    m_config.ticks.enabled = enabled;
    updateTickSystem();
    update();
  }

  void Coordinate2D::setTicksVisible(char axis, bool visible) {
    if (axis == 'x') {
      m_config.ticks.x.visible = visible;
    } else if (axis == 'y') {
      m_config.ticks.y.visible = visible;
    }
    updateTickSystem();
    update();
  }

  void Coordinate2D::setTicksRange(char axis, float min, float max, float step) {
    if (axis == 'x') {
      m_config.ticks.x.range.min = min;
      m_config.ticks.x.range.max = max;
      m_config.ticks.x.range.step = step;
    } else if (axis == 'y') {
      m_config.ticks.y.range.min = min;
      m_config.ticks.y.range.max = max;
      m_config.ticks.y.range.step = step;
    }
    updateTickSystem();
    update();
  }

  void Coordinate2D::setTicksOffset(char axis, const QVector3D &offset) {
    if (!m_tickSystem) return;

    if (axis == 'x') {
      m_config.ticks.x.offset = offset;
    } else if (axis == 'y') {
      m_config.ticks.y.offset = offset;
    }
    updateTickSystem();
    update();
  }

  void Coordinate2D::setTicksAlignment(char axis, Qt::Alignment alignment) {
    switch (axis) {
      case 'x':
        m_config.ticks.x.alignment = alignment;
        break;
      case 'y':
        m_config.ticks.y.alignment = alignment;
        break;
    }
    updateTickSystem();
    update();
  }

  void Coordinate2D::setTicksStyle(char axis,
                                   const TextRenderer::TextStyle &style) {
    if (axis == 'x') {
      m_config.ticks.x.style = style;
    } else if (axis == 'y') {
      m_config.ticks.y.style = style;
    }
    updateTickSystem();
    update();
  }

  void Coordinate2D::setTicksFormatter(char axis,
                                       std::function<QString(float)> formatter) {
    if (axis == 'x') {
      m_config.ticks.x.formatter = formatter;
    } else if (axis == 'y') {
      m_config.ticks.y.formatter = formatter;
    }
    updateTickSystem();
    update();
  }

  void Coordinate2D::setMargin(float left, float right, float top, float bottom) {
    m_config.margin.left = left;
    m_config.margin.right = right;
    m_config.margin.top = top;
    m_config.margin.bottom = bottom;
    setupCamera();
    update();
  }

  // 更新系统方法
  void Coordinate2D::updateAxisSystem() {
    if (!m_axisSystem) return;
    Axis::Config axisConfig;
    axisConfig.length = m_config.size;
    if (m_config.axis.enabled) {
      // X轴配置
      axisConfig.xAxisVisible = m_config.axis.x.visible;
      axisConfig.xAxisColor = m_config.axis.x.color;
      axisConfig.xAxisThickness = m_config.axis.x.thickness;
      // Y轴配置
      axisConfig.yAxisVisible = m_config.axis.y.visible;
      axisConfig.yAxisColor = m_config.axis.y.color;
      axisConfig.yAxisThickness = m_config.axis.y.thickness;
      // 禁用Z轴
      axisConfig.zAxisVisible = false;
    }

    m_axisSystem->setConfig(axisConfig);
  }

  void Coordinate2D::updateGridSystem() {
    if (!m_gridSystem) return;
    Grid::Config gridConfig;
    gridConfig.size = m_config.size;
    if (m_config.grid.enabled) {
      // XY平面网格配置
      gridConfig.xy = {
        m_config.grid.xy.visible,
        m_config.grid.xy.thickness,
        m_config.grid.xy.spacing,
        m_config.grid.xy.majorColor,
        m_config.grid.xy.minorColor
      };

      gridConfig.xy.sineWave = m_config.grid.xy.sineWave;
      //禁用XZ和YZ平面网格
      gridConfig.xz.visible = false;
      gridConfig.yz.visible = false;
    }

    m_gridSystem->setConfig(gridConfig);
  }

  void Coordinate2D::updateNameSystem() {
    if (!m_nameSystem) return;

    AxisName::Config nameConfig;
    nameConfig.size = m_config.size;

    if (m_config.names.enabled) {
      // X轴名称默认偏移
      nameConfig.x = m_config.names.x;
      nameConfig.x.offset = m_config.names.x.offset;

      // Y轴名称默认偏移
      nameConfig.y = m_config.names.y;
      nameConfig.y.offset = m_config.names.y.offset;

      // 禁用Z轴名称
      nameConfig.z.visible = false;
    }

    m_nameSystem->setConfig(nameConfig);
  }

  void Coordinate2D::updateTickSystem() {
    if (!m_tickSystem) return;

    AxisTicks::Config tickConfig;
    tickConfig.size = m_config.size;

    if (m_config.ticks.enabled) {
      // X轴刻度配置
      tickConfig.x = m_config.ticks.x;
      // 移除与轴线可见性的绑定
      tickConfig.x.visible = m_config.ticks.x.visible;
      tickConfig.x.offset = m_config.ticks.x.offset;
      tickConfig.x.alignment = Qt::AlignHCenter | Qt::AlignTop;

      // Y轴刻度配置
      tickConfig.y = m_config.ticks.y;
      // 移除与轴线可见性的绑定
      tickConfig.y.visible = m_config.ticks.y.visible;
      tickConfig.y.offset = m_config.ticks.y.offset;
      tickConfig.y.alignment = Qt::AlignVCenter | Qt::AlignRight;

      // 禁用Z轴刻度
      tickConfig.z.visible = false;
    }

    m_tickSystem->setConfig(tickConfig);
  }
} // namespace ProGraphics
