#version 460
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoords;
layout (location = 3) in vec3 camPos;

layout (location = 0) out vec4 positionColor;
layout (location = 1) out vec4 albedoColor;
layout (location = 2) out vec4 normalColor;
layout (location = 3) out vec4 metallicRoughnessAOColor;

layout(push_constant) uniform constant
{
    mat4 model;
    int materialID;
} Constant;

layout(set = 0, binding = 2) uniform sampler2D[500] textures;

vec3 GetNormalFromMap()
{
    vec3 tangentNormal = texture(textures[Constant.materialID * 5 + 1], texCoords).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(position);
    vec3 Q2  = dFdy(position);
    vec2 st1 = dFdx(texCoords);
    vec2 st2 = dFdy(texCoords);

    vec3 N   = normalize(normal);
    vec3 T   = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B   = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main()
{
    positionColor = vec4(position, 1.0);
    normalColor   = vec4(GetNormalFromMap(), 1.0);
 
	albedoColor = texture(textures[Constant.materialID * 5 + 0], texCoords);

    metallicRoughnessAOColor.r = texture(textures[Constant.materialID * 5 + 2], texCoords).r;
    metallicRoughnessAOColor.g = texture(textures[Constant.materialID * 5 + 3], texCoords).g;
    metallicRoughnessAOColor.b = texture(textures[Constant.materialID * 5 + 4], texCoords).b;
    metallicRoughnessAOColor.a = 1.0;
}