#include "prographics/core/graphics/shape3d.h"

namespace ProGraphics {
std::unique_ptr<QOpenGLShaderProgram> Shape3D::s_shaderProgram;
int Shape3D::s_shaderUsers = 0;

Shape3D::Shape3D()
    : m_vbo(QOpenGLBuffer::VertexBuffer),
      m_instanceVBO(QOpenGLBuffer::VertexBuffer), m_vertexCount(0),
      m_visible(true) {
  initializeOpenGLFunctions();
  initializeShader();
}

Shape3D::~Shape3D() {
  destroy();
  releaseShader();
}

void Shape3D::initialize() {
  auto vertices = generateVertices(32); // 默认分段数
  setupBuffer(vertices, 8);             // stride = 8 (3 pos + 3 normal + 2 tex)
  m_vertexCount = vertices.size() / 8;
}

void Shape3D::initializeShader() {
  if (!s_shaderProgram) {
    s_shaderProgram = std::make_unique<QOpenGLShaderProgram>();

    // 顶点着色器
    const char *vertexShaderSource = R"(
            #version 410 core
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec3 aNormal;
            layout (location = 2) in vec2 aTexCoord;
            layout (location = 3) in mat4 instanceMatrix; // 实例化矩阵

            uniform mat4 model;
            uniform mat4 view;
            uniform mat4 projection;
            uniform bool useInstancing;

            out vec3 FragPos;
            out vec3 Normal;
            out vec2 TexCoord;

            void main() {
                mat4 modelMatrix = useInstancing ? instanceMatrix : model;
                FragPos = vec3(modelMatrix * vec4(aPos, 1.0));
                Normal = mat3(transpose(inverse(modelMatrix))) * aNormal;
                TexCoord = aTexCoord;
                gl_Position = projection * view * modelMatrix * vec4(aPos, 1.0);
            }
        )";

    // 片段着色器
    const char *fragmentShaderSource = R"(
                #version 410 core
                in vec3 FragPos;
                in vec3 Normal;
                in vec2 TexCoord;

                uniform vec4 material_ambient;
                uniform vec4 material_diffuse;
                uniform vec4 material_specular;
                uniform float material_shininess;
                uniform float material_opacity;
                uniform bool material_wireframe;
                uniform vec4 material_wireframe_color;
                uniform bool material_use_texture;
                uniform sampler2D material_texture;
                uniform vec3 lightPos;
                uniform vec3 viewPos;

                out vec4 FragColor;

                void main() {
                    if (material_wireframe) {
                        FragColor = material_wireframe_color;
                        return;
                    }

                    // 纹理颜色
                    vec4 texColor = material_use_texture ? texture(material_texture, TexCoord)
                                                       : vec4(1.0);

                    // 环境光
                    vec4 ambient = material_ambient * texColor;

                    // 漫反射
                    vec3 norm = normalize(Normal);
                    vec3 lightDir = normalize(lightPos - FragPos);
                    float diff = max(dot(norm, lightDir), 0.0);
                    vec4 diffuse = diff * material_diffuse * texColor;

                    // 镜面反射
                    vec3 viewDir = normalize(viewPos - FragPos);
                    vec3 reflectDir = reflect(-lightDir, norm);
                    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material_shininess);
                    vec4 specular = spec * material_specular;

                    vec4 result = ambient + diffuse + specular;
                    result.a = material_opacity;
                    FragColor = result;
                }
            )";

    if (!s_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                  vertexShaderSource)) {
      qDebug() << "Vertex shader compilation failed:" << s_shaderProgram->log();
      return;
    }

    if (!s_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                  fragmentShaderSource)) {
      qDebug() << "Fragment shader compilation failed:"
               << s_shaderProgram->log();
      return;
    }

    if (!s_shaderProgram->link()) {
      qDebug() << "Shader program linking failed:" << s_shaderProgram->log();
      return;
    }
  }
  s_shaderUsers++;
}

