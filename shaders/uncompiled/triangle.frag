#version 450
#extension GL_EXT_nonuniform_qualifier : enable

#define PI 3.14159265359
#define TEXTURES_PER_MATERIAL 5

layout (location = 0) in vec3 fragNormal;
layout (location = 1) in vec2 fragTexCoord;
layout (location = 3) in vec3 worldPos;
layout (location = 4) in vec3 camPos;

layout (location = 0) out vec4 albedoColor;
layout (location = 1) out vec4 normalColor;
layout (location = 2) out vec4 IDColor;
layout (location = 3) out vec4 outColor;

layout (binding = 2) uniform sampler2D texSampler[];

layout (push_constant) uniform PushConstant
{
    mat4 model;
    vec4 IDColor;
    int materialOffset;
} pushConstant;


vec3 getNormalFromMap(int normalMapIndex)
{
    vec3 tangentNormal = texture(texSampler[normalMapIndex], fragTexCoord).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(worldPos);
    vec3 Q2  = dFdy(worldPos);
    vec2 st1 = dFdx(fragTexCoord);
    vec2 st2 = dFdy(fragTexCoord);

    vec3 N   = normalize(fragNormal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

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

void main() {
    int baseIndex = TEXTURES_PER_MATERIAL * pushConstant.materialOffset;

    vec3 albedo = pow(texture(texSampler[baseIndex], fragTexCoord).rgb, vec3(2.2));
    vec3 N = getNormalFromMap(baseIndex + 1);

    albedoColor = vec4(albedo, 1);
    normalColor = vec4(N, 1);
    IDColor = vec4(1);
    outColor = vec4(1);
    return;

    float metallic = texture(texSampler[baseIndex + 2], fragTexCoord).b;
    float roughness = texture(texSampler[baseIndex + 3], fragTexCoord).g;
    float ao = texture(texSampler[baseIndex + 4], fragTexCoord).r;

    if (albedo == vec3(0))
        albedo = vec3(1);
    if (metallic == 0)
        metallic = 0.5;
    if (roughness == 0)
        roughness = 0.5;
    if (ao == 0)
        ao = 0.5;

    //vec3 N = getNormalFromMap(baseIndex + 1);
    vec3 V = normalize(camPos - worldPos);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    vec3 L = normalize(vec3(0, 2, 0) - worldPos);
    vec3 H = normalize(V + L);
    float distance = length(vec3(0, 2, 0) - worldPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = vec3(2);// * attenuation;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
           
    vec3 numerator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular = numerator / denominator;
        
    // kS is equal to Fresnel
    vec3 kS = F;
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    vec3 kD = vec3(1.0) - kS;
    // multiply kD by the inverse metalness such that only non-metals 
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= 1.0 - metallic;	  

    // scale light by NdotL
    float NdotL = max(dot(N, L), 0.0);        

    // add to outgoing radiance Lo
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again

    vec3 ambient = vec3(0.06) * albedo * ao;

    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0/2.2)); 

    outColor = vec4(color, 1.0);
}