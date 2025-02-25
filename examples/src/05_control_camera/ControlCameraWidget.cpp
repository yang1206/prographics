#include "ControlCameraWidget.h"
#include <QGroupBox>
#include <QVBoxLayout>

ControlCameraWidget::ControlCameraWidget(QWidget *parent)
    : BaseGLWidget(parent), m_texture1(nullptr), m_texture2(nullptr) {
  initCubePositions();
  setupUI();
  m_camera = ProGraphics::Camera(ProGraphics::CameraType::Free,
                                 ProGraphics::ProjectionType::Perspective);
  m_camera.setPerspectiveParams(45.0f, width() / float(height()), 0.1f, 100.0f);
  m_cameraTypeCombo->setCurrentIndex(m_cameraTypeCombo->findData(
      static_cast<int>(ProGraphics::CameraType::Free)));
  m_projectionTypeCombo->setCurrentIndex(m_projectionTypeCombo->findData(
      static_cast<int>(ProGraphics::ProjectionType::Perspective)));
}

ControlCameraWidget::~ControlCameraWidget() {
  makeCurrent();
  m_vbo.destroy();
  delete m_texture1;
  delete m_texture2;
  doneCurrent();
}

void ControlCameraWidget::initializeGLObjects() {
  setFocusPolicy(Qt::StrongFocus);
  m_frameTimer = QElapsedTimer();
  m_frameTimer.start();
  // 创建着色器程序
  m_program = new QOpenGLShaderProgram(this);

  // 添加并编译顶点着色器
  if (!m_program->addShaderFromSourceFile(
          QOpenGLShader::Vertex,
          ":/shaders/shaders/04_coordinate/vertex.glsl")) {
    qDebug() << "Vertex Shader Error:" << m_program->log();
  }
  // 添加并编译片段着色器
  if (!m_program->addShaderFromSourceFile(
          QOpenGLShader::Fragment,
          ":/shaders/shaders/04_coordinate/fragment.glsl")) {
    qDebug() << "Fragment Shader Error:" << m_program->log();
  }
  // 链接着色器程序
  if (!m_program->link()) {
    qDebug() << "Shader Program Link Error:" << m_program->log();
  }

  float vertices[] = {-0.5f, -0.5f, -0.5f, 0.0f,  0.0f,  0.5f,  -0.5f, -0.5f,
                      1.0f,  0.0f,  0.5f,  0.5f,  -0.5f, 1.0f,  1.0f,  0.5f,
                      0.5f,  -0.5f, 1.0f,  1.0f,  -0.5f, 0.5f,  -0.5f, 0.0f,
                      1.0f,  -0.5f, -0.5f, -0.5f, 0.0f,  0.0f,

                      -0.5f, -0.5f, 0.5f,  0.0f,  0.0f,  0.5f,  -0.5f, 0.5f,
                      1.0f,  0.0f,  0.5f,  0.5f,  0.5f,  1.0f,  1.0f,  0.5f,
                      0.5f,  0.5f,  1.0f,  1.0f,  -0.5f, 0.5f,  0.5f,  0.0f,
                      1.0f,  -0.5f, -0.5f, 0.5f,  0.0f,  0.0f,

                      -0.5f, 0.5f,  0.5f,  1.0f,  0.0f,  -0.5f, 0.5f,  -0.5f,
                      1.0f,  1.0f,  -0.5f, -0.5f, -0.5f, 0.0f,  1.0f,  -0.5f,
                      -0.5f, -0.5f, 0.0f,  1.0f,  -0.5f, -0.5f, 0.5f,  0.0f,
                      0.0f,  -0.5f, 0.5f,  0.5f,  1.0f,  0.0f,

                      0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.5f,  0.5f,  -0.5f,
                      1.0f,  1.0f,  0.5f,  -0.5f, -0.5f, 0.0f,  1.0f,  0.5f,
                      -0.5f, -0.5f, 0.0f,  1.0f,  0.5f,  -0.5f, 0.5f,  0.0f,
                      0.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.0f,

                      -0.5f, -0.5f, -0.5f, 0.0f,  1.0f,  0.5f,  -0.5f, -0.5f,
                      1.0f,  1.0f,  0.5f,  -0.5f, 0.5f,  1.0f,  0.0f,  0.5f,
                      -0.5f, 0.5f,  1.0f,  0.0f,  -0.5f, -0.5f, 0.5f,  0.0f,
                      0.0f,  -0.5f, -0.5f, -0.5f, 0.0f,  1.0f,

                      -0.5f, 0.5f,  -0.5f, 0.0f,  1.0f,  0.5f,  0.5f,  -0.5f,
                      1.0f,  1.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.5f,
                      0.5f,  0.5f,  1.0f,  0.0f,  -0.5f, 0.5f,  0.5f,  0.0f,
                      0.0f,  -0.5f, 0.5f,  -0.5f, 0.0f,  1.0f

  };

  // 创建并绑定顶点数组对象（VAO）
  m_vao.create();
  m_vao.bind();

  // 创建并绑定顶点缓冲对象（VBO），并分配数据
  m_vbo.create();
  m_vbo.bind();
  m_vbo.allocate(vertices, sizeof(vertices));

  // 开启深度测试
  glEnable(GL_DEPTH_TEST);

  // 绑定着色器程序，确保属性位置查询有效
  m_program->bind();

  // 获取顶点位置属性在着色器中的位置
  int posLoc = m_program->attributeLocation("aPos");
  // 启用顶点位置属性
  m_program->enableAttributeArray(posLoc);
  // 指定顶点数据布局：偏移 0，每个顶点包含 6 个 float，取前 3 个作为位置数据
  m_program->setAttributeBuffer(posLoc, GL_FLOAT, 0, 3, 5 * sizeof(float));

  int texCoordLoc = m_program->attributeLocation("aTexCoord");
  m_program->enableAttributeArray(texCoordLoc);
  m_program->setAttributeBuffer(texCoordLoc, GL_FLOAT, 3 * sizeof(float), 2,
                                5 * sizeof(float));

  m_program->setUniformValue("texture1", 0);
  m_program->setUniformValue("texture2", 1);

  // 解绑 VBO 与 VAO（通常在设置完毕后解绑，后续绘制时再绑定）
  m_vbo.release();
  m_vao.release();
  m_program->release();

  loadTextures();
}

