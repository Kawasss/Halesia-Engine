#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT Payload {
	vec3 origin;
	vec3 direction;
	vec3 color;

	int isActive;
} payload;

layout(binding = 4, set = 0) uniform samplerCube skybox;

void main()
{
	payload.isActive = 0;
	payload.color += texture(skybox, payload.direction).rgb;
}