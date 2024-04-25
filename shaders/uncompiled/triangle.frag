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

#define MAX_LIGHT_INDICES 32

struct Cell
{
	float lightCount;
	//float lightIndices[MAX_LIGHT_INDICES];
};

layout (set = 0, binding = 3) buffer Cells
{
	uint depth;
	uint width;
	uint height;
	Cell data[];
} cells;

vec2 GetRelativePosition()
{
    return gl_FragCoord.xy / vec2(800, 600);
}

float GetLinearizedDepth()
{
    return (2 * 0.01f * 1000.0f) / (1000.0f - gl_FragCoord.z * (1000.0f - 0.01f)) / 1000.0f;
}

uint GetCellIndex()
{
    vec2 cellSpace = GetRelativePosition() * vec2(cells.width, cells.height);
    uint sliceSize = uint(cells.width * cells.height);
    uint zIndex = uint(GetLinearizedDepth() * cells.depth) * sliceSize;
	uint cellIndex = uint(cells.height * cellSpace.x + cellSpace.y);

	return zIndex + cellIndex;
}

void main() 
{
    vec3 lightPos = camPos;
    vec3 lightColor = vec3(1);
    uint state = ID * gl_PrimitiveID;
    
    vec3 lightDir   = normalize(lightPos - position);
    vec3 viewDir    = normalize(camPos - position);
    vec3 halfwayDir = normalize(lightDir + viewDir);

    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    float spec = pow(max(dot(normal, halfwayDir), 0.0), 0.3);
    vec3 specular = lightColor * spec;

    vec3 color = vec3(RandomValue(state), RandomValue(state), RandomValue(state));
    //result = vec4((ambient + diffuse + specular) * color, 1);
    float rel = cells.data[GetCellIndex()].lightCount / (32*32);

    result = vec4(rel, 0, 1 - rel, 1);
}