void Shape3D::releaseShader() {
  s_shaderUsers--;
  if (s_shaderUsers == 0) {
    s_shaderProgram.reset();
  }
}

void Shape3D::setMaterial(const Material &material) {
  m_material.ambient = material.ambient;
  m_material.diffuse = material.diffuse;
  m_material.specular = material.specular;
  m_material.shininess = material.shininess;
  m_material.opacity = material.opacity;
  m_material.wireframe = material.wireframe;
  m_material.wireframeColor = material.wireframeColor;
  m_material.useTexture = material.useTexture;

  // 处理纹理
  if (!material.textureImage.isNull()) {
    m_material.textureImage = material.textureImage;
    m_material.texture =
        std::make_unique<QOpenGLTexture>(m_material.textureImage);
    m_material.texture->setMinificationFilter(
        QOpenGLTexture::LinearMipMapLinear);
    m_material.texture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_material.texture->setWrapMode(QOpenGLTexture::Repeat);
    m_material.useTexture = true;
  }
}

void Shape3D::setTexture(const QImage &image) {
  if (image.isNull())
    return;

  m_material.textureImage = image;
  m_material.texture =
      std::make_unique<QOpenGLTexture>(m_material.textureImage);
  m_material.texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
  m_material.texture->setMagnificationFilter(QOpenGLTexture::Linear);
  m_material.texture->setWrapMode(QOpenGLTexture::Repeat);
  m_material.useTexture = true;
}

void Shape3D::removeTexture() {
  m_material.texture.reset();
  m_material.textureImage = QImage();
  m_material.useTexture = false;
}

void Shape3D::draw(const QMatrix4x4 &projection, const QMatrix4x4 &view) {
  if (!m_visible || !s_shaderProgram)
    return;

  s_shaderProgram->bind();

  // 设置变换矩阵
  s_shaderProgram->setUniformValue("model", m_transform.getMatrix());
  s_shaderProgram->setUniformValue("view", view);
  s_shaderProgram->setUniformValue("projection", projection);
  s_shaderProgram->setUniformValue("useInstancing", false);

  // 设置材质属性
  s_shaderProgram->setUniformValue("material_ambient", m_material.ambient);
  s_shaderProgram->setUniformValue("material_diffuse", m_material.diffuse);
  s_shaderProgram->setUniformValue("material_specular", m_material.specular);
  s_shaderProgram->setUniformValue("material_shininess", m_material.shininess);
  s_shaderProgram->setUniformValue("material_opacity", m_material.opacity);
  s_shaderProgram->setUniformValue("material_wireframe", m_material.wireframe);
  s_shaderProgram->setUniformValue("material_wireframe_color",
                                   m_material.wireframeColor);
  s_shaderProgram->setUniformValue("material_use_texture",
                                   m_material.useTexture);

  // 设置光照参数（这里使用简单的定点光源）
  s_shaderProgram->setUniformValue("lightPos", QVector3D(5.0f, 5.0f, 5.0f));
  s_shaderProgram->setUniformValue("viewPos", QVector3D(0.0f, 0.0f, 5.0f));

  // 绑定纹理
  if (m_material.useTexture && m_material.texture) {
    m_material.texture->bind();
  }

  m_vao.bind();
  if (m_material.wireframe) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }
  glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
  if (m_material.wireframe) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }
  m_vao.release();
  if (m_material.useTexture && m_material.texture) {
    m_material.texture->release();
  }

  s_shaderProgram->release();
}

