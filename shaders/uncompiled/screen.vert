#version 460

vec3 coordinates[6] = vec3[] (
	vec3(-1, -1, 0), vec3(1, 1, 0), vec3(1, -1, 0),
    vec3(1, 1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0)
);

layout(push_constant) uniform constant
{
	vec2 offset;
	vec2 size;
} Constant;

layout (location = 0) out vec2 uvCoord;

void main()
{
	uvCoord = (coordinates[gl_VertexIndex].xy + 1) * 0.5;
	vec3 pos = coordinates[gl_VertexIndex].xyz;
	if (pos.x != -1)
		pos.x = Constant.size.x * 2 - 1;
	if (pos.y != -1)
		pos.y = Constant.size.y * 2 - 1;
	pos.xy += Constant.offset * 2;
	gl_Position = vec4(pos, 1);
}