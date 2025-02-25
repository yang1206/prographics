#include "TexturedRectWidget.h"
#include <QImageReader>

TexturedRectWidget::TexturedRectWidget(QWidget *parent)
    : BaseGLWidget(parent), m_texture(nullptr) {}

TexturedRectWidget::~TexturedRectWidget() {
  makeCurrent();
  m_vbo.destroy();
  m_ebo.destroy();
  delete m_texture;
  doneCurrent();
}

void TexturedRectWidget::initializeGLObjects() {
  // 创建着色器程序
  m_program = new QOpenGLShaderProgram(this);

  // 添加并编译顶点着色器
  if (!m_program->addShaderFromSourceFile(
          QOpenGLShader::Vertex,
          ":/shaders/shaders/01_textured_rect/vertex.glsl")) {
    qDebug() << "Vertex Shader Error:" << m_program->log();
  }
  // 添加并编译片段着色器
  if (!m_program->addShaderFromSourceFile(
          QOpenGLShader::Fragment,
          ":/shaders/shaders/01_textured_rect/fragment.glsl")) {
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

  // 解绑 VBO 与 VAO（通常在设置完毕后解绑，后续绘制时再绑定）
  m_vbo.release();
  m_ebo.release();
  m_vao.release();
  m_program->release();

  loadTexture(":/textures/assets/textures/wall.jpg");
}

void TexturedRectWidget::paintGLObjects() {
  float timeValue = m_timer.elapsed() / 1000.0f; // 获取以秒为单位的时间
  float redValue = (sin(timeValue) / 2.0f) + 0.5f;
  float greenValue = (sin(timeValue + 2.0f) / 2.0f) + 0.5f;
  float blueValue = (sin(timeValue + 4.0f) / 2.0f) + 0.5f;

  // 设置 uniform 变量
  m_program->bind();
  m_program->setUniformValue("uColor",
                             QVector3D(redValue, greenValue, blueValue));

  // 绘制三角形：绑定着色器程序和 VAO，然后调用绘制函数
  m_texture->bind(); // 绑定纹理
  m_vao.bind();
  m_ebo.bind();
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
  m_vao.release();
  m_program->release();
}

void TexturedRectWidget::loadTexture(const QString &path) {
  QImage img(path);

  m_texture = new QOpenGLTexture(img.mirrored());

  m_texture->setWrapMode(QOpenGLTexture::Repeat);
  m_texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
  m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
}
