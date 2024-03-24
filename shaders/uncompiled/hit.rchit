#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_16bit_storage : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#include "layouts.glsl"

#define M_PI 3.1415926535897932384626433832795

hitAttributeEXT vec2 hitCoordinate;

layout(location = 0) rayPayloadInEXT Payload {
  vec3 rayOrigin;
  vec3 rayDirection;
  
  vec3 directColor;
  vec3 indirectColor;
  int rayDepth;

  int rayActive;
  uint64_t intersectedObjectHandle;
  vec3 normal;
  vec3 albedo;
  vec2 motion;
} payload;

struct Vertex
{
    vec3 position;
	vec3 normal;
	vec2 textureCoordinates;
    ivec4 boneIDs1;
	vec4 weights1;
};

struct InstanceMeshData
{
    mat4 transformation;
    uint indexBufferOffset;
    uint vertexBufferOffset;
    uint materialIndex;
    int meshIsLight;
    vec2 motion;
    uint64_t handle;
};

layout(location = 1) rayPayloadEXT bool isShadow;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0) uniform Camera {
  vec4 position;
  mat4 viewInv;
  mat4 projInv;
  uvec2 mouseXY;

  uint frameCount;
  int showUnique;
  int raySamples;
  int rayDepth;
  int renderProgressive;
  int whiteAlbedo;
  vec3 directionalLightDir;
} camera;

layout (binding = 2, set = 0) buffer IndexBuffer { uint16_t data[]; } indexBuffer;
layout (binding = 3, set = 0) buffer VertexBuffer { Vertex data[]; } vertexBuffer;

layout (mesh_instances) buffer InstanceData { InstanceMeshData data[]; } instanceDataBuffer;
layout (bindless_textures) uniform sampler2D[] textures;

mat4 GetModelMatrix()
{
    return instanceDataBuffer.data[gl_InstanceCustomIndexEXT].transformation;
}

vec3 MultiplyPositionWithModelMatrix(Vertex vertex)
{
    return (GetModelMatrix() * vec4(vertex.position, 1)).xyz;
}

vec3 getNormalFromMap(uint normalMapIndex, vec3 barycentric, vec2 fragTexCoord, Vertex vertex1, Vertex vertex2, Vertex vertex3)
{
    vec3 tangentNormal = texture(textures[normalMapIndex], fragTexCoord).xyz * 2.0 - 1.0;

    vec3 Q1  = MultiplyPositionWithModelMatrix(vertex2) - MultiplyPositionWithModelMatrix(vertex1);
    vec3 Q2  = MultiplyPositionWithModelMatrix(vertex3) - MultiplyPositionWithModelMatrix(vertex1);
    vec2 st1 = vertex2.textureCoordinates - vertex1.textureCoordinates;
    vec2 st2 = vertex3.textureCoordinates - vertex1.textureCoordinates;

    vec3 N   = vertex1.normal * barycentric.x + vertex2.normal * barycentric.y + vertex3.normal * barycentric.z;//normalize(cross(vertex2.position - vertex1.position, vertex3.position - vertex1.position));
    N = normalize(mat3(transpose(inverse(GetModelMatrix()))) * N);
//    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
//    vec3 B  = -normalize(cross(N, T));
//    mat3 TBN = mat3(T, B, N);

    return N;//normalize(TBN * tangentNormal);
}

float random(vec2 uv, float seed) {
  return fract(sin(mod(dot(uv, vec2(12.9898, 78.233)) + 1113.1 * seed, M_PI)) * 43758.5453);
}

// PCG (permuted congruential generator). Thanks to:
// www.pcg-random.org and www.shadertoy.com/view/XlGcRh
uint NextRandom(inout uint state)
{
	state = state * 747796405 + 2891336453;
	uint result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
	result = (result >> 22) ^ result;
	return result;
}

float RandomValue(inout uint state)
{
	return NextRandom(state) / 4294967295.0; // 2^32 - 1
}

// Random value in normal distribution (with mean=0 and sd=1)
float RandomValueNormalDistribution(inout uint state)
{
	// Thanks to https://stackoverflow.com/a/6178290
	float theta = 2 * 3.1415926 * RandomValue(state);
	float rho = sqrt(-2 * log(RandomValue(state)));
	return rho * cos(theta);
}

// Calculate a random direction
vec3 RandomDirection(inout uint state)
{
	// Thanks to https://math.stackexchange.com/a/1585996
	float x = RandomValueNormalDistribution(state);
	float y = RandomValueNormalDistribution(state);
	float z = RandomValueNormalDistribution(state);
	return normalize(vec3(x, y, z));
}

vec3 CosineSampleHemisphere(vec2 uv) {
  float radial = sqrt(uv.x);
  float theta = 2.0 * M_PI * uv.y;

  float x = radial * cos(theta);
  float y = radial * sin(theta);

  return vec3(x, y, sqrt(1 - uv.x));
}

