// basic.frag
#version 460 core

layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) flat in int in_texIndex;
layout(location = 4) flat in int in_isOutline;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D u_Textures[100];

const vec3 lightDir = normalize(vec3(0.5, 1.0, 1.0));

void main()
{
    // For outlines: draw only back faces and make them solid black.
    if (in_isOutline != 0) {
        if (gl_FrontFacing) discard;      // emulate front-face culling (no pipeline change)
        out_color = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    int idx = clamp(in_texIndex, 0, 99);
    vec3 N = normalize(in_normal);

    float intensity = max(dot(N, lightDir), 0.35);
    vec4 tex = texture(u_Textures[idx], in_uv);

    out_color = vec4(tex.rgb * intensity, tex.a);
}
