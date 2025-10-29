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
layout (location = 3) out vec4 prevPosition;
layout (location = 4) out vec4 currPosition;
layout (location = 5) out vec3 tangent;
layout (location = 6) out vec3 bitangent;

DECLARE_EXTERNAL_SET(1)

layout(set = 1, binding = scene_data_buffer_binding) uniform SceneData
{
    mat4 view;
    mat4 proj;

    mat4 prevView;
    mat4 prevProj;
    
    mat4 viewInv;
    mat4 projInv;

    vec2 viewportSize;

    float zNear;
    float zFar;

    vec3 camPosition;
    vec3 camDirection;
    vec3 camRight;
    vec3 camUp;
    float padding;
    float camFov;
    uint frameCount;
    float time;
} sceneData;

layout(push_constant) uniform constant
{
    mat4 model;
    int materialID;
} Constant;

void main() 
{
    position = (Constant.model * vec4(inPosition, 1.0)).xyz;

    mat3 model3x3 = mat3(Constant.model);

    mat3 normalMatrix = transpose(inverse(model3x3));
    tangent   = normalize(normalMatrix * inTangent);
    bitangent = normalize(normalMatrix * inBiTangent);
    normal    = normalize(normalMatrix * inNormal);

    texCoords = inTexCoords;

    prevPosition = sceneData.prevProj * sceneData.prevView * vec4(position, 1.0);
    currPosition = sceneData.proj * sceneData.view * vec4(position, 1.0);
    gl_Position = currPosition;
}