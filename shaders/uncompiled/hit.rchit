#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_16bit_storage : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

#define M_PI 3.1415926535897932384626433832795

hitAttributeEXT vec2 hitCoordinate;

layout(location = 0) rayPayloadInEXT Payload {
  vec3 rayOrigin;
  vec3 rayDirection;
  vec3 previousNormal;
  
  vec3 directColor;
  vec3 indirectColor;
  int rayDepth;

  int rayActive;
  uint64_t intersectedObjectHandle;
  vec3 currentNormal;
  vec3 currentAlbedo;
}
payload;

struct Vertex
{
    vec3 position;
	vec3 normal;
	vec2 textureCoordinates;
    vec2 padding;
};

struct InstanceMeshData
{
    mat4 transformation;
    uint indexBufferOffset;
    uint vertexBufferOffset;
    uint materialIndex;
    int meshIsLight;
    uint64_t handle;
    uint64_t padding;
};

layout(location = 1) rayPayloadEXT bool isShadow;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0) uniform Camera 
{
  vec4 position;
  mat4 viewInv;
  mat4 projInv;
  uvec2 mouseXY;

  uint frameCount;
  int showUnique;
  int raySamples;
  int rayDepth;
  int renderProgressive;
  int whiteAbledo;
} camera;

layout (binding = 2, set = 0) buffer IndexBuffer { uint16_t data[]; } indexBuffer;
layout (binding = 3, set = 0) buffer VertexBuffer { Vertex data[]; } vertexBuffer;

layout (binding = 0, set = 1) buffer InstanceData { InstanceMeshData data[]; } instanceDataBuffer;
layout (binding = 1, set = 1) uniform sampler2D[] textures;

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
    //vec3 tangentNormal = texture(textures[normalMapIndex], fragTexCoord).xyz * 2.0 - 1.0;

    vec3 Q1  = MultiplyPositionWithModelMatrix(vertex2) - MultiplyPositionWithModelMatrix(vertex1);
    vec3 Q2  = MultiplyPositionWithModelMatrix(vertex3) - MultiplyPositionWithModelMatrix(vertex1);
    vec2 st1 = vertex2.textureCoordinates - vertex1.textureCoordinates;
    vec2 st2 = vertex3.textureCoordinates - vertex1.textureCoordinates;

    vec3 N   = normalize(cross(vertex2.position - vertex1.position, vertex3.position - vertex1.position));//vertex1.normal * barycentric.x + vertex2.normal * barycentric.y + vertex3.normal * barycentric.z;
    N = normalize(mat3(transpose(inverse(GetModelMatrix()))) * N);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

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

  vec3 surfaceColor = texture(textures[4 * materialIndex], uvCoordinates).xyz;
  float smoothness = 1 - texture(textures[4 * materialIndex + 2], uvCoordinates).g;
  float metallic = texture(textures[4 * materialIndex + 3], uvCoordinates).b;

  bool isSpecular = metallic >= RandomValue(state);

  if (instanceDataBuffer.data[gl_InstanceCustomIndexEXT].meshIsLight == 1)
  {
    vec3 lightColor = vec3(1);
    payload.indirectColor += lightColor * payload.directColor;
    surfaceColor = vec3(0);
    payload.rayDepth += 1;
    payload.rayActive = 0;
    payload.currentNormal = geometricNormal;
    payload.currentAlbedo = lightColor;
    return;
  }
  payload.directColor *= mix(surfaceColor, vec3(1), smoothness * int(isSpecular));
  
  vec3 hemisphere = CosineSampleHemisphere(vec2(random(gl_LaunchIDEXT.xy, camera.frameCount), random(gl_LaunchIDEXT.xy, camera.frameCount + 1)));
  vec3 alignedHemisphere = geometricNormal + RandomDirection(state);
  vec3 specularDirection = reflect(payload.rayDirection, geometricNormal);

  payload.rayOrigin = position;
  payload.rayDirection = normalize(mix(alignedHemisphere, specularDirection, GetFresnelReflect(geometricNormal, payload.rayDirection, smoothness)));
  payload.previousNormal = geometricNormal;
  payload.currentNormal = geometricNormal;
  payload.currentAlbedo = surfaceColor;

  payload.rayDepth += 1;
}