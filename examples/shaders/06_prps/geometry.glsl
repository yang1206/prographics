#version 410 core
layout (lines) in;
layout (triangle_strip, max_vertices = 128) out;

in VS_OUT {
    vec3 color;
    vec4 worldPos;
} gs_in[];

out vec3 vertexColor;
out vec3 fragNormal;
out vec3 fragPos;

uniform mat4 projection;
uniform mat4 view;

const float PI = 3.1415926;
const int NUM_SEGMENTS = 8;  // 圆柱体的段数
const float ARROW_LENGTH = 0.3;  // 箭头长度
const float ARROW_WIDTH = 0.15;  // 箭头宽度

// 辅助函数：发射一个三角形
void emitTriangle(vec4 v1, vec4 v2, vec4 v3, vec3 normal, vec3 color) {
    gl_Position = projection * v1;
    vertexColor = color;
    fragNormal = normal;
    fragPos = v1.xyz;
    EmitVertex();

    gl_Position = projection * v2;
    vertexColor = color;
    fragNormal = normal;
    fragPos = v2.xyz;
    EmitVertex();

    gl_Position = projection * v3;
    vertexColor = color;
    fragNormal = normal;
    fragPos = v3.xyz;
    EmitVertex();

    EndPrimitive();
}

void main() {
    float radius = 0.05;  // 圆柱体半径

    // 获取线段在观察空间中的起点和终点
    vec4 start = view * gs_in[0].worldPos;
    vec4 end = view * gs_in[1].worldPos;

    // 计算方向向量
    vec3 dir = normalize(end.xyz - start.xyz);

    // 计算垂直于方向向量的基向量
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(dir, up));
    up = normalize(cross(right, dir));

    // 调整终点，为箭头留出空间
    vec4 shaftEnd = end - vec4(dir * ARROW_LENGTH, 0.0);

    // 绘制圆柱体主体
    for (int i = 0; i <= NUM_SEGMENTS; i++) {
        float angle = (i % NUM_SEGMENTS) * (2.0 * PI / NUM_SEGMENTS);
        float x = cos(angle);
        float y = sin(angle);

        vec3 offset = (right * x + up * y) * radius;

        vec4 v1 = start + vec4(offset, 0.0);
        gl_Position = projection * v1;
        vertexColor = gs_in[0].color;
        fragNormal = normalize(offset);
        fragPos = v1.xyz;
        EmitVertex();

        vec4 v2 = shaftEnd + vec4(offset, 0.0);
        gl_Position = projection * v2;
        vertexColor = gs_in[1].color;
        fragNormal = normalize(offset);
        fragPos = v2.xyz;
        EmitVertex();
    }
    EndPrimitive();

    // 绘制箭头（四个三角形组成的金字塔）
    float arrowRadius = ARROW_WIDTH;
    vec4 tip = end;
    vec4 arrowBase = shaftEnd;

    // 计算箭头底部的四个点
    vec4 b1 = arrowBase + vec4((right + up) * arrowRadius, 0.0);
    vec4 b2 = arrowBase + vec4((right - up) * arrowRadius, 0.0);
    vec4 b3 = arrowBase + vec4((-right - up) * arrowRadius, 0.0);
    vec4 b4 = arrowBase + vec4((-right + up) * arrowRadius, 0.0);

    // 计算每个面的法线
    vec3 n1 = normalize(cross(vec3(b2 - b1), vec3(tip - b1)));
    vec3 n2 = normalize(cross(vec3(b3 - b2), vec3(tip - b2)));
    vec3 n3 = normalize(cross(vec3(b4 - b3), vec3(tip - b3)));
    vec3 n4 = normalize(cross(vec3(b1 - b4), vec3(tip - b4)));

    // 发射四个三角形
    emitTriangle(b1, b2, tip, n1, gs_in[1].color);
    emitTriangle(b2, b3, tip, n2, gs_in[1].color);
    emitTriangle(b3, b4, tip, n3, gs_in[1].color);
    emitTriangle(b4, b1, tip, n4, gs_in[1].color);
}