void Shape3D::drawInstanced(const QMatrix4x4 &projection,
                            const QMatrix4x4 &view,
                            const std::vector<Transform> &instances) {
  if (!m_visible || !s_shaderProgram || instances.empty())
    return;

  if (!m_instancedMode) {
    initializeInstanceBuffer();
  }
  updateInstanceData(instances);

  s_shaderProgram->bind();

  s_shaderProgram->setUniformValue("model", m_transform.getMatrix());
  s_shaderProgram->setUniformValue("view", view);
  s_shaderProgram->setUniformValue("projection", projection);
  s_shaderProgram->setUniformValue("useInstancing", true);

  // 设置材质属性
  s_shaderProgram->setUniformValue("material_ambient", m_material.ambient);
  s_shaderProgram->setUniformValue("material_diffuse", m_material.diffuse);
  s_shaderProgram->setUniformValue("material_specular", m_material.specular);
  s_shaderProgram->setUniformValue("material_shininess", m_material.shininess);
  s_shaderProgram->setUniformValue("material_opacity", m_material.opacity);
  s_shaderProgram->setUniformValue("material_wireframe", m_material.wireframe);
  s_shaderProgram->setUniformValue("material_wireframe_color",
                                   m_material.wireframeColor);
  s_shaderProgram->setUniformValue("material_use_texture",
                                   m_material.useTexture);

  // 设置光照参数（这里使用简单的定点光源）
  s_shaderProgram->setUniformValue("lightPos", QVector3D(5.0f, 5.0f, 5.0f));
  s_shaderProgram->setUniformValue("viewPos", QVector3D(0.0f, 0.0f, 5.0f));

  // 绑定纹理
  if (m_material.useTexture && m_material.texture) {
    m_material.texture->bind();
  }
  m_vao.bind();
  m_instanceVBO.bind();
  if (m_material.wireframe) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  }
  glDrawArraysInstanced(GL_TRIANGLES, 0, m_vertexCount, instances.size());
  if (m_material.wireframe) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }
  m_instanceVBO.release();
  m_vao.release();
  s_shaderProgram->release();
}

void Shape3D::initializeInstanceBuffer() {
  m_vao.bind();
  m_instanceVBO.create();
  m_instanceVBO.bind();

  // 为实例矩阵预分配空间
  m_instanceVBO.allocate(1024 * sizeof(QMatrix4x4)); // 预分配1024个实例的空间

  // 设置实例化属性
  for (int i = 0; i < 4; i++) {
    glEnableVertexAttribArray(3 + i);
    glVertexAttribPointer(3 + i, 4, GL_FLOAT, GL_FALSE, sizeof(QMatrix4x4),
                          reinterpret_cast<void *>(i * sizeof(QVector4D)));
    glVertexAttribDivisor(3 + i, 1);
  }

  m_instanceVBO.release();
  m_vao.release();
  m_instancedMode = true;
}

void Shape3D::updateInstanceData(const std::vector<Transform> &instances) {
  if (!m_instancedMode)
    return;

  std::vector<QMatrix4x4> matrices;
  matrices.reserve(instances.size());
  for (const auto &transform : instances) {
    matrices.push_back(transform.getMatrix());
  }

  m_instanceVBO.bind();
  m_instanceVBO.allocate(matrices.data(), matrices.size() * sizeof(QMatrix4x4));
  m_instanceVBO.release();
}

void Shape3D::setLODLevels(const std::vector<int> &segmentCounts) {
  m_lodSegments = segmentCounts;
  if (!m_lodSegments.empty()) {
    initialize(); // 重新初始化几何体
  }
}

void Shape3D::updateLOD(const QVector3D &cameraPos) {
  if (m_lodSegments.empty())
    return;

  float distance = (cameraPos - m_transform.position).length();
  int newLOD = getOptimalSegmentCount(distance);

  if (newLOD != m_currentLOD) {
    m_currentLOD = newLOD;
    auto vertices = generateVertices(m_currentLOD);
    setupBuffer(vertices, 8); // stride = 8 (3 pos + 3 normal + 2 tex)
    m_vertexCount = vertices.size() / 8;
  }
}

int Shape3D::getOptimalSegmentCount(float distance) const {
  if (m_lodSegments.empty())
    return 32; // 默认细分数

  int index = static_cast<int>(distance / m_lodThreshold);
  index = std::min(index, static_cast<int>(m_lodSegments.size()) - 1);
  return m_lodSegments[index];
}

