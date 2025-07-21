#define LightType int
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

#define LIGHT_OUT_OF_REACH_TRESHOLD 0.001

#ifdef __cplusplus // my syntax highlighter doesnt recognize this as shader code so add some function definitions for the syntax highlighting

struct vec3
{
	float x, y, z;
	float xyz;
};

struct vec4
{
	float x, y, z, w;
	float xyz;
};

struct ivec4
{
	int x, y, z, w;
	float xyz;
};

vec3 normalize(float);
float length(float);
float dot(vec3, float);
float clamp(float, float, float);

float operator-(float v1, vec3 v2);

#endif // __cplusplus

struct Light
{
	vec4 pos; // w is padding
	vec4 color; // w is padding
	vec4 direction; // only for spot lights (if type is LIGHT_TYPE_SPOT the w is for the cutoff, the outer cutoff is placed inside pos.w)
	ivec4 type; // w is padding
};

vec3 GetLightDir(Light light, vec3 pos)
{
	if (light.type.x == LIGHT_TYPE_DIRECTIONAL)
	{
		return normalize(-light.direction.xyz);
	}
	else
	{
		return normalize(light.pos.xyz - pos);
	}
}

float GetAttenuation(Light light, vec3 pos)
{
	if (light.type.x == LIGHT_TYPE_DIRECTIONAL)
	{
		return 1.0;
	}
	else
	{
		float dist = length(light.pos.xyz - pos);
		return 1.0 / (dist * dist);
	}
}

bool LightIsOutOfReach(Light light, vec3 pos)
{
	if (light.type.x == LIGHT_TYPE_DIRECTIONAL)
		return false;

	float attenuation = GetAttenuation(light, pos);
	return attenuation < LIGHT_OUT_OF_REACH_TRESHOLD;
}

bool LightIsOutOfRange(Light light, vec3 lightDir)
{
	if (light.type.x != LIGHT_TYPE_SPOT)
	{
		return false;
	}

	float theta = dot(lightDir, normalize(-light.direction.xyz));
	return theta <= light.pos.w;
}

float GetIntensity(Light light, vec3 lightDir)
{
	if (light.type.x != LIGHT_TYPE_SPOT)
	{
		return 1.0;
	}

	float theta   = dot(lightDir, normalize(-light.direction.xyz));
	float epsilon = light.direction.w - light.pos.w;

	return clamp((theta - light.pos.w) / epsilon, 0.0, 1.0);
}