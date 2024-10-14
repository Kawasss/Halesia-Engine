#version 460

layout (location = 0) in  vec3 inPosition;
layout (location = 1) in  vec3 inNormal;
layout (location = 2) in  vec2 inTexCoords;
layout (location = 3) in ivec4 inBoneIDs;
layout (location = 4) in  vec4 inBoneWeights;

layout (location = 0) out vec3 pos;

layout(push_constant) uniform Constant
{
	mat4 projection;
	mat4 view;
} constant;

void main()
{
	pos = inPosition;
	gl_Position = constant.projection * constant.view * vec4(inPosition, 1.0);
}