void Shape3D::destroy() {
  if (m_vbo.isCreated()) {
    m_vbo.destroy();
  }
  if (m_vao.isCreated()) {
    m_vao.destroy();
  }
}

void Shape3D::setupBuffer(const std::vector<float> &vertices, int stride) {
  if (!m_vao.isCreated()) {
    m_vao.create();
  }
  m_vao.bind();

  if (!m_vbo.isCreated()) {
    m_vbo.create();
  }
  m_vbo.bind();
  m_vbo.allocate(vertices.data(), vertices.size() * sizeof(float));

  // 位置
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float),
                        nullptr);

  // 法线
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float),
                        reinterpret_cast<void *>(3 * sizeof(float)));

  // 纹理坐标
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride * sizeof(float),
                        reinterpret_cast<void *>(6 * sizeof(float)));

  m_vbo.release();
  m_vao.release();
}

Cube::Cube(float size) : m_size(size) {}

void Cube::initialize() {
  // 立方体的顶点数据（位置和法线）
  std::vector<float> vertices = {
      // 位置              // 法线
      // 前面
      -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
      0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
      -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f,

      // 后面
      -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
      -1.0f, 0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.5f, 0.5f, -0.5f, 0.0f,
      0.0f, -1.0f, -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, -0.5f, -0.5f, -0.5f,
      0.0f, 0.0f, -1.0f,

      // 左面
      -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, -0.5f, 0.5f, -0.5f, -1.0f, 0.0f,
      0.0f, -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, -0.5f, -0.5f, -0.5f, -1.0f,
      0.0f, 0.0f, -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, -0.5f, 0.5f, 0.5f,
      -1.0f, 0.0f, 0.0f,

      // 右面
      0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
      0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
      0.0f, 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
      0.0f,

      // 底面
      -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.5f, -0.5f, -0.5f, 0.0f, -1.0f,
      0.0f, 0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.5f, -0.5f, 0.5f, 0.0f,
      -1.0f, 0.0f, -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, -0.5f, -0.5f, -0.5f,
      0.0f, -1.0f, 0.0f,

      // 顶面
      -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
      0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
      -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
      0.0f};

  // 应用尺寸缩放
  for (size_t i = 0; i < vertices.size(); i += 6) {
    vertices[i] *= m_size;
    vertices[i + 1] *= m_size;
    vertices[i + 2] *= m_size;
  }
  setupBuffer(vertices, 6); // stride = 6 (3 for position + 3 for normal)
  m_vertexCount = 36;       // 6 faces * 2 triangles * 3 vertices
}

// 圆柱体实现
Cylinder::Cylinder(float radius, float height, int segments)
    : m_radius(radius), m_height(height), m_segments(segments) {}

