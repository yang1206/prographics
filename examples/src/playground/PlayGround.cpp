#include "PlayGround.h"
#include <QDebug>
#include <QPainter>
#include <QTimer>

PlayGround::PlayGround(QWidget *parent) : BaseGLWidget(parent) {
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  m_camera = ProGraphics::Camera(ProGraphics::CameraType::Orbit,
                                 ProGraphics::ProjectionType::Perspective);
  m_camera.setPivotPoint(QVector3D(0, 0, 0));
  m_camera.zoom(-10.0f); // 设置初始距离，相当于之前的 m_radius = 15.0f
  m_camera.setFov(55.0f);
  m_camera.orbit(-90.0f, 0.0f);

  m_elapsedTimer = new QElapsedTimer();
  m_elapsedTimer->start();

  m_controls = std::make_unique<ProGraphics::OrbitControls>(&m_camera);
  ProGraphics::OrbitControls::ViewLimits limits;
  limits.yaw.min = -180.0f; // 水平旋转范围 ±180 度
  limits.yaw.max = 180.0f;
  limits.pitch.min = -60.0f; // 垂直旋转范围 -60 到 +60 度
  limits.pitch.max = 60.0f;
  limits.distance.min = 2.0f; // 缩放距离范围 2 到 50
  limits.distance.max = 50.0f;
  limits.enabled = true; // 启用限制
  m_controls->setViewLimits(limits);

  // 设置按键控制
  ProGraphics::OrbitControls::ButtonControls controls;
  controls.leftEnabled = true; // 允许左键旋转
  controls.rightEnabled = false; // 禁用右键平移
  controls.middleEnabled = true; // 允许中键行为
  controls.wheelEnabled = true; // 允许滚轮缩放
  m_controls->setButtonControls(controls);
  connect(m_controls.get(), &ProGraphics::OrbitControls::updated, this,
          [this]() { update(); });

  m_animationTimer = new QTimer(this);
  m_animationTimer->setInterval(16); // 约60fps
  connect(m_animationTimer, &QTimer::timeout, this, [this]() {
    m_animationAngle += 0.02f; // 调整动画速度
    update(); // 触发重绘
  });
  m_animationTimer->start();
}

PlayGround::~PlayGround() {
  makeCurrent();
  // 清理 2D 图元
  m_testLine.reset();
  m_testPoint.reset();
  m_testTriangle.reset();
  m_testRectangle.reset();
  m_testCircle.reset();

  // 清理 3D 图元
  m_testCube.reset();
  m_testCylinder.reset();
  m_testSphere.reset();
  m_testArrow.reset();
  m_cubes.clear();
  m_instancedSpheres.reset();
  m_lodSphere.reset();
  doneCurrent();
}

