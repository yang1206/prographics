#include "prographics/charts/coordinate/coordinate3d.h"
#include "prographics/charts/coordinate/axis.h"
#include "prographics/charts/coordinate/grid.h"
#include "prographics/charts/base/gl_widget.h"
#include <QMouseEvent>

namespace ProGraphics {
  // 定义着色器源代码
  const char *Coordinate3D::vertexShaderSource = R"(
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

  const char *Coordinate3D::fragmentShaderSource = R"(
    #version 410 core
in vec4 vertexColor;
out vec4 FragColor;

void main() {
    FragColor = vertexColor;
}
)";

  Coordinate3D::Coordinate3D(QWidget *parent) : BaseGLWidget(parent) {
    // 设置鼠标跟踪和焦点策略
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    m_camera = Camera(CameraType::Orbit, ProjectionType::Perspective);

    float size = m_config.size;

    // 初始化控制器并设置合适的参数
    m_controls = std::make_unique<OrbitControls>(&m_camera);
    // 设置更合适的初始视角
    m_camera.setPosition(QVector3D(10.0f, 10.0f, 10.0f));
    m_camera.setPivotPoint(QVector3D(size / 2, size / 2.5, 0.0f));
    m_camera.setOrbitYaw(85.0f);
    m_camera.setOrbitPitch(7.0f);
    m_camera.orbit(-180.0f, 50.0f);
    // 设置投影参数
    m_camera.setFov(50.0f);
    m_camera.setNearPlane(0.1f);
    m_camera.setFarPlane(1000.0f);


    connect(m_controls.get(), &OrbitControls::updated, this,
            [this]() { update(); });
  }

  Coordinate3D::~Coordinate3D() {
    makeCurrent();
    m_axisSystem.reset();
    m_gridSystem.reset();
    doneCurrent();
  }

  bool Coordinate3D::initializeShaders() {
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

  void Coordinate3D::initializeGLObjects() {
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

  void Coordinate3D::paintGLObjects() {
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

  void Coordinate3D::resizeGL(int w, int h) {
    BaseGLWidget::resizeGL(w, h);
    if (m_config.displayMode != DisplayMode3D::FreeView) {
      adjustCameraForDisplayMode();
    } else {
      // 自由模式只更新纵横比
      m_camera.setAspectRatio(float(w) / float(h));
    }
  }

  void Coordinate3D::setConfig(const Config &config) {
    m_config = config;
    updateAxisSystem();
    updateGridSystem();
    updateNameSystem();
    updateTickSystem();
    update();
  }

  void Coordinate3D::setSize(float size) {
    m_config.size = size;
    updateAxisSystem();
    updateGridSystem();
    updateNameSystem();
    updateTickSystem();
    update();
  }

  void Coordinate3D::setEnabled(bool enabled) {
    m_config.enabled = enabled;
    update();
  }

  void Coordinate3D::setAxisEnabled(bool enabled) {
    m_config.axis.enabled = enabled;
    updateAxisSystem();
    update();
  }

  void Coordinate3D::setAxisVisible(char axis, bool visible) {
    switch (axis) {
      case 'x':
        m_config.axis.x.visible = visible;
        break;
      case 'y':
        m_config.axis.y.visible = visible;
        break;
      case 'z':
        m_config.axis.z.visible = visible;
        break;
    }
    updateAxisSystem();
    update();
  }

  void Coordinate3D::setAxisColor(char axis, const QColor &color) {
    QVector4D vColor = QVector4D(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    switch (axis) {
      case 'x':
        m_config.axis.x.color = vColor;
        break;
      case 'y':
        m_config.axis.y.color = vColor;
        break;
      case 'z':
        m_config.axis.z.color = vColor;
        break;
      default:
        qWarning() << "错误的axis";
    }
    updateAxisSystem();
    update();
  }

  void Coordinate3D::setGridEnabled(bool enabled) {
    m_config.grid.enabled = enabled;
    updateGridSystem();
    update();
  }

  void Coordinate3D::setGridVisible(const QString &plane, bool visible) {
    if (plane == "xy")
      m_config.grid.xy.visible = visible;
    else if (plane == "xz")
      m_config.grid.xz.visible = visible;
    else if (plane == "yz")
      m_config.grid.yz.visible = visible;
    updateGridSystem();
    update();
  }

  void Coordinate3D::setAxisThickness(char axis, float thickness) {
    switch (axis) {
      case 'x':
        m_config.axis.x.thickness = thickness;
        break;
      case 'y':
        m_config.axis.y.thickness = thickness;
        break;
      case 'z':
        m_config.axis.z.thickness = thickness;
        break;
    }
    updateAxisSystem();
    update();
  }

  void Coordinate3D::setGridSpacing(const QString &plane, float spacing) {
    if (plane == "xy") {
      m_config.grid.xy.spacing = spacing;
    } else if (plane == "xz") {
      m_config.grid.xz.spacing = spacing;
    } else if (plane == "yz") {
      m_config.grid.yz.spacing = spacing;
    }
    updateGridSystem();
    update();
  }

  void Coordinate3D::setGridThickness(const QString &plane, float thickness) {
    if (plane == "xy") {
      m_config.grid.xy.thickness = thickness;
    } else if (plane == "xz") {
      m_config.grid.xz.thickness = thickness;
    } else if (plane == "yz") {
      m_config.grid.yz.thickness = thickness;
    }
    updateGridSystem();
    update();
  }

  void Coordinate3D::setGridSineWaveConfig(const QString &plane, Grid::SineWaveConfig &config) {
    if (plane == "xy") {
      m_config.grid.xy.sineWave = config;
    } else if (plane == "xz") {
      m_config.grid.xz.sineWave = config;
    } else if (plane == "yz") {
      m_config.grid.yz.sineWave = config;
    }
    updateGridSystem();
    update();
  }

  void Coordinate3D::setGridColors(const QString &plane,
                                   const QColor &majorColor,
                                   const QColor &minorColor) {
    QVector4D vMajorColor = QVector4D(majorColor.redF(), majorColor.greenF(), majorColor.blueF(),
                                      majorColor.alphaF());
    QVector4D vMinorColor = QVector4D(minorColor.redF(), minorColor.greenF(), minorColor.blueF(),
                                      minorColor.alphaF());
    if (plane == "xy") {
      m_config.grid.xy.majorColor = vMajorColor;
      m_config.grid.xy.minorColor = vMinorColor;
    } else if (plane == "xz") {
      m_config.grid.xz.majorColor = vMajorColor;
      m_config.grid.xz.minorColor = vMinorColor;
    } else if (plane == "yz") {
      m_config.grid.yz.majorColor = vMajorColor;
      m_config.grid.yz.minorColor = vMinorColor;
    }
    updateGridSystem();
    update();
  }

  // 新增的方法实现
  void Coordinate3D::setTicksEnabled(bool enabled) {
    m_config.ticks.enabled = enabled;
    updateTickSystem();
    update();
  }

  void Coordinate3D::setTicksVisible(char axis, bool visible) {
    switch (axis) {
      case 'x':
        m_config.ticks.x.visible = visible;
        break;
      case 'y':
        m_config.ticks.y.visible = visible;
        break;
      case 'z':
        m_config.ticks.z.visible = visible;
        break;
    }
    updateTickSystem();
    update();
  }

  void Coordinate3D::setTicksRange(char axis, float min, float max, float step) {
    switch (axis) {
      case 'x':
        m_config.ticks.x.range.min = min;
        m_config.ticks.x.range.max = max;
        m_config.ticks.x.range.step = step;
        break;
      case 'y':
        m_config.ticks.y.range.min = min;
        m_config.ticks.y.range.max = max;
        m_config.ticks.y.range.step = step;
        break;
      case 'z':
        m_config.ticks.z.range.min = min;
        m_config.ticks.z.range.max = max;
        m_config.ticks.z.range.step = step;
        break;
    }
    updateTickSystem();
    update();
  }

  void Coordinate3D::setTicksOffset(char axis, const QVector3D &offset) {
    switch (axis) {
      case 'x':
        m_config.ticks.x.offset = offset;
        break;
      case 'y':
        m_config.ticks.y.offset = offset;
        break;
      case 'z':
        m_config.ticks.z.offset = offset;
        break;
    }
    updateTickSystem();
    update();
  }

  void Coordinate3D::setTicksAlignment(char axis, Qt::Alignment alignment) {
    switch (axis) {
      case 'x':
        m_config.ticks.x.alignment = alignment;
        break;
      case 'y':
        m_config.ticks.y.alignment = alignment;
        break;
      case 'z':
        m_config.ticks.z.alignment = alignment;
        break;
    }
    updateTickSystem();
    update();
  }

  void Coordinate3D::setTicksStyle(char axis,
                                   const TextRenderer::TextStyle &style) {
    switch (axis) {
      case 'x':
        m_config.ticks.x.style = style;
        break;
      case 'y':
        m_config.ticks.y.style = style;
        break;
      case 'z':
        m_config.ticks.z.style = style;
        break;
    }
    updateTickSystem();
    update();
  }

  void Coordinate3D::setTicksFormatter(char axis,
                                       std::function<QString(float)> formatter) {
    switch (axis) {
      case 'x':
        m_config.ticks.x.formatter = formatter;
        break;
      case 'y':
        m_config.ticks.y.formatter = formatter;
        break;
      case 'z':
        m_config.ticks.z.formatter = formatter;
        break;
    }
    updateTickSystem();
    update();
  }

  void Coordinate3D::setAxisAndGridColor(char axis, const QColor &axisColor,
                                         const QColor &gridMajorColor,
                                         const QColor &gridMinorColor) {
    // 设置轴颜色
    setAxisColor(axis, axisColor);

    // 设置相关平面的网格颜色
    switch (axis) {
      case 'x':
        setGridColors("xy", gridMajorColor, gridMinorColor);
        setGridColors("xz", gridMajorColor, gridMinorColor);
        break;
      case 'y':
        setGridColors("xy", gridMajorColor, gridMinorColor);
        setGridColors("yz", gridMajorColor, gridMinorColor);
        break;
      case 'z':
        setGridColors("xz", gridMajorColor, gridMinorColor);
        setGridColors("yz", gridMajorColor, gridMinorColor);
        break;
    }
  }


  void Coordinate3D::setDisplayMode(DisplayMode3D mode) {
    m_config.displayMode = mode;
    adjustCameraForDisplayMode();
  }

  void Coordinate3D::setDisplaySettings(const Config::DisplaySettings &settings) {
    m_config.displaySettings = settings;
    if (m_config.displayMode != DisplayMode3D::FreeView) {
      adjustCameraForDisplayMode();
    }
  }

  void Coordinate3D::resetCameraToOptimalView() {
    auto params = calculateOptimalCameraParams(width(), height());
    m_camera.setPosition(params.position);
    m_camera.setFov(params.fov);
    m_camera.setAspectRatio(static_cast<float>(width()) / height());
    update();
  }


  void Coordinate3D::setAxisNameEnabled(bool enabled) {
    m_config.names.enabled = enabled;
    updateNameSystem();
    update();
  }

  void Coordinate3D::setAxisNameVisible(char axis, bool visible) {
    switch (axis) {
      case 'x':
        m_config.names.x.visible = visible;
        break;
      case 'y':
        m_config.names.y.visible = visible;
        break;
      case 'z':
        m_config.names.z.visible = visible;
        break;
    }
    updateNameSystem();
    update();
  }

  void Coordinate3D::setAxisName(char axis, const QString &name,
                                 const QString &unit) {
    switch (axis) {
      case 'x':
        m_config.names.x.text = name;
        m_config.names.x.unit = unit;
        break;
      case 'y':
        m_config.names.y.text = name;
        m_config.names.y.unit = unit;
        break;
      case 'z':
        m_config.names.z.text = name;
        m_config.names.z.unit = unit;
        break;
    }
    updateNameSystem();
    update();
  }

  void Coordinate3D::setAxisNameLocation(char axis, AxisName::Location location) {
    switch (axis) {
      case 'x':
        m_config.names.x.location = location;
        break;
      case 'y':
        m_config.names.y.location = location;
        break;
      case 'z':
        m_config.names.z.location = location;
        break;
    }
    updateNameSystem();
    update();
  }

  void Coordinate3D::setAxisNameOffset(char axis, const QVector3D &offset) {
    if (axis == 'x') {
      m_config.names.x.offset = offset;
    } else if (axis == 'y') {
      m_config.names.y.offset = offset;
    } else if (axis == 'z') {
      m_config.names.z.offset = offset;
    }
    updateNameSystem();
    update();
  }

  void Coordinate3D::setAxisNameGap(char axis, float gap) {
    switch (axis) {
      case 'x':
        m_config.names.x.gap = gap;
        break;
      case 'y':
        m_config.names.y.gap = gap;
        break;
      case 'z':
        m_config.names.z.gap = gap;
        break;
    }
    updateNameSystem();
    update();
  }

  void Coordinate3D::setAxisNameStyle(char axis,
                                      const TextRenderer::TextStyle &style) {
    switch (axis) {
      case 'x':
        m_config.names.x.style = style;
        break;
      case 'y':
        m_config.names.y.style = style;
        break;
      case 'z':
        m_config.names.z.style = style;
        break;
    }
    updateNameSystem();
    update();
  }

  void Coordinate3D::updateAxisSystem() {
    if (!m_axisSystem)
      return;

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

      // Z轴配置
      axisConfig.zAxisVisible = m_config.axis.z.visible;
      axisConfig.zAxisColor = m_config.axis.z.color;
      axisConfig.zAxisThickness = m_config.axis.z.thickness;
    } else {
      axisConfig.xAxisVisible = false;
      axisConfig.yAxisVisible = false;
      axisConfig.zAxisVisible = false;
    }

    m_axisSystem->setConfig(axisConfig);
  }

  void Coordinate3D::updateGridSystem() {
    if (!m_gridSystem)
      return;

    Grid::Config gridConfig;
    gridConfig.size = m_config.size;

    if (m_config.grid.enabled) {
      // XY平面配置
      gridConfig.xy = {
        m_config.grid.xy.visible, m_config.grid.xy.thickness,
        m_config.grid.xy.spacing, m_config.grid.xy.majorColor,
        m_config.grid.xy.minorColor
      };
      gridConfig.xy.visible = m_config.grid.enabled && m_config.grid.xy.visible;

      gridConfig.xy.sineWave = m_config.grid.xy.sineWave;
      gridConfig.yz.sineWave = m_config.grid.yz.sineWave;
      gridConfig.xz.visible = m_config.grid.xz.visible;

      // XZ平面配置
      gridConfig.xz = {
        m_config.grid.xz.visible, m_config.grid.xz.thickness,
        m_config.grid.xz.spacing, m_config.grid.xz.majorColor,
        m_config.grid.xz.minorColor
      };
      gridConfig.xz.visible = m_config.grid.enabled && m_config.grid.xz.visible;

      // YZ平面配置
      gridConfig.yz = {
        m_config.grid.yz.visible, m_config.grid.yz.thickness,
        m_config.grid.yz.spacing, m_config.grid.yz.majorColor,
        m_config.grid.yz.minorColor
      };
      gridConfig.yz.visible = m_config.grid.enabled && m_config.grid.yz.visible;
    }

    m_gridSystem->setConfig(gridConfig);
  }

  void Coordinate3D::updateNameSystem() {
    if (!m_nameSystem)
      return;

    AxisName::Config nameConfig;
    nameConfig.size = m_config.size;

    // 配置X轴名称
    nameConfig.x = m_config.names.x;
    nameConfig.x.visible = m_config.axis.x.visible && m_config.names.enabled && m_config.names.x.visible;
    nameConfig.x.style = m_config.names.x.style;

    // 配置Y轴名称
    nameConfig.y = m_config.names.y;
    nameConfig.y.visible = m_config.axis.y.visible && m_config.names.enabled && m_config.names.y.visible;
    nameConfig.y.style = m_config.names.y.style;

    // 配置Z轴名称
    nameConfig.z = m_config.names.z;
    nameConfig.z.visible = m_config.axis.z.visible && m_config.names.enabled && m_config.names.z.visible;
    nameConfig.z.style = m_config.names.z.style;

    m_nameSystem->setConfig(nameConfig);
  }

  void Coordinate3D::updateTickSystem() {
    if (!m_tickSystem)
      return;

    AxisTicks::Config tickConfig;
    tickConfig.size = m_config.size;

    if (m_config.ticks.enabled) {
      // 配置X轴刻度
      tickConfig.x = m_config.ticks.x;
      tickConfig.x.visible = m_config.axis.x.visible && m_config.ticks.enabled && m_config.ticks.x.visible;
      tickConfig.x.style = m_config.ticks.x.style;
      tickConfig.x.offset = m_config.ticks.x.offset;
      tickConfig.x.alignment = Qt::AlignHCenter | Qt::AlignTop;

      // 配置Y轴刻度
      tickConfig.y = m_config.ticks.y;
      tickConfig.y.visible = m_config.axis.y.visible && m_config.ticks.enabled && m_config.ticks.y.visible;
      tickConfig.y.style = m_config.ticks.y.style;
      tickConfig.y.offset = m_config.ticks.y.offset;
      tickConfig.y.alignment = Qt::AlignRight | Qt::AlignVCenter;

      // 配置Z轴刻度
      tickConfig.z = m_config.ticks.z;
      tickConfig.z.visible = m_config.axis.z.visible && m_config.ticks.enabled && m_config.ticks.z.visible;
      tickConfig.z.style = m_config.ticks.z.style;
      tickConfig.z.offset = m_config.ticks.z.offset;
      tickConfig.z.alignment = Qt::AlignLeft | Qt::AlignVCenter;
    }

    m_tickSystem->setConfig(tickConfig);
  }

  void Coordinate3D::adjustCameraForDisplayMode() {
    int windowWidth = width();
    int windowHeight = height();
    // 防止除零错误
    if (windowWidth <= 0) windowWidth = 800;
    if (windowHeight <= 0) windowHeight = 600;

    const auto &settings = m_config.displaySettings;

    switch (m_config.displayMode) {
      case DisplayMode3D::FreeView:
        // 自由模式：只更新纵横比，不做其他调整
        m_camera.setAspectRatio(static_cast<float>(windowWidth) / windowHeight);
        break;

      case DisplayMode3D::AdaptiveFOV: {
        // 自适应FOV模式：根据窗口大小调整视野角度
        int minDimension = std::min(windowWidth, windowHeight);
        float adaptiveFov = settings.baseFOV;

        if (minDimension < 300) {
          // 很小的窗口，增加FOV显示更多内容
          adaptiveFov = settings.baseFOV + (300 - minDimension) * 0.08f * settings.adaptiveSensitivity;
        } else if (minDimension < 500) {
          // 小窗口，适度增加FOV
          adaptiveFov = settings.baseFOV + (500 - minDimension) * 0.04f * settings.adaptiveSensitivity;
        } else if (minDimension > 1000) {
          // 大窗口，减少FOV获得更好细节
          adaptiveFov = settings.baseFOV - (minDimension - 1000) * 0.01f * settings.adaptiveSensitivity;
        }

        // 限制FOV范围
        adaptiveFov = std::clamp(adaptiveFov, settings.minFOV, settings.maxFOV);

        m_camera.setFov(adaptiveFov);
        m_camera.setAspectRatio(static_cast<float>(windowWidth) / windowHeight);
      }
      break;
      case DisplayMode3D::FixedDistance: {
        // 固定距离模式：保持相机到目标的距离，但调整FOV
        QVector3D currentPos = m_camera.getPosition();
        QVector3D target = m_camera.getTarget();
        float currentDistance = (currentPos - target).length();

        // 保持距离不变，但根据窗口调整FOV
        float windowAspect = static_cast<float>(windowWidth) / windowHeight;
        float fovAdjustment = 1.0f;

        if (windowAspect > 2.0f) {
          // 超宽屏，增加FOV
          fovAdjustment = 1.0f + (windowAspect - 2.0f) * 0.1f;
        } else if (windowAspect < 0.5f) {
          // 超高屏，增加FOV
          fovAdjustment = 1.0f + (0.5f - windowAspect) * 0.2f;
        }

        float adjustedFov = settings.baseFOV * fovAdjustment;
        adjustedFov = std::clamp(adjustedFov, settings.minFOV, settings.maxFOV);

        m_camera.setFov(adjustedFov);
        m_camera.setAspectRatio(windowAspect);
      }
      break;
      case DisplayMode3D::BestFit: {
        // 最佳适应模式：计算最佳相机参数
        auto params = calculateOptimalCameraParams(windowWidth, windowHeight);

        if (!settings.preserveUserRotation) {
          // 不保持用户旋转，完全重置到最佳位置
          m_camera.setPosition(params.position);
        } else {
          // 保持用户的旋转角度，只调整距离
          QVector3D target = m_camera.getTarget();
          QVector3D currentDir = (m_camera.getPosition() - target).normalized();
          m_camera.setPosition(target + currentDir * params.distance);
        }

        m_camera.setFov(params.fov);
        m_camera.setAspectRatio(static_cast<float>(windowWidth) / windowHeight);
      }
      break;
      case DisplayMode3D::SmartHybrid: {
        // 智能混合模式：只在窗口变化较大时调整
        static int lastWidth = windowWidth;
        static int lastHeight = windowHeight;

        float widthChange = std::abs(windowWidth - lastWidth) / static_cast<float>(lastWidth);
        float heightChange = std::abs(windowHeight - lastHeight) / static_cast<float>(lastHeight);

        // 只有当窗口变化超过20%时才进行调整
        if (widthChange > 0.2f || heightChange > 0.2f) {
          // 进行适度的FOV调整
          int minDimension = std::min(windowWidth, windowHeight);
          float fovAdjustment = 1.0f;

          if (minDimension < 400) {
            fovAdjustment = 1.0f + (400 - minDimension) * 0.001f * settings.adaptiveSensitivity;
          } else if (minDimension > 800) {
            fovAdjustment = 1.0f - (minDimension - 800) * 0.0005f * settings.adaptiveSensitivity;
          }

          float newFov = settings.baseFOV * fovAdjustment;
          newFov = std::clamp(newFov, settings.minFOV, settings.maxFOV);
          m_camera.setFov(newFov);
        }

        lastWidth = windowWidth;
        lastHeight = windowHeight;
        m_camera.setAspectRatio(static_cast<float>(windowWidth) / windowHeight);
      }
      break;
      case DisplayMode3D::KeepAspectRatio: {
        // 锁定纵横比模式：保持3D场景的原始比例
        float targetAspect = 4.0f / 3.0f; // 假设目标纵横比为4:3
        float windowAspect = static_cast<float>(windowWidth) / windowHeight;

        // 根据纵横比差异调整FOV，但保持场景比例
        if (std::abs(windowAspect - targetAspect) > 0.1f) {
          float fovAdjustment = targetAspect / windowAspect;
          fovAdjustment = std::clamp(fovAdjustment, 0.7f, 1.5f);

          float adjustedFov = settings.baseFOV * fovAdjustment;
          adjustedFov = std::clamp(adjustedFov, settings.minFOV, settings.maxFOV);
          m_camera.setFov(adjustedFov);
        }

        m_camera.setAspectRatio(windowAspect);
      }
      break;
    }
  }


  Coordinate3D::CameraParams Coordinate3D::calculateOptimalCameraParams(int windowWidth, int windowHeight) {
    CameraParams params;
    float size = m_config.size;
    const auto &settings = m_config.displaySettings;
    // 计算最佳观察距离（基于坐标系大小和窗口尺寸）
    int minDimension = std::min(windowWidth, windowHeight);
    float distanceMultiplier = 1.0f;
    if (minDimension < 300) {
      distanceMultiplier = 2.5f; // 小窗口，距离远一些
    } else if (minDimension < 600) {
      distanceMultiplier = 2.0f;
    } else if (minDimension > 1200) {
      distanceMultiplier = 1.5f; // 大窗口，可以近一些
    } else {
      distanceMultiplier = 1.8f;
    }
    params.distance = size * distanceMultiplier;
    // 计算最佳FOV
    float windowAspect = static_cast<float>(windowWidth) / windowHeight;
    params.fov = settings.baseFOV;
    if (windowAspect > 2.0f) {
      // 超宽屏
      params.fov = settings.baseFOV * 1.2f;
    } else if (windowAspect < 0.6f) {
      // 超高屏
      params.fov = settings.baseFOV * 1.3f;
    }
    params.fov = std::clamp(params.fov, settings.minFOV, settings.maxFOV);
    // 计算最佳相机位置（保持一个好的观察角度）
    float angle = 45.0f; // 默认45度角观察
    float height = params.distance * std::sin(qDegreesToRadians(angle));
    float horizontal = params.distance * std::cos(qDegreesToRadians(angle));
    QVector3D target = QVector3D(size / 2, size / 2, size / 2);
    params.position = target + QVector3D(horizontal, horizontal, height);

    return params;
  }


  // 事件处理
  void Coordinate3D::mousePressEvent(QMouseEvent *event) {
    m_controls->handleMousePress(event->pos(), Qt::LeftButton);
  }

  void Coordinate3D::mouseMoveEvent(QMouseEvent *event) {
    m_controls->handleMouseMove(event->pos(), event->buttons());
  }

  void Coordinate3D::mouseReleaseEvent(QMouseEvent *event) {
    m_controls->handleMouseRelease(event->button());
  }

  void Coordinate3D::wheelEvent(QWheelEvent *event) {
    m_controls->handleWheel(event->angleDelta().y() / 120.0f);
  }
} // namespace ProGraphics