void Cylinder::initialize() {
  std::vector<float> vertices;
  float halfHeight = m_height * 0.5f;

  // 侧面顶点
  for (int i = 0; i < m_segments; ++i) {
    float angle1 = 2.0f * M_PI * i / m_segments;
    float angle2 = 2.0f * M_PI * (i + 1) / m_segments;
    float x1 = cos(angle1);
    float z1 = sin(angle1);
    float x2 = cos(angle2);
    float z2 = sin(angle2);

    // 第一个三角形
    // 底部顶点1
    vertices.push_back(x1 * m_radius);
    vertices.push_back(-halfHeight);
    vertices.push_back(z1 * m_radius);
    vertices.push_back(x1);
    vertices.push_back(0.0f);
    vertices.push_back(z1);

    // 顶部顶点1
    vertices.push_back(x1 * m_radius);
    vertices.push_back(halfHeight);
    vertices.push_back(z1 * m_radius);
    vertices.push_back(x1);
    vertices.push_back(0.0f);
    vertices.push_back(z1);

    // 底部顶点2
    vertices.push_back(x2 * m_radius);
    vertices.push_back(-halfHeight);
    vertices.push_back(z2 * m_radius);
    vertices.push_back(x2);
    vertices.push_back(0.0f);
    vertices.push_back(z2);

    // 第二个三角形
    // 顶部顶点1
    vertices.push_back(x1 * m_radius);
    vertices.push_back(halfHeight);
    vertices.push_back(z1 * m_radius);
    vertices.push_back(x1);
    vertices.push_back(0.0f);
    vertices.push_back(z1);

    // 顶部顶点2
    vertices.push_back(x2 * m_radius);
    vertices.push_back(halfHeight);
    vertices.push_back(z2 * m_radius);
    vertices.push_back(x2);
    vertices.push_back(0.0f);
    vertices.push_back(z2);

    // 底部顶点2
    vertices.push_back(x2 * m_radius);
    vertices.push_back(-halfHeight);
    vertices.push_back(z2 * m_radius);
    vertices.push_back(x2);
    vertices.push_back(0.0f);
    vertices.push_back(z2);
  }

  // 顶面
  for (int i = 0; i < m_segments; ++i) {
    float angle1 = 2.0f * M_PI * i / m_segments;
    float angle2 = 2.0f * M_PI * (i + 1) / m_segments;
    float x1 = cos(angle1) * m_radius;
    float z1 = sin(angle1) * m_radius;
    float x2 = cos(angle2) * m_radius;
    float z2 = sin(angle2) * m_radius;

    // 中心点
    vertices.push_back(0.0f);
    vertices.push_back(halfHeight);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(1.0f);
    vertices.push_back(0.0f);

    // 边缘点1
    vertices.push_back(x1);
    vertices.push_back(halfHeight);
    vertices.push_back(z1);
    vertices.push_back(0.0f);
    vertices.push_back(1.0f);
    vertices.push_back(0.0f);

    // 边缘点2
    vertices.push_back(x2);
    vertices.push_back(halfHeight);
    vertices.push_back(z2);
    vertices.push_back(0.0f);
    vertices.push_back(1.0f);
    vertices.push_back(0.0f);
  }

  // 底面
  for (int i = 0; i < m_segments; ++i) {
    float angle1 = 2.0f * M_PI * i / m_segments;
    float angle2 = 2.0f * M_PI * (i + 1) / m_segments;
    float x1 = cos(angle1) * m_radius;
    float z1 = sin(angle1) * m_radius;
    float x2 = cos(angle2) * m_radius;
    float z2 = sin(angle2) * m_radius;

    // 中心点
    vertices.push_back(0.0f);
    vertices.push_back(-halfHeight);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(-1.0f);
    vertices.push_back(0.0f);

    // 边缘点1
    vertices.push_back(x1);
    vertices.push_back(-halfHeight);
    vertices.push_back(z1);
    vertices.push_back(0.0f);
    vertices.push_back(-1.0f);
    vertices.push_back(0.0f);

    // 边缘点2
    vertices.push_back(x2);
    vertices.push_back(-halfHeight);
    vertices.push_back(z2);
    vertices.push_back(0.0f);
    vertices.push_back(-1.0f);
    vertices.push_back(0.0f);
  }

  setupBuffer(vertices, 6);
  m_vertexCount = vertices.size() / 6;
}

// 球体实现
Sphere::Sphere(float radius, int rings, int sectors)
    : m_radius(radius), m_rings(rings), m_sectors(sectors) {}

