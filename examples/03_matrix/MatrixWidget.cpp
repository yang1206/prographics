#include "MatrixWidget.h"
#include <QImageReader>
#include <QLabel>
#include <QVBoxLayout>

MatrixWidget::MatrixWidget(QWidget *parent)
    : BaseGLWidget(parent), m_texture1(nullptr), m_texture2(nullptr) {
  setupUI();
}

MatrixWidget::~MatrixWidget() {
  makeCurrent();
  m_vbo.destroy();
  m_ebo.destroy();
  delete m_texture1;
  delete m_texture2;
  doneCurrent();
}

void MatrixWidget::setupUI() {
  auto *layout = new QVBoxLayout(this);

  // 纹理混合滑动条
  auto *mixLabel = new QLabel("纹理混合", this);
  m_mixSlider = new QSlider(Qt::Horizontal, this);
  m_mixSlider->setRange(0, 100);
  m_mixSlider->setValue(20);

  // 平移 X 滑动条
  auto *translateXLabel = new QLabel("X平移", this);
  m_translateXSlider = new QSlider(Qt::Horizontal, this);
  m_translateXSlider->setRange(-100, 100);
  m_translateXSlider->setValue(10);

  // 平移 Y 滑动条
  auto *translateYLabel = new QLabel("Y平移", this);
  m_translateYSlider = new QSlider(Qt::Horizontal, this);
  m_translateYSlider->setRange(-100, 100);
  m_translateYSlider->setValue(-10);

  // 旋转角度滑动条
  auto *rotateLabel = new QLabel("旋转角度", this);
  m_rotateSlider = new QSlider(Qt::Horizontal, this);
  m_rotateSlider->setRange(0, 360);
  m_rotateSlider->setValue(0);

  // 缩放 X 滑动条
  auto *scaleXLabel = new QLabel("X缩放", this);
  m_scaleXSlider = new QSlider(Qt::Horizontal, this);
  m_scaleXSlider->setRange(10, 200);
  m_scaleXSlider->setValue(100);

  // 缩放 Y 滑动条
  auto *scaleYLabel = new QLabel("Y缩放", this);
  m_scaleYSlider = new QSlider(Qt::Horizontal, this);
  m_scaleYSlider->setRange(10, 200);
  m_scaleYSlider->setValue(100);

  // 自动旋转复选框
  m_autoRotateCheckBox = new QCheckBox("自动旋转", this);
  m_autoRotateCheckBox->setChecked(true);

  // 添加到布局
  layout->addWidget(mixLabel);
  layout->addWidget(m_mixSlider);
  layout->addWidget(translateXLabel);
  layout->addWidget(m_translateXSlider);
  layout->addWidget(translateYLabel);
  layout->addWidget(m_translateYSlider);
  layout->addWidget(rotateLabel);
  layout->addWidget(m_rotateSlider);
  layout->addWidget(scaleXLabel);
  layout->addWidget(m_scaleXSlider);
  layout->addWidget(scaleYLabel);
  layout->addWidget(m_scaleYSlider);
  layout->addWidget(m_autoRotateCheckBox);
  layout->addStretch();

  // 连接信号槽
  connect(m_mixSlider, &QSlider::valueChanged, this,
          &MatrixWidget::onMixValueChanged);
  connect(m_translateXSlider, &QSlider::valueChanged, this,
          &MatrixWidget::onTranslateXChanged);
  connect(m_translateYSlider, &QSlider::valueChanged, this,
          &MatrixWidget::onTranslateYChanged);
  connect(m_rotateSlider, &QSlider::valueChanged, this,
          &MatrixWidget::onRotateAngleChanged);
  connect(m_scaleXSlider, &QSlider::valueChanged, this,
          &MatrixWidget::onScaleXChanged);
  connect(m_scaleYSlider, &QSlider::valueChanged, this,
          &MatrixWidget::onScaleYChanged);
  connect(m_autoRotateCheckBox, &QCheckBox::toggled, this,
          &MatrixWidget::onAutoRotateChanged);
}

