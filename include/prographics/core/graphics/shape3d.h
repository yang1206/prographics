#pragma once
#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QQuaternion>
#include <QOpenGLVertexArrayObject>

namespace ProGraphics {
    /**
     * @brief 3D图形的材质结构
     *
     * 定义了3D图形的外观属性，包括光照反应、透明度和纹理
     */
    struct Material {
        QVector4D ambient{0.2f, 0.2f, 0.2f, 1.0f}; ///< 环境光颜色
        QVector4D diffuse{0.8f, 0.8f, 0.8f, 1.0f}; ///< 漫反射颜色
        QVector4D specular{1.0f, 1.0f, 1.0f, 1.0f}; ///< 镜面反射颜色
        float shininess = 32.0f; ///< 镜面反射强度
        float opacity = 1.0f; ///< 透明度
        bool wireframe = false; ///< 线框模式
        QVector4D wireframeColor{0.0f, 0.0f, 0.0f, 1.0f}; ///< 线框颜色
        std::unique_ptr<QOpenGLTexture> texture; ///< 纹理对象
        bool useTexture = false; ///< 是否使用纹理
        QImage textureImage; ///< 纹理图像
    };


    /**
 * @brief 3D图形的变换结构
 *
 * 定义了3D图形在空间中的位置、旋转和缩放
 */
    struct Transform {
        QVector3D position{0.0f, 0.0f, 0.0f}; ///< 位置
        QVector3D scale{1.0f, 1.0f, 1.0f}; ///< 缩放
        QQuaternion rotation; ///< 旋转
        /**
         * @brief 获取变换矩阵
         * @return 组合后的4x4变换矩阵
         */
        QMatrix4x4 getMatrix() const {
            QMatrix4x4 matrix;
            matrix.translate(position);
            matrix.rotate(rotation);
            matrix.scale(scale);
            return matrix;
        }
    };

    /**
 * @brief 3D图形基类
 *
 * 提供3D图形的基本功能，包括：
 * - 渲染和材质管理
 * - 空间变换
 * - LOD（细节层次）控制
 * - 实例化渲染
 */
    class Shape3D : protected QOpenGLExtraFunctions {
    public:
        Shape3D();

        virtual ~Shape3D();

        virtual void initialize();

        /**
  * @brief 渲染图形
  * @param projection 投影矩阵
  * @param view 视图矩阵
  */
        virtual void draw(const QMatrix4x4 &projection, const QMatrix4x4 &view);

        virtual void destroy();

        // 变换相关方法
        void setPosition(const QVector3D &position) {
            m_transform.position = position;
        }

        void setRotation(const QQuaternion &rotation) {
            m_transform.rotation = rotation;
        }

        void setScale(const QVector3D &scale) { m_transform.scale = scale; }
        void setTransform(const Transform &transform) { m_transform = transform; }

        void setMaterial(const Material &material);

        void setVisible(bool visible) { m_visible = visible; }

        // 纹理相关
        void setTexture(const QImage &image);

        void removeTexture();

        /**
   * @brief 实例化渲染
   * @param projection 投影矩阵
   * @param view 视图矩阵
   * @param instances 实例变换数组
   */
        void drawInstanced(const QMatrix4x4 &projection, const QMatrix4x4 &view,
                           const std::vector<Transform> &instances);

        // LOD 控制
        void setLODLevels(const std::vector<int> &segmentCounts);

        void updateLOD(const QVector3D &cameraPos);

        void setLODThreshold(float threshold) { m_lodThreshold = threshold; }

        // 获取变换信息
        QVector3D getPosition() const { return m_transform.position; }
        QQuaternion getRotation() const { return m_transform.rotation; }
        QVector3D getScale() const { return m_transform.scale; }
        const Transform &getTransform() const { return m_transform; }

        // 增量变换方法
        void translate(const QVector3D &delta) { m_transform.position += delta; }

        void rotate(const QQuaternion &delta) {
            m_transform.rotation = delta * m_transform.rotation;
        }

        void scale(const QVector3D &delta) { m_transform.scale *= delta; }

        /**
   * @brief 设置朝向
   * @param target 目标点
   * @param up 上方向向量
   */
        void lookAt(const QVector3D &target,
                    const QVector3D &up = QVector3D(0.0f, 1.0f, 0.0f)) {
            QVector3D direction = (target - m_transform.position).normalized();
            m_transform.rotation = QQuaternion::fromDirection(direction, up);
        }

