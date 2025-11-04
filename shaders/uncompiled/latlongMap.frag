#version 460
#include "include/atmosCommon.glsl"
#include "include/light.glsl"

layout (location = 0) in vec2 uvCoord;
layout (location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D transmittanceLUT;
layout(set = 0, binding = 1) uniform sampler2D mscatteringLUT;

DECLARE_EXTERNAL_SET(2)

layout(set = 2, binding = scene_data_buffer_binding) uniform SceneData
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

layout(set = 2, binding = light_buffer_binding) readonly buffer lightBuffer
{
	int count;
	Light data[];
} lights;

vec3 GetSunDir()
{
    for (int i = 0; i < lights.count; i++)
    {
        if (lights.data[i].type.x == LIGHT_TYPE_DIRECTIONAL)
            return GetLightDir(lights.data[i], vec3(0.0));
    }
    return vec3(0.0, 1.0, 0.0);
}

const int numScatteringSteps = 32;
vec3 raymarchScattering(vec3 pos, 
                              vec3 rayDir, 
                              vec3 sunDir,
                              float tMax,
                              float numSteps) {
    float cosTheta = dot(rayDir, sunDir);
    
	float miePhaseValue = getMiePhase(cosTheta);
	float rayleighPhaseValue = getRayleighPhase(-cosTheta);
    
    vec3 lum = vec3(0.0);
    vec3 transmittance = vec3(1.0);
    float t = 0.0;
    for (float i = 0.0; i < numSteps; i += 1.0) {
        float newT = ((i + 0.3)/numSteps)*tMax;
        float dt = newT - t;
        t = newT;
        
        vec3 newPos = pos + t*rayDir;
        
        vec3 rayleighScattering, extinction;
        float mieScattering;
        getScatteringValues(newPos, rayleighScattering, mieScattering, extinction);
        
        vec3 sampleTransmittance = exp(-dt*extinction);

        vec3 sunTransmittance = getValFromTLUT(transmittanceLUT, sceneData.viewportSize, newPos, sunDir);
        vec3 psiMS = getValFromMultiScattLUT(mscatteringLUT, sceneData.viewportSize, newPos, sunDir);
        
        vec3 rayleighInScattering = rayleighScattering*(rayleighPhaseValue*sunTransmittance + psiMS);
        vec3 mieInScattering = mieScattering*(miePhaseValue*sunTransmittance + psiMS);
        vec3 inScattering = (rayleighInScattering + mieInScattering);

        // Integrated scattering within path segment.
        vec3 scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / extinction;

        lum += scatteringIntegral*transmittance;
        
        transmittance *= sampleTransmittance;
    }
    return lum;
}

void main()
{;
    vec2 fragCoord = uvCoord * sceneData.viewportSize;

    if (fragCoord.x >= (skyLUTRes.x+1.5) || fragCoord.y >= (skyLUTRes.y+1.5)) {
        fragColor = vec4(0, 0, 0, 1);
        return;
    }

    float u = clamp(fragCoord.x, 0.0, skyLUTRes.x-1.0)/skyLUTRes.x;
    float v = clamp(fragCoord.y, 0.0, skyLUTRes.y-1.0)/skyLUTRes.y;

    float azimuthAngle = (u - 0.5)*2.0*PI;
    // Non-linear mapping of altitude. See Section 5.3 of the paper.
    float adjV;
    if (v < 0.5) {
		float coord = 1.0 - 2.0*v;
		adjV = -coord*coord;
	} else {
		float coord = v*2.0 - 1.0;
		adjV = coord*coord;
	}
    
    vec3 viewPos = viewPosBase + sceneData.camPosition * 0.01;

    float height = length(viewPos);
    vec3 up = viewPos / height;
    float horizonAngle = safeacos(sqrt(height * height - groundRadiusMM * groundRadiusMM) / height) - 0.5*PI;
    float altitudeAngle = adjV*0.5*PI - horizonAngle;
    
    float cosAltitude = cos(altitudeAngle);
    vec3 rayDir = vec3(cosAltitude*sin(azimuthAngle), sin(altitudeAngle), -cosAltitude*cos(azimuthAngle));
    
    float sunAltitude = (0.5*PI) - acos(dot(GetSunDir(), up));
    vec3 sunDir = vec3(0.0, sin(sunAltitude), -cos(sunAltitude));
    
    float atmoDist = rayIntersectSphere(viewPos, rayDir, atmosphereRadiusMM);
    float groundDist = rayIntersectSphere(viewPos, rayDir, groundRadiusMM);
    float tMax = (groundDist < 0.0) ? atmoDist : groundDist;
    vec3 lum = raymarchScattering(viewPos, rayDir, sunDir, tMax, float(numScatteringSteps));
    fragColor = vec4(lum, 1.0);
}