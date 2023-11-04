#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_16bit_storage : enable

#define M_PI 3.1415926535897932384626433832795

struct Material {
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  vec3 emission;
};

hitAttributeEXT vec2 hitCoordinate;

layout(location = 0) rayPayloadInEXT Payload {
  vec3 rayOrigin;
  vec3 rayDirection;
  vec3 previousNormal;

  vec3 directColor;
  vec3 indirectColor;
  int rayDepth;

  int rayActive;
}
payload;

struct Vertex
{
    vec3 position;
	vec3 normal;
	vec2 textureCoordinates;
	ivec2 drawID;
};

struct InstanceMeshData
{
    uint indexBufferOffset;
    uint vertexBufferOffset;
    uint materialIndex;
    int meshIsLight;
};

layout(location = 1) rayPayloadEXT bool isShadow;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0) uniform Camera 
{
  vec4 position;
  vec4 right;
  vec4 up;
  vec4 forward;

  uint frameCount;
  int showNormals;
  int showUnique;
  int showAlbedo;
  int raySamples;
  int rayDepth;
  int renderProgressive;
} camera;

layout (binding = 2, set = 0) buffer IndexBuffer { uint16_t data[]; } indexBuffer;
layout (binding = 3, set = 0) buffer VertexBuffer { Vertex data[]; } vertexBuffer;

layout (binding = 0, set = 1) buffer MaterialBuffer { Material data[]; } materialBuffer;
layout (binding = 1, set = 1) buffer ModelBuffer { mat4 data[]; } modelBuffer;
layout (binding = 2, set = 1) buffer InstanceData { InstanceMeshData data[]; } instanceDataBuffer;
layout (binding = 3, set = 1) uniform sampler2D[] textures;

vec3 MultiplyPositionWithModelMatrix(Vertex vertex)
{
    return (modelBuffer.data[vertex.drawID.x] * vec4(vertex.position, 1)).xyz;
}

vec3 getNormalFromMap(uint normalMapIndex, vec3 barycentric, vec2 fragTexCoord, Vertex vertex1, Vertex vertex2, Vertex vertex3)
{
    vec3 tangentNormal = texture(textures[normalMapIndex], fragTexCoord).xyz * 2.0 - 1.0;

    vec3 Q1  = MultiplyPositionWithModelMatrix(vertex2) - MultiplyPositionWithModelMatrix(vertex1);
    vec3 Q2  = MultiplyPositionWithModelMatrix(vertex3) - MultiplyPositionWithModelMatrix(vertex1);
    vec2 st1 = vertex2.textureCoordinates - vertex1.textureCoordinates;
    vec2 st2 = vertex3.textureCoordinates - vertex1.textureCoordinates;

    vec3 N   = vertex1.normal * barycentric.x + vertex2.normal * barycentric.y + vertex3.normal * barycentric.z;
    N = normalize(mat3(transpose(inverse(modelBuffer.data[vertex1.drawID.x]))) * N);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return N;//normalize(TBN * tangentNormal);
}

float random(vec2 uv, float seed) {
  return fract(sin(mod(dot(uv, vec2(12.9898, 78.233)) + 1113.1 * seed, M_PI)) * 43758.5453);
}

vec3 uniformSampleHemisphere(vec2 uv) {
  float radial = sqrt(uv.x);
  float theta = 2.0 * M_PI * uv.y;

  float x = radial * cos(theta);
  float y = radial * sin(theta);

  return vec3(x, y, sqrt(1 - uv.x));
}

vec3 alignHemisphereWithCoordinateSystem(vec3 hemisphere, vec3 up) {
  vec3 right = normalize(cross(up, vec3(0.0072f, 1.0f, 0.0034f)));
  vec3 forward = cross(right, up);

  return hemisphere.x * right + hemisphere.y * up + hemisphere.z * forward;
}

