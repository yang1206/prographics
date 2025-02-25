#version 410 core
out vec4 FragColor;
in vec2 TexCoord;
uniform float mixValue;

// texture samplers
uniform sampler2D texture1;
uniform sampler2D texture2;
uniform vec4 tintColor;
void main()
{
    vec4 texColor = mix(texture(texture1, TexCoord), texture(texture2, TexCoord), mixValue);
    FragColor = texColor * tintColor;
}