void MatrixWidget::initializeGLObjects() {
  // 创建着色器程序
  m_program = new QOpenGLShaderProgram(this);

  // 添加并编译顶点着色器
  if (!m_program->addShaderFromSourceFile(
          QOpenGLShader::Vertex, ":/shaders/shaders/03_matrix/vertex.glsl")) {
    qDebug() << "Vertex Shader Error:" << m_program->log();
  }
  // 添加并编译片段着色器
  if (!m_program->addShaderFromSourceFile(
          QOpenGLShader::Fragment,
          ":/shaders/shaders/03_matrix/fragment.glsl")) {
    qDebug() << "Fragment Shader Error:" << m_program->log();
  }
  // 链接着色器程序
  if (!m_program->link()) {
    qDebug() << "Shader Program Link Error:" << m_program->log();
  }

  // 均为类成员变量
  float vertices[] = {
      //     ---- 位置 ----       ---- 颜色 ----     - 纹理坐标 -
      0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // 右上
      0.5f,  -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // 右下
      -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // 左下
      -0.5f, 0.5f,  0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f  // 左上
  };

  unsigned int indices[] = {
      0, 1, 3, // 第一个三角形
      1, 2, 3  // 第二个三角形
  };

  // 创建并绑定顶点数组对象（VAO）
  m_vao.create();
  m_vao.bind();

  // 创建并绑定顶点缓冲对象（VBO），并分配数据
  m_vbo.create();
  m_vbo.bind();
  m_vbo.allocate(vertices, sizeof(vertices));

  // 创建 EBO
  m_ebo.create();
  m_ebo.bind();
  m_ebo.allocate(indices, sizeof(indices));

  // 绑定着色器程序，确保属性位置查询有效
  m_program->bind();

  // 获取顶点位置属性在着色器中的位置
  int posLoc = m_program->attributeLocation("aPos");
  // 启用顶点位置属性
  m_program->enableAttributeArray(posLoc);
  // 指定顶点数据布局：偏移 0，每个顶点包含 6 个 float，取前 3 个作为位置数据
  m_program->setAttributeBuffer(posLoc, GL_FLOAT, 0, 3, 8 * sizeof(float));

  // 获取顶点颜色属性在着色器中的位置
  int colorLoc = m_program->attributeLocation("aColor");
  // 启用顶点颜色属性
  m_program->enableAttributeArray(colorLoc);
  // 指定顶点数据布局：从偏移 3*sizeof(float) 开始，每个顶点包含 6 个
  // float，取后 3 个作为颜色数据
  m_program->setAttributeBuffer(colorLoc, GL_FLOAT, 3 * sizeof(float), 3,
                                8 * sizeof(float));

  int texCoordLoc = m_program->attributeLocation("aTexCoord");
  m_program->enableAttributeArray(texCoordLoc);
  m_program->setAttributeBuffer(texCoordLoc, GL_FLOAT, 6 * sizeof(float), 2,
                                8 * sizeof(float));

  m_program->setUniformValue("texture1", 0);
  m_program->setUniformValue("texture2", 1);

  QMatrix4x4 transform;
  transform.setToIdentity();
  m_program->setUniformValue("transform", transform);

  // 解绑 VBO 与 VAO（通常在设置完毕后解绑，后续绘制时再绑定）
  m_vbo.release();
  m_ebo.release();
  m_vao.release();
  m_program->release();

  loadTextures();
}

void MatrixWidget::paintGLObjects() {
  m_program->bind();

  // 第一个矩形
  QMatrix4x4 transform;
  transform.setToIdentity();
  transform.translate(m_translateX, m_translateY, 0.0f);
  transform.translate(0.1f, 0.1f, 0.0f);
  if (m_autoRotate) {
    transform.rotate(m_timer.elapsed() / 10.0f, 0.0f, 0.0f, 1.0f);
  } else {
    transform.rotate(m_rotateAngle, 0.0f, 0.0f, 1.0f);
  }
  transform.translate(-0.1f, -0.1f, 0.0f);
  transform.scale(m_scaleX, m_scaleY, 1.0f);

  m_program->setUniformValue("transform", transform);
  m_program->setUniformValue("mixValue", m_mixValue);

  glActiveTexture(GL_TEXTURE0);
  m_texture1->bind(0);
  glActiveTexture(GL_TEXTURE1);
  m_texture2->bind(1);

  m_vao.bind();
  m_ebo.bind();
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
  m_ebo.release();
  m_vao.release();

  // 第二个矩形
  transform.setToIdentity();
  transform.translate(-0.5f, 0.5f, 0.0f);
  float scaleAmount = std::sin(m_timer.elapsed() / 1000.0f);
  transform.scale(scaleAmount, scaleAmount, scaleAmount);

  m_program->setUniformValue("transform", transform);

  m_vao.bind();
  m_ebo.bind();
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
  m_ebo.release();
  m_vao.release();

  m_texture2->release();
  m_texture1->release();
  m_program->release();

  // 触发下一帧更新
  update();
}

void MatrixWidget::loadTextures() {
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

void MatrixWidget::onMixValueChanged(int value) {
  m_mixValue = value / 100.0f;
  update(); // 触发重绘
}

void MatrixWidget::onTranslateXChanged(int value) {
  m_translateX = value / 100.0f;
  update();
}

void MatrixWidget::onTranslateYChanged(int value) {
  m_translateY = value / 100.0f;
  update();
}

void MatrixWidget::onRotateAngleChanged(int value) {
  m_rotateAngle = value;
  update();
}

void MatrixWidget::onScaleXChanged(int value) {
  m_scaleX = value / 100.0f;
  update();
}

void MatrixWidget::onScaleYChanged(int value) {
  m_scaleY = value / 100.0f;
  update();
}

void MatrixWidget::onAutoRotateChanged(bool checked) {
  m_autoRotate = checked;
  m_rotateSlider->setEnabled(!checked);
  update();
}
