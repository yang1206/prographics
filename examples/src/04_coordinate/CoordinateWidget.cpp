#include "CoordinateWidget.h"

CoordinateWidget::CoordinateWidget(QWidget *parent)
  : BaseGLWidget(parent), m_texture1(nullptr), m_texture2(nullptr) {
  initCubePositions();
  setupUI();
}

CoordinateWidget::~CoordinateWidget() {
  makeCurrent();
  m_vbo.destroy();
  delete m_texture1;
  delete m_texture2;
  doneCurrent();
}

void CoordinateWidget::initializeGLObjects() {
  m_cameraTimer = QElapsedTimer();
  m_cameraTimer.start();
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

  float vertices[] = {
    -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.5f, -0.5f, -0.5f,
    1.0f, 0.0f, 0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.5f,
    0.5f, -0.5f, 1.0f, 1.0f, -0.5f, 0.5f, -0.5f, 0.0f,
    1.0f, -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,

    -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.5f, -0.5f, 0.5f,
    1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0.5f,
    0.5f, 0.5f, 1.0f, 1.0f, -0.5f, 0.5f, 0.5f, 0.0f,
    1.0f, -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,

    -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, -0.5f, 0.5f, -0.5f,
    1.0f, 1.0f, -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, -0.5f,
    -0.5f, -0.5f, 0.0f, 1.0f, -0.5f, -0.5f, 0.5f, 0.0f,
    0.0f, -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

    0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.5f, 0.5f, -0.5f,
    1.0f, 1.0f, 0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f,
    -0.5f, -0.5f, 0.0f, 1.0f, 0.5f, -0.5f, 0.5f, 0.0f,
    0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

    -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f, -0.5f, -0.5f,
    1.0f, 1.0f, 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.5f,
    -0.5f, 0.5f, 1.0f, 0.0f, -0.5f, -0.5f, 0.5f, 0.0f,
    0.0f, -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,

    -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.5f, 0.5f, -0.5f,
    1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.5f,
    0.5f, 0.5f, 1.0f, 0.0f, -0.5f, 0.5f, 0.5f, 0.0f,
    0.0f, -0.5f, 0.5f, -0.5f, 0.0f, 1.0f

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

  // 设置投影矩阵
  QMatrix4x4 projection;
  projection.perspective(m_fov, width() / (float) height(), m_nearPlane,
                         m_farPlane);
  m_program->setUniformValue("projection", projection);

  // 设置观察矩阵
  QMatrix4x4 view;
  view.translate(0.0f, 0.0f, -3.0f);
  m_program->setUniformValue("view", view);

  // 解绑 VBO 与 VAO（通常在设置完毕后解绑，后续绘制时再绑定）
  m_vbo.release();
  m_vao.release();
  m_program->release();

  loadTextures();
}

void CoordinateWidget::paintGLObjects() {
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
  QMatrix4x4 projection;
  projection.perspective(m_fov, width() / (float) height(), m_nearPlane,
                         m_farPlane);
  m_program->setUniformValue("projection", projection);

  // 更新观察矩阵
  float time = m_cameraTimer.elapsed() / 1000.0f; // 转换为秒
  float camX = qSin(time) * m_radius;
  float camZ = qCos(time) * m_radius;

  QVector3D cameraPos(camX, 0.0f, camZ);
  QVector3D cameraTarget(0.0f, 0.0f, 0.0f);
  QVector3D upVector(0.0f, 1.0f, 0.0f);

  // 设置观察矩阵
  QMatrix4x4 view;
  view.lookAt(cameraPos, cameraTarget, upVector);
  m_program->setUniformValue("view", view);

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
    // if (i % 3 == 0) {
    angle = m_timer.elapsed() / 40.0f; // 25.0f * time
    // }

    model.rotate(angle, QVector3D(1.0f, 0.3f, 0.5f));
    m_program->setUniformValue("model", model);

    glDrawArrays(GL_TRIANGLES, 0, 36);
  }
  m_vao.release();
  m_program->release();
}

