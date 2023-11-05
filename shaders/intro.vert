#version 460

vec3 coordinates[6] = vec3[] (
	vec3(-1, -1, 0), vec3(1, 1, 0), vec3(1, -1, 0),
    vec3(1, 1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0)
);

layout (location = 0) out vec2 uvCoord;

void main()
{
	uvCoord = coordinates[gl_VertexIndex].xy;
	gl_Position = vec4(coordinates[gl_VertexIndex], 1);
}