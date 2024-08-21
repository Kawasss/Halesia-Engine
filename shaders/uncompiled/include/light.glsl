#define LightType int
#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

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
		return normalize(-light.pos.xyz);
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

bool LightIsOutOfRange(Light light, vec3 lightDir)
{
	if (light.type.x != LIGHT_TYPE_SPOT)
	{
		return false;
	}

	float theta = dot(lightDir, -light.direction.xyz);
	return theta <= light.pos.w;
}

float GetIntensity(Light light, vec3 lightDir)
{
	if (light.type.x != LIGHT_TYPE_SPOT)
	{
		return 1.0;
	}

	float theta   = dot(lightDir, -light.direction.xyz);
	float epsilon = light.pos.w - light.direction.w;

	return clamp((theta - light.pos.w) / epsilon, 0.0, 1.0);
}