void ControlCameraWidget::paintGLObjects() {
  float currentFrame = m_frameTimer.elapsed() / 1000.0f;
  m_deltaTime = currentFrame - m_lastFrame;
  m_lastFrame = currentFrame;

  glClearColor(m_bgColor.x(), m_bgColor.y(), m_bgColor.z(), m_bgColor.w());
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // 设置线框模式
  if (m_wireframeMode) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  } else {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }

  // 设置 uniform 变量
  m_program->bind();

  // 更新投影矩阵
  m_program->setUniformValue("projection", m_camera.getProjectionMatrix());

  // 设置观察矩阵
  m_program->setUniformValue("view", m_camera.getViewMatrix());

  // 处理键盘输入
  if (m_pressedKeys.value(Qt::Key_W))
    m_camera.processKeyboard(ProGraphics::Camera::FORWARD, m_deltaTime);
  if (m_pressedKeys.value(Qt::Key_S))
    m_camera.processKeyboard(ProGraphics::Camera::BACKWARD, m_deltaTime);
  if (m_pressedKeys.value(Qt::Key_A))
    m_camera.processKeyboard(ProGraphics::Camera::LEFT, m_deltaTime);
  if (m_pressedKeys.value(Qt::Key_D))
    m_camera.processKeyboard(ProGraphics::Camera::RIGHT, m_deltaTime);
  if (m_pressedKeys.value(Qt::Key_Space))
    m_camera.processKeyboard(ProGraphics::Camera::UP, m_deltaTime);
  if (m_pressedKeys.value(Qt::Key_Control))
    m_camera.processKeyboard(ProGraphics::Camera::DOWN, m_deltaTime);

  // 绘制三角形：绑定着色器程序和 VAO，然后调用绘制函数
  m_texture1->bind(0); // 绑定纹理
  m_texture2->bind(1); // 绑定纹理2

  m_program->setUniformValue("tintColor", m_tintColor);
  m_program->setUniformValue("mixValue", m_mixValue);

  m_vao.bind();

  for (int i = 0; i < m_cubePositions.size(); ++i) {
    QMatrix4x4 model;
    model.translate(m_cubePositions[i] + m_translateVec);

    float angle = 20.0f * i;
    angle = m_timer.elapsed() / 40.0f; // 25.0f * time

    model.rotate(angle, QVector3D(1.0f, 0.3f, 0.5f));
    m_program->setUniformValue("model", model);

    glDrawArrays(GL_TRIANGLES, 0, 36);
  }
  m_vao.release();
  m_program->release();
}

