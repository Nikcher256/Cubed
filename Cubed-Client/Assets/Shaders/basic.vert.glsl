// basic.vert
#version 460 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_UV;

layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_uv;
layout(location = 3) flat out int out_texIndex;
layout(location = 4) flat out int out_isOutline;

layout(set = 1, binding = 0) uniform CameraUBO {
    mat4 ViewProjection;
} u_Camera;

layout(push_constant) uniform PushConstants {
    mat4  Transform;         // model matrix
    int   TexIndex;
    int   IsOutline;         // 0 = normal, 1 = outline
    float OutlineThickness;  // in meters (e.g. 0.01)
    int   _pad;
} u_Push;

void main()
{
    // World-space position and normal
    vec4 worldPos   = u_Push.Transform * vec4(a_Position, 1.0);

    // Use the *upper-left 3x3* to transform the normal to world space,
    // then normalize so thickness isn't affected by non-uniform scale.
    vec3 worldNormal = normalize(mat3(u_Push.Transform) * a_Normal);

    // Add outline offset *in world space* so it isn't scaled again.
    if (u_Push.IsOutline != 0) {
        worldPos.xyz += worldNormal * u_Push.OutlineThickness; // meters
    }

    gl_Position = u_Camera.ViewProjection * worldPos;

    // For lighting (if any), use the inverse-transpose for correctness.
    mat3 nMat = transpose(inverse(mat3(u_Push.Transform)));
    out_normal    = normalize(nMat * a_Normal);
    out_uv        = a_UV;
    out_texIndex  = u_Push.TexIndex;
    out_isOutline = u_Push.IsOutline;
}
