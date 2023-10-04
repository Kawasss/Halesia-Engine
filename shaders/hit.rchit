#version 460
#extension GL_GOOGLE_include_directive : enable

#include "rayTracingCommon.glsl"

layout(set = 0, binding = 0) uniform sceneInfo {
    vec3 camPos;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) rayPayloadEXT hitPayload prd;

void main()
{
    const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 inUV = pixelCenter / vec2(gl_LaunchIDEXT.xy);
    vec2 d = inUV * 2 - 1;

    vec4 origin = inverse(sceneInfo.view) * vec4(0, 0, 0, 1);
    vec4 target = inverse(sceneInfo.proj) * vec4(d.x, d.y, 1, 1);
    vec4 direction = inverse(sceneInfo.view) * vec4(normalize(target.xyz), 0);

    uint rayFlags = gl_RayFlagsOpaqueEXT;
    float tMin = 0.001;
    float tMax = 10000.0;

    traceRayEXT(topLevelAS, rayFlags, 0xFF, 0, 0, 0, origin.xyz, tMin, direction.xyz, tMax, 0);
    imageStore(
}