void PlayGround::initializeGLObjects() {
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_LINE_SMOOTH);
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glEnable(GL_PROGRAM_POINT_SIZE);
  // 创建着色器程序
  // m_program = new QOpenGLShaderProgram();
  // m_program->addShaderFromSourceFile(QOpenGLShader::Vertex,
  //                                    ":/shaders/shaders/06_prps/vertex.glsl");
  //
  // m_program->addShaderFromSourceFile(QOpenGLShader::Fragment,
  //                                    ":/shaders/shaders/06_prps/fragment.glsl");
  //
  // if (!m_program->link()) {
  //   qDebug() << "Shader Program Link Error:" << m_program->log();
  // }

  // 测试图元特性
  // 1. 测试线段特性
  m_testLine = std::make_unique<ProGraphics::Line2D>(
    QVector3D(0, 0, 0), QVector3D(2, 2, 2),
    QVector4D(1, 1, 0, 0.8f) // 黄色半透明
  );

  // 设置线型
  ProGraphics::Primitive2DStyle lineStyle;
  lineStyle.lineWidth = 3.0f;
  lineStyle.lineStyle = Qt::DotLine;
  m_testLine->setStyle(lineStyle);
  m_testLine->initialize();

  // 2. 测试点特性
  m_testPoint = std::make_unique<ProGraphics::Point2D>(
    QVector3D(1, 1, 1), QVector4D(1, 0, 1, 1), // 紫色
    20.0f);

  ProGraphics::Primitive2DStyle pointStyle;
  pointStyle.pointSize = 50.0f;
  m_testPoint->setStyle(pointStyle);
  m_testPoint->initialize();

  // 3. 测试三角形特性（带边框）
  m_testTriangle = std::make_unique<ProGraphics::Triangle2D>(
    QVector3D(0, 0, 0), QVector3D(1, 0, 0), QVector3D(0.5f, 1, 0),
    QVector4D(0, 1, 1, 0.5f) // 青色半透明
  );

  ProGraphics::Primitive2DStyle triangleStyle;
  triangleStyle.filled = true;
  triangleStyle.borderColor = QVector4D(1, 1, 1, 1); // 白色边框
  triangleStyle.borderWidth = 2.0f;
  m_testTriangle->setStyle(triangleStyle);
  m_testTriangle->initialize();

  // 4. 测试矩形特性
  m_testRectangle = std::make_unique<ProGraphics::Rectangle2D>(
    QVector3D(2, 2, 0), 1.0f, 0.5f, QVector4D(1, 0.5f, 0, 0.7f) // 橙色半透明
  );

  ProGraphics::Primitive2DStyle rectStyle;
  rectStyle.filled = true;
  rectStyle.borderColor = QVector4D(0, 0, 0, 1); // 黑色边框
  rectStyle.borderWidth = 1.0f;
  m_testRectangle->setStyle(rectStyle);
  m_testRectangle->initialize();

  // 5. 测试圆形特性
  m_testCircle = std::make_unique<ProGraphics::Circle2D>(
    QVector3D(-1, -1, 0), 0.5f, 32, QVector4D(0.5f, 0, 1, 0.6f) // 紫色半透明
  );

  ProGraphics::Primitive2DStyle circleStyle;
  circleStyle.filled = true;
  circleStyle.borderColor = QVector4D(1, 1, 0, 1); // 黄色边框
  circleStyle.borderWidth = 1.0f;
  m_testCircle->setStyle(circleStyle);
  m_testCircle->initialize();

  // 测试立方体
  m_testCube = std::make_unique<ProGraphics::Cube>(1.0f);

  // 设置材质
  ProGraphics::Material testcubematerial;
  testcubematerial.ambient = QVector4D(0.1f, 0.1f, 0.1f, 1.0f);
  testcubematerial.diffuse = QVector4D(0.8f, 0.2f, 0.2f, 1.0f); // 红色
  testcubematerial.specular = QVector4D(1.0f, 1.0f, 1.0f, 1.0f);
  testcubematerial.shininess = 32.0f;
  m_testCube->setMaterial(testcubematerial);

  // 设置位置和旋转
  m_testCube->setPosition(QVector3D(0.0f, 0.0f, 0.0f));
  m_testCube->setRotation(QQuaternion::fromEulerAngles(30.0f, 45.0f, 0.0f));

  m_testCube->initialize();

  // 测试圆柱体
  m_testCylinder = std::make_unique<ProGraphics::Cylinder>(0.3f, 1.0f, 32);
  ProGraphics::Material cylinderMaterial;
  cylinderMaterial.diffuse = QVector4D(0.2f, 0.8f, 0.2f, 1.0f); // 绿色
  m_testCylinder->setMaterial(cylinderMaterial);
  m_testCylinder->setPosition(QVector3D(-2.0f, 0.0f, 0.0f));
  m_testCylinder->initialize();

  // 测试球体
  m_testSphere = std::make_unique<ProGraphics::Sphere>(0.5f, 16, 32);
  ProGraphics::Material sphereMaterial;
  sphereMaterial.diffuse = QVector4D(0.2f, 0.2f, 0.8f, 1.0f); // 蓝色
  m_testSphere->setMaterial(sphereMaterial);
  m_testSphere->setPosition(QVector3D(2.0f, 0.0f, 0.0f));
  m_testSphere->initialize();

  // 测试箭头
  m_testArrow =
      std::make_unique<ProGraphics::Arrow>(1.0f, 0.05f, 0.2f, 0.1f, 32);
  ProGraphics::Material arrowMaterial;
  arrowMaterial.diffuse = QVector4D(0.8f, 0.8f, 0.2f, 1.0f); // 黄色
  m_testArrow->setMaterial(arrowMaterial);
  m_testArrow->setPosition(QVector3D(0.0f, 2.0f, 0.0f));
  m_testArrow->setRotation(QQuaternion::fromEulerAngles(45.0f, 0.0f, 0.0f));
  m_testArrow->initialize();

  // 创建一组立方体示例
  const int cubeCount = 5;
  for (int i = 0; i < cubeCount; i++) {
    auto cube = std::make_unique<ProGraphics::Cube>(0.5f);

    // 设置不同的材质
    ProGraphics::Material material;
    material.ambient = QVector4D(0.1f, 0.1f, 0.1f, 1.0f);
    material.diffuse =
        QVector4D(0.2f + 0.6f * (float) i / cubeCount,
                  0.8f - 0.6f * (float) i / cubeCount, 0.5f, 1.0f);
    material.specular = QVector4D(1.0f, 1.0f, 1.0f, 1.0f);
    material.shininess = 32.0f + 64.0f * (float) i / cubeCount;

    // 设置位置和旋转
    float angle = 360.0f * i / cubeCount;
    float radius = 2.0f;
    QVector3D position(radius * cos(qDegreesToRadians(angle)),
                       0.5f * sin(qDegreesToRadians(angle * 2.0f)),
                       radius * sin(qDegreesToRadians(angle)));

    cube->setMaterial(material);
    cube->setPosition(position);
    cube->setRotation(QQuaternion::fromEulerAngles(30.0f * i, 45.0f * i, 0.0f));
    cube->initialize();

    m_cubes.push_back(std::move(cube));
  }

  // 添加一个带纹理的立方体
  auto texturedCube = std::make_unique<ProGraphics::Cube>(0.8f);
  ProGraphics::Material textureMaterial;
  textureMaterial.useTexture = true;
  textureMaterial.textureImage = QImage(":/textures/assets/textures/wall.jpg");
  texturedCube->setMaterial(textureMaterial);
  texturedCube->setPosition(QVector3D(0.0f, 2.0f, 0.0f));
  texturedCube->initialize();
  m_cubes.push_back(std::move(texturedCube));

  // 添加一个线框模式的立方体
  auto wireframeCube = std::make_unique<ProGraphics::Cube>(1.0f);
  ProGraphics::Material wireframeMaterial;
  wireframeMaterial.wireframe = true;
  wireframeMaterial.wireframeColor = QVector4D(1.0f, 1.0f, 0.0f, 1.0f);
  wireframeCube->setMaterial(wireframeMaterial);
  wireframeCube->setPosition(QVector3D(-2.0f, 2.0f, 0.0f));
  wireframeCube->initialize();
  m_cubes.push_back(std::move(wireframeCube));

  m_instancedSpheres = std::make_unique<ProGraphics::Sphere>(0.2f, 16, 32);

  // 设置材质
  ProGraphics::Material instanceMaterial;

  instanceMaterial.ambient = QVector4D(0.1f, 0.1f, 0.1f, 1.0f);
  instanceMaterial.diffuse = QVector4D(0.8f, 0.3f, 0.3f, 0.8f);
  instanceMaterial.specular = QVector4D(1.0f, 1.0f, 1.0f, 1.0f);
  instanceMaterial.shininess = 32.0f;
  m_instancedSpheres->setMaterial(instanceMaterial);

  // 创建实例数据
  const int numInstances = 50; // 50个实例
  const float radius = 3.0f; // 分布半径
  const int spiralTurns = 3; // 螺旋圈数

  for (int i = 0; i < numInstances; ++i) {
    float t = static_cast<float>(i) / numInstances;
    float angle = t * M_PI * 2 * spiralTurns;
    float height = (t - 0.5f) * 4.0f; // 垂直分布范围

    ProGraphics::Transform transform;
    transform.position =
        QVector3D(radius * std::cos(angle), height, radius * std::sin(angle));

    // 添加一些随机缩放
    float scale = 0.5f + (rand() % 100) / 200.0f; // 0.5 到 1.0 之间
    transform.scale = QVector3D(scale, scale, scale);

    m_sphereInstances.push_back(transform);
  }

  m_instancedSpheres->initialize();

  m_lodSphere = std::make_unique<ProGraphics::Sphere>(1.0f, 32, 32);
  ProGraphics::Material lodSphereMaterial;
  lodSphereMaterial.diffuse = QVector4D(0.8f, 0.4f, 0.0f, 1.0f); // 橙色
  lodSphereMaterial.wireframe = true; // 使用线框模式以便观察细节层次变化
  m_lodSphere->setMaterial(lodSphereMaterial);
  m_lodSphere->setPosition(QVector3D(4.0f, 0.0f, 0.0f));

  // 设置不同的LOD级别（从高精度到低精度）
  m_lodSphere->setLODLevels({32, 16, 8, 4});
  m_lodSphere->setLODThreshold(2.0f); // 每2个单位切换一次LOD级别
  m_lodSphere->initialize();

  // 创建实例化线段
  m_instancedLines = std::make_unique<ProGraphics::Line2D>(
    QVector3D(0.0f, 0.0f, 0.0f),
    QVector3D(0.0f, 1.0f, 0.0f),
    QVector4D(1.0f, 1.0f, 1.0f, 1.0f) // 使用白色，颜色将在实例中设置
  );


  // 设置线段样式
  lineStyle.lineWidth = 2.0f;
  lineStyle.lineStyle = Qt::SolidLine;
  m_instancedLines->setStyle(lineStyle);
  m_instancedLines->initialize();

  // 创建实例数据
  // const int numInstances = 20;  // 20条线段

  const float spacing = 0.2f; // 线段间距
  const float minHeight = 0.5f; // 最小高度
  const float maxHeight = 10.0f; // 最大高度
  const float startX = 0.0f; // 起始X坐标

  for (int i = 0; i < numInstances; ++i) {
    float t = static_cast<float>(i) / (numInstances - 1);

    // 为每个实例创建一个独特的颜色
    QVector4D color(
      0.5f + 0.5f * std::sin(t * M_PI), // R
      0.5f + 0.5f * std::sin(t * M_PI * 2.0f), // G
      0.5f + 0.5f * std::sin(t * M_PI * 1.5f), // B
      0.8f // Alpha
    );

    ProGraphics::Transform2D transform;
    transform.position = QVector2D(startX + i * spacing, 0.0f);
    transform.rotation = 0.0f;
    transform.scale = QVector2D(1.0f, 1.0f); // 初始高度，将在动画中更新
    transform.color = color; // 设置实例颜色

    m_lineInstances.push_back(transform);
  }


  // 创建实例化点
  m_instancedPoints = std::make_unique<ProGraphics::Point2D>(
    QVector3D(0.0f, 0.0f, 0.0f), // 初始位置
    QVector4D(1.0f, 1.0f, 1.0f, 1.0f), // 初始颜色（白色）
    5.0f // 点大小
  );

  // 设置点的样式
  pointStyle.pointSize = 45.0f;
  m_instancedPoints->setStyle(pointStyle);
  m_instancedPoints->initialize();

  // 创建点的实例数据
  const int numPoints = 100; // 100个点
  const float spiralRadius = 3.0f; // 螺旋半径

  for (int i = 0; i < numPoints; ++i) {
    float t = static_cast<float>(i) / numPoints;
    float angle = t * M_PI * 2 * spiralTurns;

    // 创建变换数据
    ProGraphics::Transform2D transform;
    transform.position = QVector2D(
      spiralRadius * std::cos(angle) * t,
      spiralRadius * std::sin(angle) * t
    );

    // 创建渐变颜色
    transform.color = QVector4D(
      0.5f + 0.5f * std::sin(t * M_PI),
      0.5f + 0.5f * std::cos(t * M_PI),
      0.5f + 0.5f * std::sin(t * M_PI * 2),
      0.8f
    );

    m_pointInstances.push_back(transform);
  }
}

