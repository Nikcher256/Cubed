#version 460 core

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

layout(binding = 0) uniform sampler2D u_Texture;

const vec3 lightDir = normalize(vec3(0.5f, 1.0, 0.0));

void main()
{
    vec3 normal = normalize(in_normal);

    float intencity = max(dot(normal, lightDir), 0.35);

    vec4 textureSample = texture(u_Texture, in_uv);

    out_color = vec4(vec3(intencity) * textureSample.rgb, 1.0);
}