        // 获取朝向
        QVector3D getForward() const {
            return m_transform.rotation.rotatedVector(QVector3D(0.0f, 0.0f, -1.0f));
        }

        QVector3D getUp() const {
            return m_transform.rotation.rotatedVector(QVector3D(0.0f, 1.0f, 0.0f));
        }

        QVector3D getRight() const {
            return m_transform.rotation.rotatedVector(QVector3D(1.0f, 0.0f, 0.0f));
        }

    protected:
        Transform m_transform; ///< 变换信息
        Material m_material; ///< 材质信息
        QOpenGLBuffer m_vbo; ///< 顶点缓冲对象
        QOpenGLVertexArrayObject m_vao; ///< 顶点数组对象
        int m_vertexCount; ///< 顶点数量
        bool m_visible; ///< 可见性

        static std::unique_ptr<QOpenGLShaderProgram> s_shaderProgram; ///< 共享着色器程序
        static int s_shaderUsers; ///< 着色器程序使用计数

        // 着色器相关方法
        void initializeShader();

        void releaseShader();

        void setupBuffer(const std::vector<float> &vertices, int stride);

        // LOD 相关
        std::vector<int> m_lodSegments; ///< LOD级别的细分数
        int m_currentLOD = 0; ///< 当前LOD级别
        float m_lodThreshold = 10.0f; ///< LOD切换阈值

        // 实例化渲染缓冲
        QOpenGLBuffer m_instanceVBO;
        bool m_instancedMode = false;

        virtual void initializeInstanceBuffer();

        virtual void updateInstanceData(const std::vector<Transform> &instances);

        virtual int getOptimalSegmentCount(float distance) const;

        /**
   * @brief 生成顶点数据
   * @param segments 细分段数
   * @return 顶点数据数组
   */
        virtual std::vector<float> generateVertices([[maybe_unused]] int segments) { return {}; }

        /**
   * @brief 顶点数据结构
   */
        struct Vertex {
            QVector3D position; ///< 位置
            QVector3D normal; ///< 法线
            QVector2D texCoord; ///< 纹理坐标
        };

        /**
   * @brief 将顶点结构转换为浮点数组
   */
        static std::vector<float> vertexToFloat(const std::vector<Vertex> &vertices) {
            std::vector<float> result;
            result.reserve(vertices.size() * 8); // 3 pos + 3 normal + 2 tex
            for (const auto &v: vertices) {
                // 位置
                result.push_back(v.position.x());
                result.push_back(v.position.y());
                result.push_back(v.position.z());
                // 法线
                result.push_back(v.normal.x());
                result.push_back(v.normal.y());
                result.push_back(v.normal.z());
                // 纹理坐标
                result.push_back(v.texCoord.x());
                result.push_back(v.texCoord.y());
            }
            return result;
        }

        /**
  * @brief 生成圆形顶点数据
  * @param radius 半径
  * @param segments 细分段数
  * @param y Y坐标
  * @param up 是否朝上
  */
        static std::vector<Vertex> generateCircle(float radius, int segments, float y,
                                                  bool up) {
            std::vector<Vertex> vertices;
            QVector3D normal(0.0f, up ? 1.0f : -1.0f, 0.0f);

            for (int i = 0; i < segments; ++i) {
                float angle1 = 2.0f * M_PI * i / segments;
                float angle2 = 2.0f * M_PI * (i + 1) / segments;

                float x1 = cos(angle1) * radius;
                float z1 = sin(angle1) * radius;
                float x2 = cos(angle2) * radius;
                float z2 = sin(angle2) * radius;

                // 中心点
                vertices.push_back(
                    {QVector3D(0.0f, y, 0.0f), normal, QVector2D(0.5f, 0.5f)});
                // 边缘点1
                vertices.push_back({
                    QVector3D(x1, y, z1), normal,
                    QVector2D((x1 / radius + 1.0f) / 2.0f,
                              (z1 / radius + 1.0f) / 2.0f)
                });
                // 边缘点2
                vertices.push_back({
                    QVector3D(x2, y, z2), normal,
                    QVector2D((x2 / radius + 1.0f) / 2.0f,
                              (z2 / radius + 1.0f) / 2.0f)
                });
            }
            return vertices;
        }
    };

