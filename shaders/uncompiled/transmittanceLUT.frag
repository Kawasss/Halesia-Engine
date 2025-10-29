#version 460
#include "include/atmosCommon.glsl"

layout (location = 0) out vec4 fragColor;
layout (location = 0) in  vec2 uvCoord;

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

const float sunTransmittanceSteps = 40.0;

vec3 getSunTransmittance(vec3 pos, vec3 sunDir) {
    if (rayIntersectSphere(pos, sunDir, groundRadiusMM) > 0.0) {
        return vec3(0.0);
    }
    
    float atmoDist = rayIntersectSphere(pos, sunDir, atmosphereRadiusMM);
    float t = 0.0;
    
    vec3 transmittance = vec3(1.0);
    for (float i = 0.0; i < sunTransmittanceSteps; i += 1.0) {
        float newT = ((i + 0.3)/sunTransmittanceSteps)*atmoDist;
        float dt = newT - t;
        t = newT;
        
        vec3 newPos = pos + t*sunDir;
        
        vec3 rayleighScattering, extinction;
        float mieScattering;
        getScatteringValues(newPos, rayleighScattering, mieScattering, extinction);
        
        transmittance *= exp(-dt*extinction);
    }
    return transmittance;
}

void main()
{
    vec2 fragCoord = uvCoord * sceneData.viewportSize;

    if (fragCoord.x >= (tLUTRes.x+1.5) || fragCoord.y >= (tLUTRes.y+1.5)) {
        fragColor = vec4(0, 0, 0, 1);
        return;
    }

    float u = clamp(fragCoord.x, 0.0, tLUTRes.x-1.0)/tLUTRes.x;
    float v = clamp(fragCoord.y, 0.0, tLUTRes.y-1.0)/tLUTRes.y;
    
    float sunCosTheta = 2.0*u - 1.0;
    float sunTheta = safeacos(sunCosTheta);
    float height = mix(groundRadiusMM, atmosphereRadiusMM, v);
    
    vec3 pos = vec3(0.0, height, 0.0); 
    vec3 sunDir = normalize(vec3(0.0, sunCosTheta, -sin(sunTheta)));
    
    fragColor = vec4(getSunTransmittance(pos, sunDir), 1.0);
}