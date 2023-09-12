#version 450
#extension GL_EXT_nonuniform_qualifier : enable

#define TEXTURES_PER_MATERIAL 2

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
flat layout (location = 2) in int drawID;

layout(location = 0) out vec4 outColor;

layout(binding = 2) uniform sampler2D texSampler[];

void main() {
    outColor = texture(texSampler[TEXTURES_PER_MATERIAL * drawID], fragTexCoord);
    if (outColor == vec4(0))
	outColor = vec4(fragNormal, 1);
}