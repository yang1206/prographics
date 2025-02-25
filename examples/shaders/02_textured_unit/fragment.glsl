#version 410 core
out vec4 FragColor;

in vec3 ourColor;
in vec2 TexCoord;

uniform float mixValue;

// texture samplers
uniform sampler2D texture1;
uniform sampler2D texture2;

void main()
{
    vec4 tex1 = texture(texture1, TexCoord);
    vec4 tex2 = texture(texture2, TexCoord);
    FragColor = mix(tex1, tex2, mixValue);  // 0.2是混合因子，可以调整
}