void Sphere::initialize() {
  std::vector<float> vertices;

  for (int r = 0; r <= m_rings; ++r) {
    float phi = M_PI * r / m_rings;
    float cosPhi = cos(phi);
    float sinPhi = sin(phi);

    for (int s = 0; s <= m_sectors; ++s) {
      float theta = 2.0f * M_PI * s / m_sectors;
      float cosTheta = cos(theta);
      float sinTheta = sin(theta);

      float x = cosTheta * sinPhi;
      float y = cosPhi;
      float z = sinTheta * sinPhi;

      // 顶点位置
      vertices.push_back(x * m_radius);
      vertices.push_back(y * m_radius);
      vertices.push_back(z * m_radius);

      // 法线（归一化的位置即为法线）
      vertices.push_back(x);
      vertices.push_back(y);
      vertices.push_back(z);
    }
  }

  // 生成索引
  std::vector<unsigned int> indices;
  for (int r = 0; r < m_rings; ++r) {
    for (int s = 0; s < m_sectors; ++s) {
      int first = r * (m_sectors + 1) + s;
      int second = first + m_sectors + 1;

      vertices.push_back(vertices[first * 6]);
      vertices.push_back(vertices[first * 6 + 1]);
      vertices.push_back(vertices[first * 6 + 2]);
      vertices.push_back(vertices[first * 6 + 3]);
      vertices.push_back(vertices[first * 6 + 4]);
      vertices.push_back(vertices[first * 6 + 5]);

      vertices.push_back(vertices[second * 6]);
      vertices.push_back(vertices[second * 6 + 1]);
      vertices.push_back(vertices[second * 6 + 2]);
      vertices.push_back(vertices[second * 6 + 3]);
      vertices.push_back(vertices[second * 6 + 4]);
      vertices.push_back(vertices[second * 6 + 5]);

      vertices.push_back(vertices[(first + 1) * 6]);
      vertices.push_back(vertices[(first + 1) * 6 + 1]);
      vertices.push_back(vertices[(first + 1) * 6 + 2]);
      vertices.push_back(vertices[(first + 1) * 6 + 3]);
      vertices.push_back(vertices[(first + 1) * 6 + 4]);
      vertices.push_back(vertices[(first + 1) * 6 + 5]);

      vertices.push_back(vertices[second * 6]);
      vertices.push_back(vertices[second * 6 + 1]);
      vertices.push_back(vertices[second * 6 + 2]);
      vertices.push_back(vertices[second * 6 + 3]);
      vertices.push_back(vertices[second * 6 + 4]);
      vertices.push_back(vertices[second * 6 + 5]);

      vertices.push_back(vertices[(second + 1) * 6]);
      vertices.push_back(vertices[(second + 1) * 6 + 1]);
      vertices.push_back(vertices[(second + 1) * 6 + 2]);
      vertices.push_back(vertices[(second + 1) * 6 + 3]);
      vertices.push_back(vertices[(second + 1) * 6 + 4]);
      vertices.push_back(vertices[(second + 1) * 6 + 5]);

      vertices.push_back(vertices[(first + 1) * 6]);
      vertices.push_back(vertices[(first + 1) * 6 + 1]);
      vertices.push_back(vertices[(first + 1) * 6 + 2]);
      vertices.push_back(vertices[(first + 1) * 6 + 3]);
      vertices.push_back(vertices[(first + 1) * 6 + 4]);
      vertices.push_back(vertices[(first + 1) * 6 + 5]);
    }
  }

  setupBuffer(vertices, 6);
  m_vertexCount = vertices.size() / 6;
}

// 箭头实现
Arrow::Arrow(float length, float shaftRadius, float headLength,
             float headRadius, int segments)
    : m_length(length), m_shaftRadius(shaftRadius), m_headLength(headLength),
      m_headRadius(headRadius), m_segments(segments) {}