    /**
     *  @brief 立方体
     */
    class Cube : public Shape3D {
    public:
        explicit Cube(float size = 1.0f);

        void initialize() override;

        std::vector<float> generateVertices(int) override {
            std::vector<Vertex> vertices;
            float halfSize = m_size * 0.5f;

            // 定义立方体的8个顶点
            QVector3D p1(-halfSize, -halfSize, -halfSize);
            QVector3D p2(halfSize, -halfSize, -halfSize);
            QVector3D p3(halfSize, halfSize, -halfSize);
            QVector3D p4(-halfSize, halfSize, -halfSize);
            QVector3D p5(-halfSize, -halfSize, halfSize);
            QVector3D p6(halfSize, -halfSize, halfSize);
            QVector3D p7(halfSize, halfSize, halfSize);
            QVector3D p8(-halfSize, halfSize, halfSize);

            // 前面
            vertices.push_back(
                {p1, QVector3D(0.0f, 0.0f, -1.0f), QVector2D(0.0f, 0.0f)});
            vertices.push_back(
                {p2, QVector3D(0.0f, 0.0f, -1.0f), QVector2D(1.0f, 0.0f)});
            vertices.push_back(
                {p3, QVector3D(0.0f, 0.0f, -1.0f), QVector2D(1.0f, 1.0f)});
            vertices.push_back(
                {p1, QVector3D(0.0f, 0.0f, -1.0f), QVector2D(0.0f, 0.0f)});
            vertices.push_back(
                {p3, QVector3D(0.0f, 0.0f, -1.0f), QVector2D(1.0f, 1.0f)});
            vertices.push_back(
                {p4, QVector3D(0.0f, 0.0f, -1.0f), QVector2D(0.0f, 1.0f)});

            // 后面
            vertices.push_back(
                {p5, QVector3D(0.0f, 0.0f, 1.0f), QVector2D(1.0f, 0.0f)});
            vertices.push_back(
                {p8, QVector3D(0.0f, 0.0f, 1.0f), QVector2D(1.0f, 1.0f)});
            vertices.push_back(
                {p7, QVector3D(0.0f, 0.0f, 1.0f), QVector2D(0.0f, 1.0f)});
            vertices.push_back(
                {p5, QVector3D(0.0f, 0.0f, 1.0f), QVector2D(1.0f, 0.0f)});
            vertices.push_back(
                {p7, QVector3D(0.0f, 0.0f, 1.0f), QVector2D(0.0f, 1.0f)});
            vertices.push_back(
                {p6, QVector3D(0.0f, 0.0f, 1.0f), QVector2D(0.0f, 0.0f)});

            // 上面
            vertices.push_back(
                {p4, QVector3D(0.0f, 1.0f, 0.0f), QVector2D(0.0f, 1.0f)});
            vertices.push_back(
                {p3, QVector3D(0.0f, 1.0f, 0.0f), QVector2D(0.0f, 0.0f)});
            vertices.push_back(
                {p7, QVector3D(0.0f, 1.0f, 0.0f), QVector2D(1.0f, 0.0f)});
            vertices.push_back(
                {p4, QVector3D(0.0f, 1.0f, 0.0f), QVector2D(0.0f, 1.0f)});
            vertices.push_back(
                {p7, QVector3D(0.0f, 1.0f, 0.0f), QVector2D(1.0f, 0.0f)});
            vertices.push_back(
                {p8, QVector3D(0.0f, 1.0f, 0.0f), QVector2D(1.0f, 1.0f)});

            // 下面
            vertices.push_back(
                {p1, QVector3D(0.0f, -1.0f, 0.0f), QVector2D(0.0f, 0.0f)});
            vertices.push_back(
                {p5, QVector3D(0.0f, -1.0f, 0.0f), QVector2D(1.0f, 0.0f)});
            vertices.push_back(
                {p6, QVector3D(0.0f, -1.0f, 0.0f), QVector2D(1.0f, 1.0f)});
            vertices.push_back(
                {p1, QVector3D(0.0f, -1.0f, 0.0f), QVector2D(0.0f, 0.0f)});
            vertices.push_back(
                {p6, QVector3D(0.0f, -1.0f, 0.0f), QVector2D(1.0f, 1.0f)});
            vertices.push_back(
                {p2, QVector3D(0.0f, -1.0f, 0.0f), QVector2D(0.0f, 1.0f)});

            // 右面
            vertices.push_back(
                {p2, QVector3D(1.0f, 0.0f, 0.0f), QVector2D(0.0f, 0.0f)});
            vertices.push_back(
                {p6, QVector3D(1.0f, 0.0f, 0.0f), QVector2D(1.0f, 0.0f)});
            vertices.push_back(
                {p7, QVector3D(1.0f, 0.0f, 0.0f), QVector2D(1.0f, 1.0f)});
            vertices.push_back(
                {p2, QVector3D(1.0f, 0.0f, 0.0f), QVector2D(0.0f, 0.0f)});
            vertices.push_back(
                {p7, QVector3D(1.0f, 0.0f, 0.0f), QVector2D(1.0f, 1.0f)});
            vertices.push_back(
                {p3, QVector3D(1.0f, 0.0f, 0.0f), QVector2D(0.0f, 1.0f)});

            // 左面
            vertices.push_back(
                {p1, QVector3D(-1.0f, 0.0f, 0.0f), QVector2D(1.0f, 0.0f)});
            vertices.push_back(
                {p4, QVector3D(-1.0f, 0.0f, 0.0f), QVector2D(1.0f, 1.0f)});
            vertices.push_back(
                {p8, QVector3D(-1.0f, 0.0f, 0.0f), QVector2D(0.0f, 1.0f)});
            vertices.push_back(
                {p1, QVector3D(-1.0f, 0.0f, 0.0f), QVector2D(1.0f, 0.0f)});
            vertices.push_back(
                {p8, QVector3D(-1.0f, 0.0f, 0.0f), QVector2D(0.0f, 1.0f)});
            vertices.push_back(
                {p5, QVector3D(-1.0f, 0.0f, 0.0f), QVector2D(0.0f, 0.0f)});

            return vertexToFloat(vertices);
        }

