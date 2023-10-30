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
	int drawID;
};

layout(location = 1) rayPayloadEXT bool isShadow;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0) uniform Camera {
  vec4 position;
  vec4 right;
  vec4 up;
  vec4 forward;

  uint frameCount;
  int showNormals;
  int showUnique;
  int raySamples;
  int rayDepth;
}
camera;

layout(binding = 2, set = 0) buffer IndexBuffer { uint16_t data[]; }
indexBuffer;
layout(binding = 3, set = 0) buffer VertexBuffer { Vertex data[]; }
vertexBuffer;

layout(binding = 0, set = 1) buffer MaterialIndexBuffer { uint data[]; }
materialIndexBuffer;
layout(binding = 1, set = 1) buffer MaterialBuffer { Material data[]; }
materialBuffer;

float random(vec2 uv, float seed) {
  return fract(sin(mod(dot(uv, vec2(12.9898, 78.233)) + 1113.1 * seed, M_PI)) * 43758.5453);
}

vec3 uniformSampleHemisphere(vec2 uv) {
  float z = uv.x;
  float r = sqrt(max(0, 1.0 - z * z));
  float phi = 2.0 * M_PI * uv.y;

  return vec3(r * cos(phi), z, r * sin(phi));
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

  ivec3 indices = ivec3(indexBuffer.data[3 * gl_PrimitiveID + 0],
                        indexBuffer.data[3 * gl_PrimitiveID + 1],
                        indexBuffer.data[3 * gl_PrimitiveID + 2]);

  vec3 barycentric = vec3(1.0 - hitCoordinate.x - hitCoordinate.y,
                          hitCoordinate.x, hitCoordinate.y);

vec3 vertexA = vertexBuffer.data[indices.x].position;
vec3 vertexB = vertexBuffer.data[indices.y].position;
vec3 vertexC = vertexBuffer.data[indices.z].position;

  vec3 position = vertexA * barycentric.x + vertexB * barycentric.y +
                  vertexC * barycentric.z;
  vec3 geometricNormal = vertexBuffer.data[indices.x].normal;

  if (camera.showUnique == 1)
  {
      float x = random(vec2(1), 8045389.1568479 * gl_PrimitiveID);
      float y = random(vec2(1), 2754650.7645183 * gl_PrimitiveID);
      float z = random(vec2(1), 1436885.4987659 * gl_PrimitiveID);
      payload.directColor = vec3(x, y, z);
      payload.rayActive = 0;
      return;
  }
  if (camera.showNormals == 1)
  {
      payload.directColor = geometricNormal;
      payload.rayActive = 0;
      return;
  }

  vec3 surfaceColor = vec3(1);//materialBuffer.data[materialIndexBuffer.data[gl_PrimitiveID]].diffuse;

  if (gl_PrimitiveID  == 12 || gl_PrimitiveID  == 13) {
    if (payload.rayDepth == 0) {
      payload.directColor = vec3(1);//materialBuffer.data[materialIndexBuffer.data[gl_PrimitiveID]].emission;
      payload.rayActive = 0;
      return;
    } else {
      payload.indirectColor +=
          (1.0 / payload.rayDepth) * vec3(1) * dot(payload.previousNormal, payload.rayDirection);// materialBuffer.data[materialIndexBuffer.data[gl_PrimitiveID]].emission
    }
  } else {
    int randomIndex = int(random(gl_LaunchIDEXT.xy, camera.frameCount) * 2 + 40);
    vec3 lightColor = vec3(1);

    ivec3 lightIndices = ivec3(indexBuffer.data[3 * randomIndex + 0],
                               indexBuffer.data[3 * randomIndex + 1],
                               indexBuffer.data[3 * randomIndex + 2]);

vec3 lightVertexA = vertexBuffer.data[lightIndices.x].position;
vec3 lightVertexB = vertexBuffer.data[lightIndices.y].position;
vec3 lightVertexC = vertexBuffer.data[lightIndices.z].position;

    vec2 uv = vec2(random(gl_LaunchIDEXT.xy, camera.frameCount),
                   random(gl_LaunchIDEXT.xy, camera.frameCount + 1));
    if (uv.x + uv.y > 1.0f) {
      uv.x = 1.0f - uv.x;
      uv.y = 1.0f - uv.y;
    }

    vec3 lightBarycentric = vec3(1.0 - uv.x - uv.y, uv.x, uv.y);
    vec3 lightPosition = lightVertexA * lightBarycentric.x +
                         lightVertexB * lightBarycentric.y +
                         lightVertexC * lightBarycentric.z;

    vec3 positionToLightDirection = normalize(lightPosition - position);

    vec3 shadowRayOrigin = position;
    vec3 shadowRayDirection = positionToLightDirection;
    float shadowRayDistance = length(lightPosition - position) - 0.001f;

    uint shadowRayFlags = gl_RayFlagsTerminateOnFirstHitEXT |
                          gl_RayFlagsOpaqueEXT |
                          gl_RayFlagsSkipClosestHitShaderEXT;

    isShadow = true;
    traceRayEXT(topLevelAS, shadowRayFlags, 0xFF, 0, 0, 1, shadowRayOrigin,
                0.001, shadowRayDirection, shadowRayDistance, 1);

    if (!isShadow) {
      if (payload.rayDepth == 0) {
        payload.directColor = surfaceColor * lightColor *
                              dot(geometricNormal, positionToLightDirection);
      } else {
        payload.indirectColor +=
            (1.0 / payload.rayDepth) * surfaceColor * lightColor *
            dot(payload.previousNormal, payload.rayDirection) *
            dot(geometricNormal, positionToLightDirection);
      }
    } else {
      if (payload.rayDepth == 0) {
        payload.directColor = vec3(0.0, 0.0, 0.0);
      } else {
        payload.rayActive = 0;
      }
    }
  }

  vec3 hemisphere = uniformSampleHemisphere(
      vec2(random(gl_LaunchIDEXT.xy, camera.frameCount),
           random(gl_LaunchIDEXT.xy, camera.frameCount + 1)));
  vec3 alignedHemisphere =
      alignHemisphereWithCoordinateSystem(hemisphere, geometricNormal);

  payload.rayOrigin = position;
  payload.rayDirection = alignedHemisphere;
  payload.previousNormal = geometricNormal;

  payload.rayDepth += 1;
}

