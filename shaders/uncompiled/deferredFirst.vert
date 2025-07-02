#version 460
layout (location = 0) in  vec3 inPosition;
layout (location = 1) in  vec3 inNormal;
layout (location = 2) in  vec2 inTexCoords;
layout (location = 3) in  vec3 inTangent;
layout (location = 4) in  vec3 inBiTangent;
layout (location = 5) in ivec4 inBoneIDs;
layout (location = 6) in  vec4 inBoneWeights;

layout (location = 0) out vec3 position;
layout (location = 1) out vec3 normal;
layout (location = 2) out vec2 texCoords;
layout (location = 3) out vec3 camPos;
layout (location = 4) out vec4 prevPosition;
layout (location = 5) out vec4 currPosition;
layout (location = 6) out mat3 tangentToWorld;

layout(set = 0, binding = 0) uniform sceneInfo {
    vec3 camPos;
    mat4 view;
    mat4 proj;
    mat4 prevView;
    mat4 prevProj;
} ubo;

layout(push_constant) uniform constant
{
    mat4 model;
    int materialID;
} Constant;

void main() 
{
    position = (Constant.model * vec4(inPosition, 1.0)).xyz;

    mat3 normalMatrix = transpose(inverse(mat3(Constant.model)));
    vec3 T = normalize(normalMatrix * inTangent);
    vec3 B = normalize(normalMatrix * inBiTangent);
    vec3 N = normalize(normalMatrix * inNormal);

    tangentToWorld = mat3(T, B, N);

    normal = N;

    texCoords = inTexCoords;
    camPos = ubo.camPos;

    prevPosition = ubo.prevProj * ubo.prevView * vec4(position, 1.0);
    currPosition = ubo.proj * ubo.view * vec4(position, 1.0);
    gl_Position = currPosition;
}