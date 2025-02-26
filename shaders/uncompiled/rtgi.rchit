#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_ray_tracing_position_fetch : require

layout(location = 0) rayPayloadInEXT Payload {
	vec3 origin;
	vec3 direction;
	vec3 color;

	int depth;

	int isActive;
} payload;

hitAttributeEXT vec2 hitCoordinate;

void main()
{
	if (payload.isActive == 0)
		return;

	vec3 barycentric = vec3(1.0 - hitCoordinate.x - hitCoordinate.y, hitCoordinate.x, hitCoordinate.y);

	vec3 vertex0 = (gl_ObjectToWorld3x4EXT * gl_HitTriangleVertexPositionsEXT[0]).xyz;
	vec3 vertex1 = (gl_ObjectToWorld3x4EXT * gl_HitTriangleVertexPositionsEXT[1]).xyz;
	vec3 vertex2 = (gl_ObjectToWorld3x4EXT * gl_HitTriangleVertexPositionsEXT[2]).xyz;

	payload.direction = normalize(cross(vertex1 - vertex0, vertex2 - vertex0));
	payload.origin = vertex0 * barycentric.x + vertex1 * barycentric.y + vertex2 * barycentric.z;

	// read the objects albedo and perform very basic lighting, then add to payload.color
	payload.color += vec3(0.01);
	payload.depth++;
}