void main() {
  if (payload.rayActive == 0) {
    return;
  }

  vec3 oldColor = payload.directColor;

  uint indexOffset = instanceDataBuffer.data[gl_InstanceCustomIndexEXT].indexBufferOffset;
  uint vertexOffset = instanceDataBuffer.data[gl_InstanceCustomIndexEXT].vertexBufferOffset;
  uint materialIndex = instanceDataBuffer.data[gl_InstanceCustomIndexEXT].materialIndex;

  ivec3 indices = ivec3(indexBuffer.data[indexOffset + 3 * gl_PrimitiveID + 0], indexBuffer.data[indexOffset + 3 * gl_PrimitiveID + 1], indexBuffer.data[indexOffset + 3 * gl_PrimitiveID + 2]);
  indices += ivec3(vertexOffset);

  vec3 barycentric = vec3(1.0 - hitCoordinate.x - hitCoordinate.y, hitCoordinate.x, hitCoordinate.y);

  vec3 vertexA = vertexBuffer.data[indices.x].position;
  vec3 vertexB = vertexBuffer.data[indices.y].position;
  vec3 vertexC = vertexBuffer.data[indices.z].position;

  vec3 position = vertexA * barycentric.x + vertexB * barycentric.y + vertexC * barycentric.z;
  position = (modelBuffer.data[vertexBuffer.data[indices.x].drawID.x] * vec4(position, 1)).xyz;

  vec2 uvCoordinates = vertexBuffer.data[indices.x].textureCoordinates * barycentric.x + vertexBuffer.data[indices.y].textureCoordinates * barycentric.y + vertexBuffer.data[indices.z].textureCoordinates * barycentric.z;
  vec3 geometricNormal = getNormalFromMap(materialIndex * 5 + 1, barycentric, uvCoordinates, vertexBuffer.data[indices.x], vertexBuffer.data[indices.y], vertexBuffer.data[indices.z]);

  if (camera.showAlbedo == 1)
  {
    payload.indirectColor = texture(textures[5 * materialIndex], uvCoordinates).xyz;
    payload.rayDepth = 1;
    payload.rayActive = 0;
    return;
  }
  if (camera.showUnique == 1)
  {
      float x = random(vec2(1), 8045389.1568479 * (gl_InstanceCustomIndexEXT + gl_PrimitiveID));
      float y = random(vec2(1), 2754650.7645183 * (gl_InstanceCustomIndexEXT + gl_PrimitiveID));
      float z = random(vec2(1), 1436885.4987659 * (gl_InstanceCustomIndexEXT + gl_PrimitiveID));
      payload.indirectColor = vec3(x, y, z);
      payload.rayDepth = 1;
      payload.rayActive = 0;
      return;
  }
  if (camera.showNormals == 1)
  {
      payload.indirectColor = geometricNormal;
      payload.rayDepth = 1;
      payload.rayActive = 0;
      return;
  }

  vec3 surfaceColor = texture(textures[5 * materialIndex], uvCoordinates).xyz;
  float smoothness = 1 - texture(textures[5 * materialIndex + 3], uvCoordinates).g;
  if (smoothness == 1)
     smoothness = 0.995;
  else if (smoothness == 0.5)
    smoothness = 0.6;
  float metallic = texture(textures[5 * materialIndex + 2], uvCoordinates).b;
    
  bool isSpecular = metallic >= random(gl_LaunchIDEXT.xy, camera.frameCount * (gl_InstanceCustomIndexEXT + gl_PrimitiveID));

  if (instanceDataBuffer.data[gl_InstanceCustomIndexEXT].meshIsLight == 1)
  {
    vec3 lightColor = vec3(1);
    payload.indirectColor += lightColor * payload.directColor;
    surfaceColor = vec3(0);
    payload.rayDepth = 1;
    payload.rayActive = 0;
    return;
  }
  else if (gl_InstanceCustomIndexEXT == 2)
  {
    //payload.indirectColor += vec3(0.05) * payload.directColor;
  }
  payload.directColor *= surfaceColor;



  vec3 hemisphere = uniformSampleHemisphere(vec2(random(gl_LaunchIDEXT.xy, camera.frameCount), random(gl_LaunchIDEXT.xy, camera.frameCount + 1)));
  vec3 alignedHemisphere = alignHemisphereWithCoordinateSystem(hemisphere, geometricNormal);
  vec3 specularDirection = reflect(payload.rayDirection, geometricNormal);

  payload.rayOrigin = position;
  payload.rayDirection = normalize(mix(alignedHemisphere, specularDirection, smoothness));
  payload.previousNormal = geometricNormal;

  payload.rayDepth += 1;
}