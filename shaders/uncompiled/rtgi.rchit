#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT Payload {
	vec3 origin;
	vec3 direction;
	vec3 color;

	int isActive;
} payload;

void main()
{
	if (payload.isActive == 0)
		return;

	// read the objects albedo and perform very basic lighting, then add to payload.color
	payload.color += vec3(0.01);
}