void CoordinateWidget::initCubePositions() {
  m_cubePositions = {
    QVector3D(0.0f, 0.0f, 0.0f), QVector3D(2.0f, 5.0f, -15.0f),
    QVector3D(-1.5f, -2.2f, -2.5f), QVector3D(-3.8f, -2.0f, -12.3f),
    QVector3D(2.4f, -0.4f, -3.5f), QVector3D(-1.7f, 3.0f, -7.5f),
    QVector3D(1.3f, -2.0f, -2.5f), QVector3D(1.5f, 2.0f, -2.5f),
    QVector3D(1.5f, 0.2f, -1.5f), QVector3D(-1.3f, 1.0f, -1.5f)
  };
}

void CoordinateWidget::resizeGL(int w, int h) {
  if (m_program) {
    m_program->bind();
    QMatrix4x4 projection;
    projection.perspective(m_fov, w / (float) h, m_nearPlane, m_farPlane);
    m_program->setUniformValue("projection", projection);
    m_program->release();
  }
}

void CoordinateWidget::loadTextures() {
  // 加载第一个纹理
  QImage img1(":/textures/assets/textures/wall.jpg");
  m_texture1 = new QOpenGLTexture(img1.mirrored());
  m_texture1->setWrapMode(QOpenGLTexture::Repeat);
  m_texture1->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
  m_texture1->setMagnificationFilter(QOpenGLTexture::Linear);

  // 加载第二个纹理
  QImage img2(":/textures/assets/textures/OIP.jpg");
  m_texture2 = new QOpenGLTexture(img2.mirrored());
  m_texture2->setWrapMode(QOpenGLTexture::Repeat);
  m_texture2->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
  m_texture2->setMagnificationFilter(QOpenGLTexture::Linear);
}

void CoordinateWidget::onMixValueChanged(int value) {
  m_mixValue = value / 100.0f;
  update(); // 触发重绘
}

void CoordinateWidget::onFovChanged(int value) {
  m_fov = value;
  update();
}

void CoordinateWidget::onNearPlaneChanged(int value) {
  m_nearPlane = value / 100.0f;
  update();
}

void CoordinateWidget::onFarPlaneChanged(int value) {
  m_farPlane = value;
  update();
}

void CoordinateWidget::onWireframeModeChanged(bool checked) {
  m_wireframeMode = checked;
  update();
}

void CoordinateWidget::onTranslateXChanged(int value) {
  m_translateVec.setX(value / 50.0f);
  update();
}

void CoordinateWidget::onTranslateYChanged(int value) {
  m_translateVec.setY(value / 50.0f);
  update();
}

void CoordinateWidget::onTranslateZChanged(int value) {
  m_translateVec.setZ(value / 50.0f);
  update();
}

void CoordinateWidget::onTintColorChanged() {
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

void CoordinateWidget::onBgColorChanged() {
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

// 重写 resizeEvent 以保持控制面板位置
void CoordinateWidget::resizeEvent(QResizeEvent *event) {
  BaseGLWidget::resizeEvent(event);
  if (m_controlWidget) {
    m_controlWidget->setGeometry(width() - m_controlWidget->width() - 10, 10,
                                 m_controlWidget->width(), height() - 20);
  }
}

void CoordinateWidget::setupUI() {
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

  // 添加所有组到主布局
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
          &CoordinateWidget::onMixValueChanged);
  connect(m_fovSlider, &QSlider::valueChanged, this,
          &CoordinateWidget::onFovChanged);
  connect(m_nearSlider, &QSlider::valueChanged, this,
          &CoordinateWidget::onNearPlaneChanged);
  connect(m_farSlider, &QSlider::valueChanged, this,
          &CoordinateWidget::onFarPlaneChanged);
  connect(m_wireframeCheckBox, &QCheckBox::toggled, this,
          &CoordinateWidget::onWireframeModeChanged);
  connect(m_translateXSlider, &QSlider::valueChanged, this,
          &CoordinateWidget::onTranslateXChanged);
  connect(m_translateYSlider, &QSlider::valueChanged, this,
          &CoordinateWidget::onTranslateYChanged);
  connect(m_translateZSlider, &QSlider::valueChanged, this,
          &CoordinateWidget::onTranslateZChanged);
  connect(m_tintColorBtn, &QPushButton::clicked, this,
          &CoordinateWidget::onTintColorChanged);
  connect(m_bgColorBtn, &QPushButton::clicked, this,
          &CoordinateWidget::onBgColorChanged);
}