    private:
        float m_size;
    };

    /**
     * @brief 椭圆
     */
    class Cylinder : public Shape3D {
    public:
        Cylinder(float radius = 0.5f, float height = 1.0f, int segments = 32);

        void initialize() override;

        std::vector<float> generateVertices(int segments) override {
            std::vector<Vertex> vertices;
            float halfHeight = m_height * 0.5f;

            // 侧面
            for (int i = 0; i < segments; ++i) {
                float angle1 = 2.0f * M_PI * i / segments;
                float angle2 = 2.0f * M_PI * (i + 1) / segments;

                float x1 = cos(angle1) * m_radius;
                float z1 = sin(angle1) * m_radius;
                float x2 = cos(angle2) * m_radius;
                float z2 = sin(angle2) * m_radius;

                QVector3D normal1(x1, 0.0f, z1);
                QVector3D normal2(x2, 0.0f, z2);
                normal1.normalize();
                normal2.normalize();

                // 第一个三角形
                vertices.push_back({
                    QVector3D(x1, -halfHeight, z1), normal1,
                    QVector2D(float(i) / segments, 0.0f)
                });
                vertices.push_back({
                    QVector3D(x1, halfHeight, z1), normal1,
                    QVector2D(float(i) / segments, 1.0f)
                });
                vertices.push_back({
                    QVector3D(x2, -halfHeight, z2), normal2,
                    QVector2D(float(i + 1) / segments, 0.0f)
                });

                // 第二个三角形
                vertices.push_back({
                    QVector3D(x1, halfHeight, z1), normal1,
                    QVector2D(float(i) / segments, 1.0f)
                });
                vertices.push_back({
                    QVector3D(x2, halfHeight, z2), normal2,
                    QVector2D(float(i + 1) / segments, 1.0f)
                });
                vertices.push_back({
                    QVector3D(x2, -halfHeight, z2), normal2,
                    QVector2D(float(i + 1) / segments, 0.0f)
                });
            }

            // 顶面和底面
            auto topVertices = generateCircle(m_radius, segments, halfHeight, true);
            auto bottomVertices =
                    generateCircle(m_radius, segments, -halfHeight, false);
            vertices.insert(vertices.end(), topVertices.begin(), topVertices.end());
            vertices.insert(vertices.end(), bottomVertices.begin(),
                            bottomVertices.end());

            return vertexToFloat(vertices);
        }