void Arrow::initialize() {
  std::vector<float> vertices;
  float shaftLength = m_length - m_headLength;

  // 箭身（圆柱体部分）
  for (int i = 0; i < m_segments; ++i) {
    float angle1 = 2.0f * M_PI * i / m_segments;
    float angle2 = 2.0f * M_PI * (i + 1) / m_segments;
    float x1 = cos(angle1);
    float z1 = sin(angle1);
    float x2 = cos(angle2);
    float z2 = sin(angle2);

    // 第一个三角形
    vertices.push_back(x1 * m_shaftRadius);
    vertices.push_back(0.0f);
    vertices.push_back(z1 * m_shaftRadius);
    vertices.push_back(x1);
    vertices.push_back(0.0f);
    vertices.push_back(z1);

    vertices.push_back(x1 * m_shaftRadius);
    vertices.push_back(shaftLength);
    vertices.push_back(z1 * m_shaftRadius);
    vertices.push_back(x1);
    vertices.push_back(0.0f);
    vertices.push_back(z1);

    vertices.push_back(x2 * m_shaftRadius);
    vertices.push_back(0.0f);
    vertices.push_back(z2 * m_shaftRadius);
    vertices.push_back(x2);
    vertices.push_back(0.0f);
    vertices.push_back(z2);

    // 第二个三角形
    vertices.push_back(x1 * m_shaftRadius);
    vertices.push_back(shaftLength);
    vertices.push_back(z1 * m_shaftRadius);
    vertices.push_back(x1);
    vertices.push_back(0.0f);
    vertices.push_back(z1);

    vertices.push_back(x2 * m_shaftRadius);
    vertices.push_back(shaftLength);
    vertices.push_back(z2 * m_shaftRadius);
    vertices.push_back(x2);
    vertices.push_back(0.0f);
    vertices.push_back(z2);

    vertices.push_back(x2 * m_shaftRadius);
    vertices.push_back(0.0f);
    vertices.push_back(z2 * m_shaftRadius);
    vertices.push_back(x2);
    vertices.push_back(0.0f);
    vertices.push_back(z2);
  }

  // 箭头（圆锥体部分）
  for (int i = 0; i < m_segments; ++i) {
    float angle1 = 2.0f * M_PI * i / m_segments;
    float angle2 = 2.0f * M_PI * (i + 1) / m_segments;
    float x1 = cos(angle1);
    float z1 = sin(angle1);
    float x2 = cos(angle2);
    float z2 = sin(angle2);

    // 计算圆锥体侧面的法线
    QVector3D p1(x1 * m_headRadius, 0, z1 * m_headRadius);
    QVector3D p2(0, m_headLength, 0);
    QVector3D p3(x2 * m_headRadius, 0, z2 * m_headRadius);

    QVector3D normal = QVector3D::crossProduct(p2 - p1, p3 - p1).normalized();

    // 圆锥体侧面三角形
    vertices.push_back(x1 * m_headRadius);
    vertices.push_back(shaftLength);
    vertices.push_back(z1 * m_headRadius);
    vertices.push_back(normal.x());
    vertices.push_back(normal.y());
    vertices.push_back(normal.z());

    vertices.push_back(0.0f);
    vertices.push_back(m_length);
    vertices.push_back(0.0f);
    vertices.push_back(normal.x());
    vertices.push_back(normal.y());
    vertices.push_back(normal.z());

    vertices.push_back(x2 * m_headRadius);
    vertices.push_back(shaftLength);
    vertices.push_back(z2 * m_headRadius);
    vertices.push_back(normal.x());
    vertices.push_back(normal.y());
    vertices.push_back(normal.z());
  }

  // 箭头底面
  for (int i = 0; i < m_segments; ++i) {
    float angle1 = 2.0f * M_PI * i / m_segments;
    float angle2 = 2.0f * M_PI * (i + 1) / m_segments;
    float x1 = cos(angle1) * m_headRadius;
    float z1 = sin(angle1) * m_headRadius;
    float x2 = cos(angle2) * m_headRadius;
    float z2 = sin(angle2) * m_headRadius;

    vertices.push_back(0.0f);
    vertices.push_back(shaftLength);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(-1.0f);
    vertices.push_back(0.0f);

    vertices.push_back(x1);
    vertices.push_back(shaftLength);
    vertices.push_back(z1);
    vertices.push_back(0.0f);
    vertices.push_back(-1.0f);
    vertices.push_back(0.0f);

    vertices.push_back(x2);
    vertices.push_back(shaftLength);
    vertices.push_back(z2);
    vertices.push_back(0.0f);
    vertices.push_back(-1.0f);
    vertices.push_back(0.0f);
  }

  setupBuffer(vertices, 6);
  m_vertexCount = vertices.size() / 6;
}
} // namespace ProGraphics