float GetFresnelReflect(vec3 normal, vec3 incident, float reflectivity)
{
    float cosX = -dot(normal, incident);
    float x = 1 - cosX;
    float ret = x * x * x * x * x;
    ret = (reflectivity + (1.0 - reflectivity) * ret);
    return ret;
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = M_PI * denom * denom;

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

void main() {
  payload.intersectedObjectHandle = instanceDataBuffer.data[gl_InstanceCustomIndexEXT].handle;
  if (payload.rayActive == 0) {
    return;
  }
  
  vec3 oldColor = payload.directColor;

  uint indexOffset = instanceDataBuffer.data[gl_InstanceCustomIndexEXT].indexBufferOffset;
  uint vertexOffset = instanceDataBuffer.data[gl_InstanceCustomIndexEXT].vertexBufferOffset;
  uint materialIndex = instanceDataBuffer.data[gl_InstanceCustomIndexEXT].materialIndex;
  uint rng = gl_InstanceCustomIndexEXT + gl_PrimitiveID + indexOffset;
  uint state = gl_LaunchIDEXT.x * gl_LaunchIDEXT.y + uint(camera.frameCount * 854.64756f);

  ivec3 indices = ivec3(indexBuffer.data[indexOffset + 3 * gl_PrimitiveID + 0], indexBuffer.data[indexOffset + 3 * gl_PrimitiveID + 1], indexBuffer.data[indexOffset + 3 * gl_PrimitiveID + 2]);
  indices += ivec3(vertexOffset);

  vec3 barycentric = vec3(1.0 - hitCoordinate.x - hitCoordinate.y, hitCoordinate.x, hitCoordinate.y);

  vec3 vertexA = vertexBuffer.data[indices.x].position;
  vec3 vertexB = vertexBuffer.data[indices.y].position;
  vec3 vertexC = vertexBuffer.data[indices.z].position;

  vec3 position = vertexA * barycentric.x + vertexB * barycentric.y + vertexC * barycentric.z;
  position = (GetModelMatrix() * vec4(position, 1)).xyz;

  vec2 uvCoordinates = vertexBuffer.data[indices.x].textureCoordinates * barycentric.x + vertexBuffer.data[indices.y].textureCoordinates * barycentric.y + vertexBuffer.data[indices.z].textureCoordinates * barycentric.z;
  vec3 geometricNormal = getNormalFromMap(materialIndex * 4 + 1, barycentric, uvCoordinates, vertexBuffer.data[indices.x], vertexBuffer.data[indices.y], vertexBuffer.data[indices.z]);

  if (camera.showUnique == 1)
  {
      float x = RandomValue(rng);
      float y = RandomValue(rng);
      float z = RandomValue(rng);
      payload.indirectColor = vec3(x, y, z);
      payload.rayDepth = 1;
      payload.rayActive = 0;
      return;
  }

  vec3 albedo = texture(textures[4 * materialIndex], uvCoordinates).xyz;
  
  if (instanceDataBuffer.data[gl_InstanceCustomIndexEXT].meshIsLight == 1)
  {
    vec3 lightColor = albedo;
    payload.indirectColor += lightColor * payload.directColor;
    albedo = vec3(0);
    payload.rayDepth += 1;
    payload.rayActive = 0;
    payload.normal = geometricNormal;
    payload.albedo = lightColor;
    return;
  }

  float smoothness = 1 - texture(textures[4 * materialIndex + 2], uvCoordinates).g;
  float roughness = 1 - smoothness;
  float metallic = texture(textures[4 * materialIndex + 3], uvCoordinates).b;

  vec3 V = normalize(camera.position.xyz - position);

  vec3 F0 = vec3(0.04); 
  F0 = mix(F0, albedo, metallic);

  vec3 L = normalize(payload.rayOrigin - position);
  vec3 H = normalize(V + L);

  // Cook-Torrance BRDF
  float NDF = DistributionGGX(geometricNormal, H, roughness);   
  float G   = GeometrySmith(geometricNormal, V, L, roughness);      
  vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

  vec3 numerator    = NDF * G * F; 
  float denominator = 4.0 * max(dot(geometricNormal, V), 0.0) * max(dot(geometricNormal, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
  vec3 specular = numerator / denominator;

  vec3 kS = F;
                    
  vec3 kD = vec3(1.0) - kS;
                    
  kD *= 1.0 - metallic;	  

  // scale light by NdotL
  float NdotL = max(dot(geometricNormal, L), 0.0);        

  // add to outgoing radiance Lo
  vec3 Lo = (kD * albedo / M_PI + specular) * NdotL;

  vec3 FRough = fresnelSchlickRoughness(max(dot(geometricNormal, V), 0.0), F0, roughness);

  vec3 kSRough = FRough;
  vec3 kDRough = 1.0 - kS;
  kDRough *= 1.0 - metallic;

  vec3 ambient = kDRough * albedo + payload.directColor * FRough * metallic;
  payload.directColor *= Lo + ambient;//mix(albedo, vec3(1), smoothness * int(isSpecular));
  
  vec3 hemisphere = CosineSampleHemisphere(vec2(random(gl_LaunchIDEXT.xy, camera.frameCount), random(gl_LaunchIDEXT.xy, camera.frameCount + 1)));
  vec3 alignedHemisphere = geometricNormal + RandomDirection(state);
  vec3 specularDirection = reflect(payload.rayDirection, geometricNormal);

  payload.rayOrigin = position;
  payload.rayDirection = normalize(mix(alignedHemisphere, specularDirection, GetFresnelReflect(geometricNormal, payload.rayDirection, smoothness)));
  payload.normal = geometricNormal;
  payload.albedo = albedo;
  payload.motion = instanceDataBuffer.data[gl_InstanceCustomIndexEXT].motion;

  payload.rayDepth += 1;
}