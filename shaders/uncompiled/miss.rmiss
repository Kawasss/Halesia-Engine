#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable

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
} payload;

void main() 
{ 
	payload.currentAlbedo = vec3(0);
	payload.currentNormal = vec3(0);
	vec3 color = payload.rayDirection.y > 0 ? mix(vec3(0.6, 0.9, 1), vec3(0, 0.75, 1), payload.rayDirection.y) : mix(vec3(0.1, 0.1, 0.1), vec3(0.05, 0.05, 0.05), -payload.rayDirection.y);
	if (payload.rayDepth == 0)
	{
		vec3 skyColor = color;
		payload.indirectColor = skyColor;
		payload.rayActive = 0;
		return;
	}

	color =  color / (color + vec3(1));
	color = pow(color, vec3(1 / 2.2));
	float strength = (color.x + color.y + color.z) / 3.0;
	payload.directColor *= color;
	payload.indirectColor += payload.directColor * strength;
	payload.rayDepth++;

	payload.rayActive = 0; 
}