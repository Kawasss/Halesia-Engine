#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT Payload {
  vec3 rayOrigin;
  vec3 rayDirection;
  vec3 previousNormal;

  vec3 directColor;
  vec3 indirectColor;
  int rayDepth;

  int rayActive;
} payload;

void main() 
{ 
//	float skyGradientT = pow(smoothstep(0, 0.4, payload.rayDirection.y), 0.35);
//	float groundToSkyT = smoothstep(-0.01, 0, payload.rayDirection.y);
//	vec3 skyGradient = mix(vec3(0.537, 0.812, 0.941), vec3(0, 1, 0), skyGradientT);
//	float sun = pow(max(0, dot(payload.rayDirection, vec3(1, 0, 0))), 1);
//	vec3 composite = mix(vec3(1, 0, 0), skyGradient, groundToSkyT) * sun * float(groundToSkyT >= 1);

//	payload.indirectColor = composite;
	if (payload.rayDepth == 0)
	{
		payload.rayDepth = 1;
		vec3 skyColor = payload.rayDirection.y > 0 ? mix(vec3(0.6, 0.9, 1), vec3(0, 0.75, 1), payload.rayDirection.y) : mix(vec3(0.1, 0.1, 0.1), vec3(0.05, 0.05, 0.05), -payload.rayDirection.y);
		payload.indirectColor = skyColor;
		payload.rayActive = 0;
		return;
	}

	payload.rayActive = 0; 
}