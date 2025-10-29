#version 460
#include "include/atmosCommon.glsl"

layout (location = 0) out vec4 fragColor;
layout (location = 0) in vec2 uvCoord;

layout(set = 0, binding = 0) uniform sampler2D transmittanceLUT;

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


const float mulScattSteps = 20.0;
const int sqrtSamples = 8;

vec3 getSphericalDir(float theta, float phi) {
     float cosPhi = cos(phi);
     float sinPhi = sin(phi);
     float cosTheta = cos(theta);
     float sinTheta = sin(theta);
     return vec3(sinPhi*sinTheta, cosPhi, sinPhi*cosTheta);
}

// Calculates Equation (5) and (7) from the paper.
void getMulScattValues(vec3 pos, vec3 sunDir, out vec3 lumTotal, out vec3 fms) {
    lumTotal = vec3(0.0);
    fms = vec3(0.0);
    
    float invSamples = 1.0/float(sqrtSamples*sqrtSamples);
    for (int i = 0; i < sqrtSamples; i++) {
        for (int j = 0; j < sqrtSamples; j++) {
            // This integral is symmetric about theta = 0 (or theta = PI), so we
            // only need to integrate from zero to PI, not zero to 2*PI.
            float theta = PI * (float(i) + 0.5) / float(sqrtSamples);
            float phi = safeacos(1.0 - 2.0*(float(j) + 0.5) / float(sqrtSamples));
            vec3 rayDir = getSphericalDir(theta, phi);
            
            float atmoDist = rayIntersectSphere(pos, rayDir, atmosphereRadiusMM);
            float groundDist = rayIntersectSphere(pos, rayDir, groundRadiusMM);
            float tMax = atmoDist;
            if (groundDist > 0.0) {
                tMax = groundDist;
            }
            
            float cosTheta = dot(rayDir, sunDir);
    
            float miePhaseValue = getMiePhase(cosTheta);
            float rayleighPhaseValue = getRayleighPhase(-cosTheta);
            
            vec3 lum = vec3(0.0), lumFactor = vec3(0.0), transmittance = vec3(1.0);
            float t = 0.0;
            for (float stepI = 0.0; stepI < mulScattSteps; stepI += 1.0) {
                float newT = ((stepI + 0.3)/mulScattSteps)*tMax;
                float dt = newT - t;
                t = newT;

                vec3 newPos = pos + t*rayDir;

                vec3 rayleighScattering, extinction;
                float mieScattering;
                getScatteringValues(newPos, rayleighScattering, mieScattering, extinction);

                vec3 sampleTransmittance = exp(-dt*extinction);
                
                // Integrate within each segment.
                vec3 scatteringNoPhase = rayleighScattering + mieScattering;
                vec3 scatteringF = (scatteringNoPhase - scatteringNoPhase * sampleTransmittance) / extinction;
                lumFactor += transmittance*scatteringF;
                
                // This is slightly different from the paper, but I think the paper has a mistake?
                // In equation (6), I think S(x,w_s) should be S(x-tv,w_s).
                vec3 sunTransmittance = getValFromTLUT(transmittanceLUT, sceneData.viewportSize, newPos, sunDir);

                vec3 rayleighInScattering = rayleighScattering*rayleighPhaseValue;
                float mieInScattering = mieScattering*miePhaseValue;
                vec3 inScattering = (rayleighInScattering + mieInScattering)*sunTransmittance;

                // Integrated scattering within path segment.
                vec3 scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / extinction;

                lum += scatteringIntegral*transmittance;
                transmittance *= sampleTransmittance;
            }
            
            if (groundDist > 0.0) {
                vec3 hitPos = pos + groundDist*rayDir;
                if (dot(pos, sunDir) > 0.0) {
                    hitPos = normalize(hitPos)*groundRadiusMM;
                    lum += transmittance*groundAlbedo*getValFromTLUT(transmittanceLUT, sceneData.viewportSize, hitPos, sunDir);
                }
            }
            
            fms += lumFactor*invSamples;
            lumTotal += lum*invSamples;
        }
    }
}

void main()
{
    vec2 fragCoord = uvCoord * sceneData.viewportSize;

    if (fragCoord.x >= (msLUTRes.x+1.5) || fragCoord.y >= (msLUTRes.y+1.5)) {
        fragColor = vec4(0, 0, 0, 1);
        return;
    }
    float u = clamp(fragCoord.x, 0.0, msLUTRes.x-1.0)/msLUTRes.x;
    float v = clamp(fragCoord.y, 0.0, msLUTRes.y-1.0)/msLUTRes.y;

    float sunCosTheta = 2.0*u - 1.0;
    float sunTheta = safeacos(sunCosTheta);
    float height = mix(groundRadiusMM, atmosphereRadiusMM, v);
    
    vec3 pos = vec3(0.0, height, 0.0); 
    vec3 sunDir = normalize(vec3(0.0, sunCosTheta, -sin(sunTheta)));
    
    vec3 lum, f_ms;
    getMulScattValues(pos, sunDir, lum, f_ms);
    
    // Equation 10 from the paper.
    vec3 psi = lum  / (1.0 - f_ms); 
    fragColor = vec4(psi, 1.0);
}