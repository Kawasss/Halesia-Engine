#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT Payload {
	vec3 origin;
	vec3 direction;
	vec3 color;

	vec3 normal;

	float pdf;

	int depth;

	int isActive;
} payload;

layout(binding = 5, set = 0) uniform samplerCube skybox;

void main()
{
	payload.isActive = 0;
	payload.color += texture(skybox, normalize(payload.direction)).rgb;
	payload.depth++;

	payload.origin = vec3(0);
	payload.normal = vec3(0);
}