#version 460
#include "include/atmosCommon.glsl"
#include "include/light.glsl"

layout(location = 0) in vec2 uvCoord;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D transmittanceLUT;
layout(set = 0, binding = 1) uniform sampler2D latlongMap;

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

vec3 getValFromSkyLUT(vec3 viewPos, vec3 rayDir, vec3 sunDir) {
    float height = length(viewPos);
    vec3 up = viewPos / height;
    
    float horizonAngle = safeacos(sqrt(height * height - groundRadiusMM * groundRadiusMM) / height);
    float altitudeAngle = horizonAngle - acos(dot(rayDir, up)); // Between -PI/2 and PI/2
    float azimuthAngle; // Between 0 and 2*PI
    if (abs(altitudeAngle) > (0.5*PI - .0001)) {
        // Looking nearly straight up or down.
        azimuthAngle = 0.0;
    } else {
        vec3 right = cross(sunDir, up);
        vec3 forward = cross(up, right);
        
        vec3 projectedDir = normalize(rayDir - up*(dot(rayDir, up)));
        float sinTheta = dot(projectedDir, right);
        float cosTheta = dot(projectedDir, forward);
        azimuthAngle = atan(sinTheta, cosTheta) + PI;
    }
    
    // Non-linear mapping of altitude angle. See Section 5.3 of the paper.
    float v = 0.5 + 0.5*sign(altitudeAngle)*sqrt(abs(altitudeAngle)*2.0/PI);
    vec2 uv = vec2(azimuthAngle / (2.0*PI), v);
    uv *= skyLUTRes;
    uv /= sceneData.viewportSize;
    
    return texture(latlongMap, uv).rgb;
}
;
vec3 jodieReinhardTonemap(vec3 c){
    // From: https://www.shadertoy.com/view/tdSXzD
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    vec3 tc = c / (c + 1.0);
    return mix(c / (l + 1.0), tc, tc);
}

vec3 sunWithBloom(vec3 rayDir, vec3 sunDir) {
    const float sunSolidAngle = 0.53*PI/180.0;
    const float minSunCosTheta = cos(sunSolidAngle);

    float cosTheta = dot(rayDir, sunDir);
    if (cosTheta >= minSunCosTheta) return vec3(1.0);
    
    float offset = minSunCosTheta - cosTheta;
    float gaussianBloom = exp(-offset*50000.0)*0.5;
    float invBloom = 1.0/(0.02 + offset*300.0)*0.01;
    return vec3(gaussianBloom+invBloom);
}

void main()
{
    vec2 UvCoord = uvCoord;
    UvCoord.y = 1.0 - UvCoord.y;

    vec3 viewPos = viewPosBase + sceneData.camPosition * 0.000001;

    const vec2 size = sceneData.viewportSize;

    vec3 sunDir = GetSunDir();

    float camWidthScale = 2.0*tan(sceneData.camFov/2.0);
    float camHeightScale = camWidthScale*size.y/size.x;
    
    vec3 camDir = sceneData.camDirection;
    vec3 camRight = sceneData.camRight;
    vec3 camUp = sceneData.camUp;
    
    vec2 xy = 2.0 * UvCoord - 1.0;
    vec3 rayDir = normalize(camDir + camRight*xy.x*camWidthScale + camUp*xy.y*camHeightScale);
    
    vec3 lum = getValFromSkyLUT(viewPos, rayDir, sunDir);

    // Bloom should be added at the end, but this is subtle and works well.
    vec3 sunLum = sunWithBloom(rayDir, sunDir);
    // Use smoothstep to limit the effect, so it drops off to actual zero.
    sunLum = smoothstep(0.002, 1.0, sunLum);
    if (length(sunLum) > 0.0) {
        if (rayIntersectSphere(viewPos, rayDir, groundRadiusMM) >= 0.0) {
            sunLum *= 0.0;
        } else {
            // If the sun value is applied to this pixel, we need to calculate the transmittance to obscure it.
            sunLum *= getValFromTLUT(transmittanceLUT, size, viewPos, sunDir);
        }
    }
    lum += sunLum;
    
    // Tonemapping and gamma. Super ad-hoc, probably a better way to do this.
    lum *= 20.0;
    lum = pow(lum, vec3(1.3));
    lum /= (smoothstep(0.0, 0.2, clamp(sunDir.y, 0.0, 1.0))*2.0 + 0.15);
    
    lum = jodieReinhardTonemap(lum);
    
    lum = pow(lum, vec3(1.0/2.2));
    
    fragColor = vec4(lum,1.0);
}