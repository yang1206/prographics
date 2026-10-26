#pragma once

#include "prographics/core/graphics/primitive2d.h"
#include <QDebug>
#include <array>
#include <cstddef>
#include <limits>

namespace ProGraphics {

// 仅供 PRPS 使用：固定环形槽保存竖线实例，动画帧只更新各槽的 Z 位置。
class PRPSRenderer final : protected QOpenGLExtraFunctions {
public:
    static constexpr int SlotCount = 80;

    int rowCapacity() const { return m_rowCapacity; }

    bool setRowCapacity(int capacity) {
        const size_t maximumCapacity = static_cast<size_t>(std::numeric_limits<int>::max()) /
            (SlotCount * sizeof(Instance));
        if (capacity <= 0 || static_cast<size_t>(capacity) > maximumCapacity) {
            qWarning() << "PRPS 实例缓冲区容量超出支持范围:" << capacity;
            return false;
        }
        if (!initialize()) return false;
        if (capacity == m_rowCapacity) return true;

        if (!m_vbo.bind()) return false;
        m_vbo.allocate(static_cast<int>(static_cast<size_t>(capacity) * SlotCount * sizeof(Instance)));
        m_vbo.release();
        m_rowCapacity = capacity;
        m_rowCounts.fill(0);
        return true;
    }

    bool uploadRow(int slot, const std::vector<Transform2D>& transforms) {
        if (slot < 0 || slot >= SlotCount || transforms.size() > static_cast<size_t>(m_rowCapacity)) {
            return false;
        }
        m_rowCounts[slot] = 0;
        if (transforms.empty()) return true;

        m_staging.resize(transforms.size());
        for (size_t i = 0; i < transforms.size(); ++i) {
            const auto& transform = transforms[i];
            auto& instance = m_staging[i];
            instance.phaseHeight[0] = transform.position.x();
            instance.phaseHeight[1] = transform.scale.y();
            instance.color[0] = transform.color.x();
            instance.color[1] = transform.color.y();
            instance.color[2] = transform.color.z();
            instance.color[3] = transform.color.w();
        }

        if (!m_vbo.bind()) return false;
        const size_t offset = static_cast<size_t>(slot) * m_rowCapacity * sizeof(Instance);
        m_vbo.write(static_cast<int>(offset), m_staging.data(),
                    static_cast<int>(m_staging.size() * sizeof(Instance)));
        m_vbo.release();
        m_rowCounts[slot] = static_cast<GLint>(transforms.size());
        return true;
    }

    void draw(const QMatrix4x4& projection, const QMatrix4x4& view,
              const std::array<float, SlotCount>& zPositions, int firstSlot, int slotCount) {
        if (slotCount <= 0 || slotCount > SlotCount || firstSlot < 0 || firstSlot >= SlotCount ||
            m_rowCapacity <= 0 || !m_program.bind()) {
            return;
        }
        m_program.setUniformValue(m_projectionLocation, projection);
        m_program.setUniformValue(m_viewLocation, view);
        m_program.setUniformValue(m_capacityLocation, m_rowCapacity);
        m_program.setUniformValueArray(m_zLocation, zPositions.data(), SlotCount, 1);
        m_program.setUniformValueArray(m_countLocation, m_rowCounts.data(), SlotCount);

        m_vao.bind();
        if (!m_vbo.bind()) {
            m_vao.release();
            m_program.release();
            return;
        }
        // 活动周期始终按时间占据连续环形槽，跨尾部时拆成两次绘制。
        const int firstCount = std::min(slotCount, SlotCount - firstSlot);
        drawSegment(firstSlot, firstCount);
        if (firstCount < slotCount) drawSegment(0, slotCount - firstCount);
        m_vao.release();
        m_vbo.release();
        m_program.release();
    }

private:
    struct Instance {
        float phaseHeight[2];
        float color[4];
    };
    static_assert(sizeof(float) == 4 && sizeof(Instance) == 24);

    bool initialize() {
        if (m_initialized) return true;
        if (m_failed) return false;
        initializeOpenGLFunctions();
        const char* vertexShader = R"(
            #version 410 core
            layout (location = 0) in vec2 phaseHeight;
            layout (location = 1) in vec4 instanceColor;
            uniform mat4 projection;
            uniform mat4 view;
            uniform int rowCapacity;
            uniform int firstSlot;
            uniform int rowCounts[80];
            uniform float zPositions[80];
            out vec4 vertexColor;
            void main() {
                int slot = firstSlot + gl_InstanceID / rowCapacity;
                int index = gl_InstanceID % rowCapacity;
                vertexColor = vec4(0.0);
                if (index >= rowCounts[slot] || zPositions[slot] <= 0.0) {
                    // 无效填充实例整体置于裁剪空间之外，不写入颜色或深度。
                    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
                    return;
                }
                vertexColor = instanceColor;
                if (zPositions[slot] < 2.0) vertexColor.a = zPositions[slot] / 2.0;
                vec4 position = vec4(phaseHeight.x, float(gl_VertexID) * phaseHeight.y,
                                     zPositions[slot], 1.0);
                gl_Position = projection * view * position;
            }
        )";
        const char* fragmentShader = R"(
            #version 410 core
            in vec4 vertexColor;
            out vec4 FragColor;
            void main() { FragColor = vertexColor; }
        )";
        if (!m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader) ||
            !m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader) ||
            !m_program.link() || !m_vao.create() || !m_vbo.create()) {
            qWarning() << "PRPS 批量渲染器初始化失败:" << m_program.log();
            m_failed = true;
            return false;
        }

        m_projectionLocation = m_program.uniformLocation("projection");
        m_viewLocation = m_program.uniformLocation("view");
        m_capacityLocation = m_program.uniformLocation("rowCapacity");
        m_firstSlotLocation = m_program.uniformLocation("firstSlot");
        m_zLocation = m_program.uniformLocation("zPositions[0]");
        m_countLocation = m_program.uniformLocation("rowCounts[0]");
        m_vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
        m_vao.bind();
        m_vbo.bind();
        glEnableVertexAttribArray(0);
        glVertexAttribDivisor(0, 1);
        glEnableVertexAttribArray(1);
        glVertexAttribDivisor(1, 1);
        m_vao.release();
        m_vbo.release();
        m_initialized = true;
        return true;
    }

    void drawSegment(int slot, int count) {
        const size_t offset = static_cast<size_t>(slot) * m_rowCapacity * sizeof(Instance);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Instance),
                              reinterpret_cast<void*>(offset + offsetof(Instance, phaseHeight)));
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Instance),
                              reinterpret_cast<void*>(offset + offsetof(Instance, color)));
        m_program.setUniformValue(m_firstSlotLocation, slot);
        glDrawArraysInstanced(GL_LINES, 0, 2, count * m_rowCapacity);
    }

    QOpenGLShaderProgram m_program;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;
    std::vector<Instance> m_staging;
    std::array<GLint, SlotCount> m_rowCounts{};
    int m_rowCapacity = 0;
    int m_projectionLocation = -1;
    int m_viewLocation = -1;
    int m_capacityLocation = -1;
    int m_firstSlotLocation = -1;
    int m_zLocation = -1;
    int m_countLocation = -1;
    bool m_initialized = false;
    bool m_failed = false;
};

} // namespace ProGraphics