void ControlCameraWidget::initCubePositions() {
  m_cubePositions = {
      QVector3D(0.0f, 0.0f, 0.0f),    QVector3D(2.0f, 5.0f, -15.0f),
      QVector3D(-1.5f, -2.2f, -2.5f), QVector3D(-3.8f, -2.0f, -12.3f),
      QVector3D(2.4f, -0.4f, -3.5f),  QVector3D(-1.7f, 3.0f, -7.5f),
      QVector3D(1.3f, -2.0f, -2.5f),  QVector3D(1.5f, 2.0f, -2.5f),
      QVector3D(1.5f, 0.2f, -1.5f),   QVector3D(-1.3f, 1.0f, -1.5f)};
}

void ControlCameraWidget::resizeGL(int w, int h) {
  if (m_camera.getProjectionType() ==
      ProGraphics::ProjectionType::Perspective) {
    m_camera.setPerspectiveParams(m_fovSlider->value(), float(w) / h,
                                  m_nearSlider->value() / 100.0f,
                                  m_farSlider->value());
  } else {
    float width = 10.0f;
    float height = width * h / w;
    m_camera.setOrthographicParams(
        width, height, m_nearSlider->value() / 100.0f, m_farSlider->value());
  }
  if (m_program) {
    m_program->bind();
    m_program->setUniformValue("projection", m_camera.getProjectionMatrix());
    m_program->release();
  }
  update();
}

void ControlCameraWidget::keyPressEvent(QKeyEvent *event) {
  m_pressedKeys[event->key()] = true;
  update(); // 触发重绘
}

void ControlCameraWidget::keyReleaseEvent(QKeyEvent *event) {
  m_pressedKeys[event->key()] = false;
}

void ControlCameraWidget::mousePressEvent(QMouseEvent *event) {
  m_lastMousePos = event->pos();
  m_firstMouse = false;
}

void ControlCameraWidget::mouseMoveEvent(QMouseEvent *event) {
  if (m_firstMouse) {
    m_lastMousePos = event->pos();
    m_firstMouse = false;
    return;
  }

  float xoffset = event->pos().x() - m_lastMousePos.x();
  float yoffset = m_lastMousePos.y() - event->pos().y();
  m_lastMousePos = event->pos();

  m_camera.processMouseMovement(xoffset, yoffset);
  update();
}

void ControlCameraWidget::wheelEvent(QWheelEvent *event) {
  m_camera.processMouseScroll(event->angleDelta().y() / 120.0f);
  update();
}

void ControlCameraWidget::loadTextures() {
  // 加载第一个纹理
  QImage img1(":/textures/assets/textures/wall.jpg");
  m_texture1 = new QOpenGLTexture(img1.mirrored());
  m_texture1->setWrapMode(QOpenGLTexture::Repeat);
  m_texture1->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
  m_texture1->setMagnificationFilter(QOpenGLTexture::Linear);

  // 加载第二个纹理
  QImage img2(":/textures/assets/textures/awesomeface.png");
  m_texture2 = new QOpenGLTexture(img2.mirrored());
  m_texture2->setWrapMode(QOpenGLTexture::Repeat);
  m_texture2->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
  m_texture2->setMagnificationFilter(QOpenGLTexture::Linear);
}

void ControlCameraWidget::onMixValueChanged(int value) {
  m_mixValue = value / 100.0f;
  update(); // 触发重绘
}

void ControlCameraWidget::onFovChanged(int value) {
  m_camera.setFov(value);
  update();
}

void ControlCameraWidget::onNearPlaneChanged(int value) {
  m_camera.setNearPlane(value / 100.0f);
  update();
}

void ControlCameraWidget::onFarPlaneChanged(int value) {
  m_camera.setFarPlane(value);
  update();
}

void ControlCameraWidget::onWireframeModeChanged(bool checked) {
  m_wireframeMode = checked;
  update();
}

void ControlCameraWidget::onTranslateXChanged(int value) {
  m_translateVec.setX(value / 50.0f);
  update();
}

void ControlCameraWidget::onTranslateYChanged(int value) {
  m_translateVec.setY(value / 50.0f);
  update();
}

void ControlCameraWidget::onTranslateZChanged(int value) {
  m_translateVec.setZ(value / 50.0f);
  update();
}