    private:
        float m_radius;
        float m_height;
        int m_segments;
    };

    /**
     * @brief 球体
     */
    class Sphere : public Shape3D {
    public:
        Sphere(float radius = 0.5f, int rings = 16, int sectors = 32);

        void initialize() override;

        std::vector<float> generateVertices(int segments) override {
            std::vector<Vertex> vertices;
            segments = std::max(segments, 8); // 确保最小分段数

            int rings = segments / 2;
            int sectors = segments;

            for (int r = 0; r <= rings; ++r) {
                float phi = M_PI * float(r) / rings;
                float cosPhi = cos(phi);
                float sinPhi = sin(phi);

                for (int s = 0; s <= sectors; ++s) {
                    float theta = 2.0f * M_PI * float(s) / sectors;
                    float cosTheta = cos(theta);
                    float sinTheta = sin(theta);

                    float x = cosTheta * sinPhi;
                    float y = cosPhi;
                    float z = sinTheta * sinPhi;

                    QVector3D pos(x * m_radius, y * m_radius, z * m_radius);
                    QVector3D normal(x, y, z);
                    QVector2D texCoord(float(s) / sectors, float(r) / rings);

                    vertices.push_back({pos, normal, texCoord});
                }
            }

            std::vector<Vertex> triangles;
            for (int r = 0; r < rings; ++r) {
                for (int s = 0; s < sectors; ++s) {
                    int first = r * (sectors + 1) + s;
                    int second = first + sectors + 1;

                    triangles.push_back(vertices[first]);
                    triangles.push_back(vertices[second]);
                    triangles.push_back(vertices[first + 1]);

                    triangles.push_back(vertices[second]);
                    triangles.push_back(vertices[second + 1]);
                    triangles.push_back(vertices[first + 1]);
                }
            }

            return vertexToFloat(triangles);
        }

    private:
        float m_radius;
        int m_rings;
        int m_sectors;
    };

    /**
     * @brief  箭头（由圆柱体和圆锥体组合而成）
     */
    class Arrow : public Shape3D {
    public:
        Arrow(float length = 1.0f, float shaftRadius = 0.05f, float headLength = 0.2f,
              float headRadius = 0.1f, int segments = 32);

        void initialize() override;

        std::vector<float> generateVertices(int segments) override {
            std::vector<Vertex> vertices;
            float shaftLength = m_length - m_headLength;

            // 箭身（圆柱体）
            Cylinder shaft(m_shaftRadius, shaftLength, segments);
            auto shaftVertices = shaft.generateVertices(segments);

            // 箭头（圆锥体）
            std::vector<Vertex> headVertices;
            for (int i = 0; i < segments; ++i) {
                float angle1 = 2.0f * M_PI * i / segments;
                float angle2 = 2.0f * M_PI * (i + 1) / segments;

                float x1 = cos(angle1);
                float z1 = sin(angle1);
                float x2 = cos(angle2);
                float z2 = sin(angle2);

                QVector3D tip(0.0f, m_length, 0.0f);
                QVector3D base1(x1 * m_headRadius, shaftLength, z1 * m_headRadius);
                QVector3D base2(x2 * m_headRadius, shaftLength, z2 * m_headRadius);

                QVector3D normal =
                        QVector3D::crossProduct(base2 - base1, tip - base1).normalized();

                headVertices.push_back(
                    {base1, normal, QVector2D(float(i) / segments, 0.0f)});
                headVertices.push_back(
                    {tip, normal, QVector2D(float(i + 0.5f) / segments, 1.0f)});
                headVertices.push_back(
                    {base2, normal, QVector2D(float(i + 1) / segments, 0.0f)});
            }

            // 箭头底面
            auto baseVertices =
                    generateCircle(m_headRadius, segments, shaftLength, false);

            // 合并所有顶点
            vertices.insert(vertices.end(), headVertices.begin(), headVertices.end());
            vertices.insert(vertices.end(), baseVertices.begin(), baseVertices.end());

            return vertexToFloat(vertices);
        }

    private:
        float m_length;
        float m_shaftRadius;
        float m_headLength;
        float m_headRadius;
        int m_segments;
    };
} // namespace ProGraphics
