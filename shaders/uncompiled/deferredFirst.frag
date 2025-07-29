#version 460

DECLARE_EXTERNAL_SET(1)

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoords;
layout (location = 3) in vec3 camPos;
layout (location = 4) in vec4 prevPosition;
layout (location = 5) in vec4 currPosition;
layout (location = 6) in vec3 tangent;
layout (location = 7) in vec3 bitangent;

layout (location = 0) out vec4 positionColor;
layout (location = 1) out vec4 albedoColor;
layout (location = 2) out vec4 normalColor;
layout (location = 3) out vec4 metallicRoughnessAOColor;
layout (location = 4) out vec4 velocityColor;
layout (location = 5) out vec4 geometricNormalColor;

layout(push_constant) uniform constant
{
    mat4 model;
    int materialID;
} Constant;

layout(set = 0, binding = 0) uniform sceneInfo {
    vec3 camPos;
    mat4 view;
    mat4 proj;
    mat4 prevView;
    mat4 prevProj;
} ubo;

layout(set = 1, binding = material_buffer_binding) uniform sampler2D[bindless_texture_size] textures;

vec3 GetNormalFromMap()
{
    vec3 tangentNormal = texture(textures[Constant.materialID * 5 + 1], texCoords).rgb * 2.0 - 1.0;

    vec3 T = normalize(tangent);
    vec3 B = normalize(bitangent);
    vec3 N = normalize(normal);
    
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main()
{
	albedoColor = texture(textures[Constant.materialID * 5 + 0], texCoords);

    if (albedoColor.a == 0.0)
        discard;

    vec3 viewDir = normalize(position - ubo.camPos);

    geometricNormalColor = dot(normal, viewDir) > 0.0 ?  geometricNormalColor = vec4(-normal, 1.0) : vec4(normal, 1.0);

    vec3 N = GetNormalFromMap();
    if (dot(N, viewDir) > 0.0)
        N = -N;

    positionColor = vec4(position, 1.0);
    normalColor   = vec4(N, 1.0);

    vec2 prevClip = prevPosition.xy / prevPosition.w;
    vec2 currClip = currPosition.xy / currPosition.w;

    velocityColor = vec4(currClip - prevClip, 0.0, 1.0);

    metallicRoughnessAOColor.r = texture(textures[Constant.materialID * 5 + 2], texCoords).r;
    metallicRoughnessAOColor.g = texture(textures[Constant.materialID * 5 + 3], texCoords).g;
    metallicRoughnessAOColor.b = texture(textures[Constant.materialID * 5 + 4], texCoords).b;
    metallicRoughnessAOColor.a = 1.0;
}