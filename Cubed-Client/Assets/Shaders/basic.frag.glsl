#version 460 core

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) flat in int in_texIndex;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D u_Textures[2];

const vec3 lightDir = normalize(vec3(0.5f, 1.0, 0.0));

void main()
{
    int idx = clamp(in_texIndex, 0, 1);
    vec3 normal = normalize(in_normal);

    float intencity = max(dot(normal, lightDir), 0.35);

    vec4 textureSample = texture(u_Textures[idx], in_uv);

    out_color = vec4(vec3(intencity) * textureSample.rgb, 1.0);
}
