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