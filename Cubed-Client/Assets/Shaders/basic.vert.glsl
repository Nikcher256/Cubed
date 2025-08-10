#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_UV;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_uv;
layout(location = 3) flat out int out_texIndex;


layout(set = 1, binding = 0) uniform CameraUBO {
    mat4 ViewProjection;
} u_Camera;

layout(push_constant) uniform PushConstants {
    mat4 Transform;
    int TexIndex;
} u_PushConstants;

void main()
{
    vec4 worldPos = u_PushConstants.Transform * vec4(a_Position, 1.0);
    gl_Position = u_Camera.ViewProjection * worldPos;
    
    out_normal = normalize(transpose(inverse(mat3(u_PushConstants.Transform))) * a_Normal);

    out_color = a_Normal * 0.5 + 0.5;
    out_uv = a_UV;
    out_texIndex = u_PushConstants.TexIndex;
}
