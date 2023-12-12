#version 450
#extension GL_EXT_nonuniform_qualifier : enable

#define PI 3.14159265359
#define TEXTURES_PER_MATERIAL 2

layout (location = 0) out vec4 albedoColor;

void main() {
    vec3 color = vec3(0.2, 1, 0.2);
    albedoColor = vec4(color, 1);
}