#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT Payload {
	vec3 origin;
	vec3 direction;
	vec3 color;

	int depth;

	int isActive;
} payload;

layout(binding = 5, set = 0) uniform samplerCube skybox;

void main()
{
	payload.isActive = 0;
	payload.color += vec3(1, 0, 0);//texture(skybox, normalize(payload.direction)).rgb;
	payload.depth++;
}