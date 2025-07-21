#version 460
#extension GL_EXT_ray_query : require

#include "include/light.glsl"

#define PI 3.14159265359

layout (location = 0) in vec2 uvCoord;

layout (location = 0) out vec4 fragColor;

layout (binding = 0) uniform sampler2D positionImage;
layout (binding = 1) uniform sampler2D albedoImage;
layout (binding = 2) uniform sampler2D normalImage;
layout (binding = 3) uniform sampler2D metallicRoughnessAOImage;

layout (binding = 4) uniform accelerationStructureEXT TLAS;

layout (binding = 5) readonly buffer Lights
{
	int count;
	Light data[];
} lights;

layout (binding = 6) uniform sampler2D globalIlluminationImage;

layout (push_constant) uniform Constant
{
    vec3 camPos;
} constant;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

void main()
{
	vec3 albedo   = texture(albedoImage, uvCoord).rgb;
	vec3 normal   = texture(normalImage, uvCoord).rgb;
    vec3 position = texture(positionImage, uvCoord).rgb;

    vec3 globalIllumination  = texture(globalIlluminationImage, uvCoord).rgb;
    vec3 metallicRoughnessAO = texture(metallicRoughnessAOImage, uvCoord).rgb;

    if (normal == vec3(0) && position == vec3(0) && metallicRoughnessAO == vec3(0))
    {
        fragColor = vec4(albedo, 1.0);
        return;
    }

	float metallic         = metallicRoughnessAO.r;
	float roughness        = metallicRoughnessAO.g;
	float ambientOcclusion = metallicRoughnessAO.b;

    vec3 N = normal;
    vec3 V = normalize(constant.camPos - position);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);

	for (int i = 0; i < lights.count; i++)
	{
		Light light = lights.data[i];
        vec3 L = GetLightDir(light, position);

        if (LightIsOutOfReach(light, position) || LightIsOutOfRange(light, L))
            continue;

        float dist = GetDistanceToLight(light, position);

        rayQueryEXT rq;

        rayQueryInitializeEXT(rq, TLAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, position, 0.0001, L, dist);

        rayQueryProceedEXT(rq);

        if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT)
        {
            continue;
        }

        vec3 H = normalize(V + L);
        float attenuation = GetAttenuation(light, position);
        vec3 radiance = light.color.xyz * attenuation;

        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
           
        float intensity = GetIntensity(light, L);

        vec3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  

        float NdotL = max(dot(N, L), 0.0);        

        Lo += (kD * albedo / PI + specular) * radiance * NdotL * intensity;
	}

    vec3 kS = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness); 
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    vec3 diffuse = globalIllumination * albedo;
    vec3 ambient = (kD * diffuse) * ambientOcclusion; 
    
    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2)); 

    fragColor = vec4(color, 1.0);
}