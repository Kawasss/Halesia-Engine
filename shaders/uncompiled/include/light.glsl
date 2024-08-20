#define LightType int
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

struct Light
{
	vec3 pos;
	vec3 color;
	LightType type;
};