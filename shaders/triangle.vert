#version 460
#define MAX_MESHES 1000

layout(set = 0, binding = 0) uniform sceneInfo {
    vec3 camPos;
    mat4 view;
    mat4 proj;
} ubo;

layout (binding = 1) uniform ModelMatrices
{
    mat4 models[MAX_MESHES];
} model;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
flat layout (location = 2) out int drawID;
layout (location = 3) out vec3 worldPos;
layout (location = 4) out vec3 camPos;

void main() {
    camPos = ubo.camPos;
    worldPos = (model.models[gl_DrawID] * vec4(inPosition, 1)).xyz;
    gl_Position = ubo.proj * ubo.view * model.models[gl_DrawID] * vec4(inPosition, 1.0);

    fragNormal = normalize(mat3(transpose(inverse(model.models[gl_DrawID]))) * inNormal);
    fragTexCoord = inTexCoord;
    drawID = gl_DrawID;
}