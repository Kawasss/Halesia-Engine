#version 460
layout (location = 0) in  vec3 inPosition;
layout (location = 1) in  vec3 inNormal;
layout (location = 2) in  vec2 inTexCoords;
layout (location = 3) in  vec3 inTangent;
layout (location = 4) in  vec3 inBiTangent;
layout (location = 5) in ivec4 inBoneIDs;
layout (location = 6) in  vec4 inBoneWeights;

layout (binding = 0) buffer UniformData
{
	mat4 view;
	mat4 proj;
	mat4 models[];
} constants;

void main()
{
	gl_Position = constants.proj * constants.view * constants.models[gl_InstanceIndex] * vec4(inPosition, 1.0);
}