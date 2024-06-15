#version 460
#extension GL_EXT_nonuniform_qualifier : enable

#define PI 3.14159265359
#define TEXTURES_PER_MATERIAL 2

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoords;
layout (location = 3) in vec3 camPos;
layout (location = 4) in flat uint ID;

layout (location = 0) out vec4 result;

uint NextRandom(inout uint state)
{
	state = state * 747796405 + 2891336453;
	uint result = ((state >> ((state >> 28) + 4)) ^ state) * 277803737;
	result = (result >> 22) ^ result;
	return result;
}

float RandomValue(inout uint state)
{
	return NextRandom(state) / 4294967295.0;
}

#define MAX_LIGHT_INDICES 7

struct Cell
{
	float lightCount;
	float lightIndices[MAX_LIGHT_INDICES];
};

layout (set = 0, binding = 3) buffer Cells
{
	uint width;
	uint height;
	Cell data[];
} cells;

layout (set = 0, binding = 4) readonly buffer Lights
{
	vec3 data[];
} lights;

layout(set = 0, binding = 0) uniform sceneInfo {
    vec3 camPos;
    mat4 view;
    mat4 proj;
    uint width;
    uint height;
} ubo;

vec2 GetRelativePosition()
{
    return gl_FragCoord.xy / vec2(ubo.width, ubo.height);
}

uint GetCellIndex()
{
    vec2 cellSpace = GetRelativePosition() * vec2(cells.width, cells.height); 
	uint cellIndex = uint(cells.height * floor(cellSpace.x) + cellSpace.y);

	return cellIndex;
}
//#define HEAT_MAP
void main() 
{
    #ifndef HEAT_MAP

    vec3 lightColor = vec3(1);
    uint state = ID * gl_PrimitiveID;

    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    vec3 diffuse  = vec3(0);
    vec3 specular = vec3(0);
    
    vec3 viewDir = normalize(camPos - position);

    uint cellIndex = GetCellIndex();
    int lightCount = int(cells.data[cellIndex].lightCount);
    for (int i = 0; i < lightCount; i++)
    {
        int lightIndex = int(cells.data[cellIndex].lightIndices[i]);
        vec3 lightPos = lights.data[lightIndex];

        vec3 lightDir   = normalize(lightPos - position);
        vec3 halfwayDir = normalize(lightDir + viewDir);

        float dist = length(lightPos - position);
        float attenuation = 1.0 / (dist * dist);

        float diff = max(dot(normal, lightDir), 0.0);
        diffuse += diff * lightColor * attenuation;

        float spec = pow(max(dot(normal, halfwayDir), 0.0), 0.3);
        specular += spec * lightColor * attenuation;
    }
    vec3 color = vec3(RandomValue(state), RandomValue(state), RandomValue(state));
    result = vec4((ambient + diffuse + specular) * color, 1);

    #else

    uint cellIndex = GetCellIndex();
    int lightCount = int(cells.data[cellIndex].lightCount);
    float rel = lightCount / float(MAX_LIGHT_INDICES);

    result = vec4(rel, 0, 1 - rel, 1);

    #endif
}