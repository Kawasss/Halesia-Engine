#define LightType int
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

struct Light
{
	vec4 pos; // z is padding
	vec4 color; // z is padding
	vec4 data; // z is padding
	ivec4 type; // z is padding
};