void ControlCameraWidget::onTintColorChanged() {
  QColor color = QColorDialog::getColor(
      QColor(m_tintColor.x() * 255, m_tintColor.y() * 255,
             m_tintColor.z() * 255, m_tintColor.w() * 255),
      this, "选择物体颜色");
  if (color.isValid()) {
    m_tintColor =
        QVector4D(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    update();
  }
}

void ControlCameraWidget::onBgColorChanged() {
  QColor color =
      QColorDialog::getColor(QColor(m_bgColor.x() * 255, m_bgColor.y() * 255,
                                    m_bgColor.z() * 255, m_bgColor.w() * 255),
                             this, "选择背景颜色");
  if (color.isValid()) {
    m_bgColor =
        QVector4D(color.redF(), color.greenF(), color.blueF(), color.alphaF());
    update();
  }
}

void ControlCameraWidget::onCameraTypeChanged(int index) {
  ProGraphics::CameraType type = static_cast<ProGraphics::CameraType>(
      m_cameraTypeCombo->itemData(index).toInt());
  m_camera.setType(type);
  update();
}

void ControlCameraWidget::onProjectionTypeChanged(int index) {
  ProGraphics::ProjectionType type = static_cast<ProGraphics::ProjectionType>(
      m_projectionTypeCombo->itemData(index).toInt());

  // 保存当前视口中心点
  QVector3D centerPoint = m_camera.getPosition() + m_camera.getFront() * 5.0f;
  float currentViewSize = m_camera.getFov() * 0.1f; // 估算当前视口大小

  m_camera.setProjectionType(type);

  bool isPerspective = (type == ProGraphics::ProjectionType::Perspective);
  m_fovSlider->setEnabled(isPerspective);

  if (isPerspective) {
    m_camera.setPerspectiveParams(45.0f, // 使用更合理的默认FOV
                                  float(width()) / height(),
                                  m_nearSlider->value() / 100.0f,
                                  m_farSlider->value());
  } else {
    // 根据当前视口大小设置正交投影参数
    float orthoWidth = currentViewSize * 2.0f;
    float orthoHeight = orthoWidth * height() / float(width());
    m_camera.setOrthographicParams(orthoWidth, orthoHeight,
                                   m_nearSlider->value() / 100.0f,
                                   m_farSlider->value());
  }
  update();
}

// 重写 resizeEvent 以保持控制面板位置
void ControlCameraWidget::resizeEvent(QResizeEvent *event) {
  BaseGLWidget::resizeEvent(event);
  if (m_controlWidget) {
    m_controlWidget->setGeometry(width() - m_controlWidget->width() - 10, 10,
                                 m_controlWidget->width(), height() - 20);
  }
}

void ControlCameraWidget::setupUI() {
  // 创建一个控制面板容器
  m_controlWidget = new QWidget(this);
  m_controlWidget->setFixedWidth(250);

  auto *layout = new QVBoxLayout(m_controlWidget);
  layout->setContentsMargins(5, 5, 5, 5);
  layout->setSpacing(5);

  // 纹理混合控制
  auto *mixGroup = new QGroupBox("纹理混合", m_controlWidget);
  auto *mixLayout = new QVBoxLayout(mixGroup);
  m_mixSlider = new QSlider(Qt::Horizontal);
  m_mixSlider->setRange(0, 100);
  m_mixSlider->setValue(20);
  mixLayout->addWidget(m_mixSlider);

  // 视角控制
  auto *viewGroup = new QGroupBox("视角控制", m_controlWidget);
  auto *viewLayout = new QVBoxLayout(viewGroup);

  m_fovSlider = new QSlider(Qt::Horizontal);
  m_fovSlider->setRange(1, 179);
  m_fovSlider->setValue(45);
  viewLayout->addWidget(new QLabel("视场角(FOV)"));
  viewLayout->addWidget(m_fovSlider);

  m_nearSlider = new QSlider(Qt::Horizontal);
  m_nearSlider->setRange(1, 100);
  m_nearSlider->setValue(10);
  viewLayout->addWidget(new QLabel("近平面"));
  viewLayout->addWidget(m_nearSlider);

  m_farSlider = new QSlider(Qt::Horizontal);
  m_farSlider->setRange(1, 1000);
  m_farSlider->setValue(100);
  viewLayout->addWidget(new QLabel("远平面"));
  viewLayout->addWidget(m_farSlider);

  // 变换控制
  auto *transformGroup = new QGroupBox("位移控制", m_controlWidget);
  auto *transformLayout = new QVBoxLayout(transformGroup);

  m_translateXSlider = new QSlider(Qt::Horizontal);
  m_translateXSlider->setRange(-100, 100);
  m_translateXSlider->setValue(0);
  transformLayout->addWidget(new QLabel("X轴位移"));
  transformLayout->addWidget(m_translateXSlider);

  m_translateYSlider = new QSlider(Qt::Horizontal);
  m_translateYSlider->setRange(-100, 100);
  m_translateYSlider->setValue(0);
  transformLayout->addWidget(new QLabel("Y轴位移"));
  transformLayout->addWidget(m_translateYSlider);

  m_translateZSlider = new QSlider(Qt::Horizontal);
  m_translateZSlider->setRange(-100, 100);
  m_translateZSlider->setValue(0);
  transformLayout->addWidget(new QLabel("Z轴位移"));
  transformLayout->addWidget(m_translateZSlider);

  // 渲染控制
  auto *renderGroup = new QGroupBox("渲染控制", m_controlWidget);
  auto *renderLayout = new QVBoxLayout(renderGroup);

  m_wireframeCheckBox = new QCheckBox("线框模式");
  renderLayout->addWidget(m_wireframeCheckBox);

  m_tintColorBtn = new QPushButton("物体颜色");
  m_bgColorBtn = new QPushButton("背景颜色");
  renderLayout->addWidget(m_tintColorBtn);
  renderLayout->addWidget(m_bgColorBtn);

  // 添加相机控制组
  auto *cameraGroup = new QGroupBox("相机控制", m_controlWidget);
  auto *cameraLayout = new QVBoxLayout(cameraGroup);

  // 相机类型选择
  m_cameraTypeCombo = new QComboBox;
  m_cameraTypeCombo->addItem("FPS相机",
                             static_cast<int>(ProGraphics::CameraType::FPS));
  m_cameraTypeCombo->addItem("轨道相机",
                             static_cast<int>(ProGraphics::CameraType::Orbit));
  m_cameraTypeCombo->addItem("自由相机",
                             static_cast<int>(ProGraphics::CameraType::Free));
  m_cameraTypeCombo->addItem("跟随相机",
                             static_cast<int>(ProGraphics::CameraType::Follow));
  cameraLayout->addWidget(new QLabel("相机类型"));
  cameraLayout->addWidget(m_cameraTypeCombo);

  // 投影类型选择
  m_projectionTypeCombo = new QComboBox;
  m_projectionTypeCombo->addItem(
      "透视投影", static_cast<int>(ProGraphics::ProjectionType::Perspective));
  m_projectionTypeCombo->addItem(
      "正交投影", static_cast<int>(ProGraphics::ProjectionType::Orthographic));
  cameraLayout->addWidget(new QLabel("投影类型"));
  cameraLayout->addWidget(m_projectionTypeCombo);

  // 添加所有组到主布局
  layout->insertWidget(0, cameraGroup);
  layout->addWidget(mixGroup);
  layout->addWidget(viewGroup);
  layout->addWidget(transformGroup);
  layout->addWidget(renderGroup);
  layout->addStretch();

  // 将控制面板放置在右上角
  m_controlWidget->setGeometry(width() - m_controlWidget->width() - 10, 10,
                               m_controlWidget->width(), height() - 20);

  // 连接信号槽
  connect(m_mixSlider, &QSlider::valueChanged, this,
          &ControlCameraWidget::onMixValueChanged);
  connect(m_fovSlider, &QSlider::valueChanged, this,
          &ControlCameraWidget::onFovChanged);
  connect(m_nearSlider, &QSlider::valueChanged, this,
          &ControlCameraWidget::onNearPlaneChanged);
  connect(m_farSlider, &QSlider::valueChanged, this,
          &ControlCameraWidget::onFarPlaneChanged);
  connect(m_wireframeCheckBox, &QCheckBox::toggled, this,
          &ControlCameraWidget::onWireframeModeChanged);
  connect(m_translateXSlider, &QSlider::valueChanged, this,
          &ControlCameraWidget::onTranslateXChanged);
  connect(m_translateYSlider, &QSlider::valueChanged, this,
          &ControlCameraWidget::onTranslateYChanged);
  connect(m_translateZSlider, &QSlider::valueChanged, this,
          &ControlCameraWidget::onTranslateZChanged);
  connect(m_tintColorBtn, &QPushButton::clicked, this,
          &ControlCameraWidget::onTintColorChanged);
  connect(m_bgColorBtn, &QPushButton::clicked, this,
          &ControlCameraWidget::onBgColorChanged);
  connect(m_cameraTypeCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ControlCameraWidget::onCameraTypeChanged);
  connect(m_projectionTypeCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ControlCameraWidget::onProjectionTypeChanged);
}