void PlayGround::paintGLObjects() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glClearColor(0.18f, 0.23f, 0.33f, 1.0f);

  // //1.先绘制不透明物体
  // m_testCube->draw(m_camera.getProjectionMatrix(), m_camera.getViewMatrix());
  // m_testCylinder->draw(m_camera.getProjectionMatrix(),
  //                      m_camera.getViewMatrix());
  // m_testSphere->draw(m_camera.getProjectionMatrix(),
  //                    m_camera.getViewMatrix());
  // m_testArrow->draw(m_camera.getProjectionMatrix(),
  //                   m_camera.getViewMatrix());
  //
  // // 更新和绘制立方体（只保留一次循环）
  // for (const auto &cube: m_cubes) {
  //   QQuaternion rotation = cube->getRotation();
  //   rotation *=
  //       QQuaternion::fromAxisAndAngle(QVector3D(0.0f, 1.0f, 0.0f), 1.0f);
  //   cube->setRotation(rotation);
  //   cube->draw(m_camera.getProjectionMatrix(), m_camera.getViewMatrix());
  // }
  //
  // // 2. 然后绘制半透明物体
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  //
  // // 2.1 绘制2D图元
  // // 创建批次渲染器
  // ProGraphics::Primitive2DBatch batch;
  // batch.begin();
  //
  // // 1. 点的效果：
  // // - 在圆形轨道上运动
  // // - 大小为 20 像素
  // // - 红色
  // QVector3D pointPos = QVector3D(2.0f * cos(m_animationAngle),
  //                                2.0f * sin(m_animationAngle), 0.0f);
  // m_testPoint->setPosition(pointPos);
  // m_testPoint->addToRenderBatch(batch);
  //
  // // 2. 线段的效果：
  // // - 像钟摆一样摆动
  // // - 虚线样式
  // // - 黄色半透明
  // float lineAngle = m_animationAngle;
  // // 使用三角函数计算旋转后的点
  // QVector3D lineStart(sin(lineAngle), -cos(lineAngle), 0.0f);
  // QVector3D lineEnd(-sin(lineAngle), cos(lineAngle), 0.0f);
  // // 缩放线段长度
  // lineStart *= 1.5f;
  // lineEnd *= 1.5f;
  // m_testLine->setPoints(lineStart, lineEnd);
  // m_testLine->addToRenderBatch(batch);
  //
  // // 3. 三角形的效果：
  // // - 呼吸式缩放
  // // - 青色半透明
  // // - 带白色边框
  // float scale = 0.5f + 0.3f * sin(m_animationAngle);
  // m_testTriangle->setPoints(QVector3D(0.0f, 0.0f, 0.0f),
  //                           QVector3D(scale, 0.0f, 0.0f),
  //                           QVector3D(scale * 0.5f, scale, 0.0f));
  // m_testTriangle->addToRenderBatch(batch);
  //
  // // 4. 矩形的效果：
  // // - 在椭圆轨道上运动
  // // - 橙色半透明
  // // - 带黑色边框
  // QVector3D rectCenter(2.0f * cos(m_animationAngle * 0.5f),
  //                      1.0f * sin(m_animationAngle * 0.5f), 0.0f);
  // m_testRectangle->setCenter(rectCenter);
  // m_testRectangle->addToRenderBatch(batch);
  //
  // // 5. 圆形的效果：
  // // - 脉动大小变化
  // // - 紫色半透明
  // // - 带黄色边框
  // float circleRadius = 0.3f + 0.2f * sin(m_animationAngle * 2.0f);
  // m_testCircle->setRadius(circleRadius);
  // m_testCircle->addToRenderBatch(batch);
  //
  // // 设置批次的全局样式
  // ProGraphics::Primitive2DStyle batchStyle;
  // batchStyle.pointSize = 10.0f;
  // batchStyle.lineWidth = 2.0f;
  // batch.setStyle(batchStyle);
  //
  // // 执行批量渲染
  // batch.end();
  // batch.draw(m_camera.getProjectionMatrix(), m_camera.getViewMatrix());
  //
  // // 2.2 更新实例化球体数据
  // m_instanceRotation += 0.01f;
  // for (size_t i = 0; i < m_sphereInstances.size(); ++i) {
  //   float t = static_cast<float>(i) / m_sphereInstances.size();
  //   float angle = m_instanceRotation + t * M_PI * 2;
  //
  //   float radius = 3.0f;
  //   m_sphereInstances[i].position =
  //       QVector3D(radius * std::cos(angle),
  //                 m_sphereInstances[i].position.y(),
  //                 radius * std::sin(angle));
  //
  //   m_sphereInstances[i].rotation = QQuaternion::fromAxisAndAngle(
  //     QVector3D(0.0f, 1.0f, 0.0f), angle * 180.0f / M_PI);
  // }
  //
  // // 2.3 绘制实例化球体
  // m_instancedSpheres->drawInstanced(m_camera.getProjectionMatrix(),
  //                                   m_camera.getViewMatrix(),
  //                                   m_sphereInstances);
  //
  // if (m_lodSphere) {
  //   m_lodSphere->updateLOD(m_camera.getPosition());
  //   m_lodSphere->draw(m_camera.getProjectionMatrix(),
  //                     m_camera.getViewMatrix());
  // }

  const float minHeight = 0.5f;
  const float maxHeight = 2.0f;
  const float animationSpeed = 2.0f; // 控制动画速度
  const float phaseOffset = M_PI / 6.0f; // 相位差，使波浪效果更明显

  for (size_t i = 0; i < m_lineInstances.size(); ++i) {
    float t = static_cast<float>(i) / m_lineInstances.size();

    // 使用时间和位置计算高度，创建波浪效果
    float height = minHeight + (maxHeight - minHeight) *
                   0.5f * (1.0f + std::sin(m_animationAngle * animationSpeed + t * M_PI * 4.0f + phaseOffset));

    // 更新高度
    m_lineInstances[i].scale.setY(height);

    // 动态更新颜色（可选）
    float colorPhase = m_animationAngle * 0.5f + t * M_PI * 2.0f;
    m_lineInstances[i].color = QVector4D(
      0.5f + 0.5f * std::sin(colorPhase),
      0.5f + 0.5f * std::sin(colorPhase + M_PI * 2.0f / 3.0f),
      0.5f + 0.5f * std::sin(colorPhase + M_PI * 4.0f / 3.0f),
      0.8f
    );
  }

  // 更新实例化线段
  m_instancedLines->drawInstanced(
    m_camera.getProjectionMatrix(),
    m_camera.getViewMatrix(),
    m_lineInstances
  );


  const float rotationSpeed = 0.5f;
  const float pulseSpeed = 2.0f;
  const float pulseScale = 0.2f;

  for (size_t i = 0; i < m_pointInstances.size(); ++i) {
    float t = static_cast<float>(i) / m_pointInstances.size();

    // 计算旋转角度
    float angle = m_animationAngle * rotationSpeed + t * M_PI * 2;

    // 更新位置（旋转运动）
    float radius = 3.0f * t; // 半径随索引变化
    QVector2D originalPos = m_pointInstances[i].position;
    float x = radius * std::cos(angle);
    float y = radius * std::sin(angle);
    m_pointInstances[i].position = QVector2D(x, y);

    // 添加呼吸效果（缩放）
    float scale = 1.0f + pulseScale * std::sin(m_animationAngle * pulseSpeed + t * M_PI * 2);
    m_pointInstances[i].scale = QVector2D(scale, scale);

    // 更新颜色（动态变化）
    float colorPhase = m_animationAngle * 0.5f + t * M_PI * 2;
    m_pointInstances[i].color = QVector4D(
      0.5f + 0.5f * std::sin(colorPhase),
      0.5f + 0.5f * std::sin(colorPhase + M_PI * 2.0f / 3.0f),
      0.5f + 0.5f * std::sin(colorPhase + M_PI * 4.0f / 3.0f),
      0.8f
    );
  }

  // 绘制实例化点
  m_instancedPoints->drawInstanced(
    m_camera.getProjectionMatrix(),
    m_camera.getViewMatrix(),
    m_pointInstances
  );

  glDisable(GL_BLEND);
}

void PlayGround::resizeGL(int w, int h) {
  m_camera.setAspectRatio(float(w) / float(h));
}

void PlayGround::mousePressEvent(QMouseEvent *event) {
  m_controls->handleMousePress(event->pos(), Qt::LeftButton);
}

void PlayGround::mouseMoveEvent(QMouseEvent *event) {
  m_controls->handleMouseMove(event->pos(), event->buttons());
}

void PlayGround::mouseReleaseEvent(QMouseEvent *event) {
  m_controls->handleMouseRelease(event->button());
}

void PlayGround::wheelEvent(QWheelEvent *event) {
  m_controls->handleWheel(event->angleDelta().y() / 120.0f);
}
