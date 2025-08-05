#version 460
layout (location = 0) in  vec3 inPosition;
layout (location = 1) in  vec3 inNormal;
layout (location = 2) in  vec2 inTexCoords;
layout (location = 3) in  vec3 inTangent;
layout (location = 4) in  vec3 inBiTangent;
layout (location = 5) in ivec4 inBoneIDs;
layout (location = 6) in  vec4 inBoneWeights;

layout (binding = 0) uniform UniformData
{
	mat4 view;
	mat4 proj;
} constants;

layout(push_constant) uniform PushConstant
{
	mat4 model;
} pushConstant;

void main()
{
	gl_Position = constants.proj * constants.view * pushConstant.model * vec4(inPosition, 1.0);
}