/*#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_debug_printf : enable
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

layout(location = 1) rayPayloadEXT bool isShadow;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0) uniform Camera {
  vec4 position;
  vec4 right;
  vec4 up;
  vec4 forward;

  uint frameCount;
}
camera;

layout(binding = 2, set = 0) buffer IndexBuffer { uint16_t data[]; }
indexBuffer;
layout(binding = 3, set = 0) buffer VertexBuffer { float data[]; }
vertexBuffer;

layout(binding = 0, set = 1) buffer MaterialIndexBuffer { uint data[]; }
materialIndexBuffer;
layout(binding = 1, set = 1) buffer MaterialBuffer { Material data[]; }
materialBuffer;

void main() {
  if (payload.rayActive == 0) {
    return;
  }

  ivec3 indices = ivec3(indexBuffer.data[3 * gl_PrimitiveID + 0],
                        indexBuffer.data[3 * gl_PrimitiveID + 1],
                        indexBuffer.data[3 * gl_PrimitiveID + 2]);

  vec3 barycentric = vec3(1.0 - hitCoordinate.x - hitCoordinate.y,
                          hitCoordinate.x, hitCoordinate.y);

  vec3 vert1 = vec3(vertexBuffer.data[3 * indices.x + 0], vertexBuffer.data[3 * indices.x + 1], vertexBuffer.data[3 * indices.x + 2]);
  vec3 vert2 = vec3(vertexBuffer.data[3 * indices.y + 0], vertexBuffer.data[3 * indices.y + 1], vertexBuffer.data[3 * indices.y + 2]);
  vec3 vert3 = vec3(vertexBuffer.data[3 * indices.z + 0], vertexBuffer.data[3 * indices.z + 1], vertexBuffer.data[3 * indices.z + 2]);
  vec3 position = vert1 * barycentric.x + vert2 * barycentric.y + vert3 * barycentric.z;

  vec3 normal = normalize(cross(vert2 - vert1, vert3 - vert2));
  if (length(normal) <= 0)
    normal *= -1;
  payload.directColor = normal;
  payload.rayActive = 0;
  return;

  if (gl_PrimitiveID == 0 || gl_PrimitiveID == 1)
  {
    if (payload.rayDepth == 0)
    {
        payload.directColor = vec3(1);//vec3(gl_PrimitiveID / 980.0f);//vec3(1);
        payload.rayActive = 0;
        return;
    }
    else
    {
        payload.directColor = vec3(1);//vec3(gl_PrimitiveID / 980.0f);//payload.directColor = vec3(1) / payload.rayDepth;
    }
  }
  else
  {
    payload.rayOrigin = position;
    payload.rayDirection = reflect(payload.rayDirection, normal);
    
  }
  payload.directColor = vec3(1);//-payload.rayDirection + 2 * normal * dot(payload.rayDirection, normal);//vec3(gl_PrimitiveID / 980.0f);
  payload.previousNormal = normal;
  payload.rayDepth += 1;
}*/

/**/