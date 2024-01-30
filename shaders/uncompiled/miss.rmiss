#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

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

void main() 
{ 
	payload.albedo = vec3(0);
	payload.normal = vec3(0);
	float co = -dot(camera.directionalLightDir, payload.normal);
	vec3 color = vec3(1) * co;
	if (payload.rayDepth == 0)
	{
		vec3 skyColor = payload.rayDirection.y > 0 ? mix(vec3(0.7, 1, 1), vec3(0.1, 0.85, 1), payload.rayDirection.y) : vec3(0.2);//mix(vec3(0.2), vec3(0.1), -payload.rayDirection.y);
		payload.indirectColor = skyColor;
		payload.rayActive = 0;
		payload.rayDepth = 1;
		
		return;
	}

	color =  color / (color + vec3(1));
	color = pow(color, vec3(1 / 2.2));
	
	payload.directColor *= color;
	payload.indirectColor += payload.directColor;// * strength;
	payload.rayDepth++;

	payload.rayActive = 0; 
}