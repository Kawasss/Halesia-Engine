#version 460
layout (location = 0) out vec3 TexCoords;

layout (push_constant) uniform Matrices
{
    mat4 view;
	mat4 projection;
} ubo;

const vec3 coordinates[36] = vec3[](
    vec3(-1.0f,  1.0f, -1.0f),
    vec3(-1.0f, -1.0f, -1.0f),
    vec3( 1.0f, -1.0f, -1.0f),
    vec3( 1.0f, -1.0f, -1.0f),
    vec3( 1.0f,  1.0f, -1.0f),
    vec3(-1.0f,  1.0f, -1.0f),

    vec3(-1.0f, -1.0f,  1.0f),
    vec3(-1.0f, -1.0f, -1.0f),
    vec3(-1.0f,  1.0f, -1.0f),
    vec3(-1.0f,  1.0f, -1.0f),
    vec3(-1.0f,  1.0f,  1.0f),
    vec3(-1.0f, -1.0f,  1.0f),

    vec3( 1.0f, -1.0f, -1.0f),
    vec3( 1.0f, -1.0f,  1.0f),
    vec3( 1.0f,  1.0f,  1.0f),
    vec3( 1.0f,  1.0f,  1.0f),
    vec3( 1.0f,  1.0f, -1.0f),
    vec3( 1.0f, -1.0f, -1.0f),

    vec3(-1.0f, -1.0f,  1.0f),
    vec3(-1.0f,  1.0f,  1.0f),
    vec3( 1.0f,  1.0f,  1.0f),
    vec3( 1.0f,  1.0f,  1.0f),
    vec3( 1.0f, -1.0f,  1.0f),
    vec3(-1.0f, -1.0f,  1.0f),

    vec3(-1.0f,  1.0f, -1.0f),
    vec3( 1.0f,  1.0f, -1.0f),
    vec3( 1.0f,  1.0f,  1.0f),
    vec3( 1.0f,  1.0f,  1.0f),
    vec3(-1.0f,  1.0f,  1.0f),
    vec3(-1.0f,  1.0f, -1.0f),

    vec3(-1.0f, -1.0f, -1.0f),
    vec3(-1.0f, -1.0f,  1.0f),
    vec3( 1.0f, -1.0f, -1.0f),
    vec3( 1.0f, -1.0f, -1.0f),
    vec3(-1.0f, -1.0f,  1.0f),
    vec3( 1.0f, -1.0f,  1.0f)
);

void main()
{
    TexCoords = coordinates[gl_VertexIndex];

	vec4 clipPos = ubo.projection * ubo.view * vec4(coordinates[gl_VertexIndex], 1.0);

	gl_Position = clipPos.xyww;
}