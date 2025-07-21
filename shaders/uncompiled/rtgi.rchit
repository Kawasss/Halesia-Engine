#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_ray_query : require
#extension GL_EXT_shader_16bit_storage : enable

#include "include/light.glsl"

DECLARE_EXTERNAL_SET(1)

layout (binding = 0, set = 0) uniform accelerationStructureEXT TLAS;

layout(location = 0) rayPayloadInEXT Payload {
	vec3 origin;
	vec3 direction;
	vec3 color;

	vec3 normal;

	float pdf;

	int depth;

	int isActive;
} payload;

layout(push_constant) uniform Camera
{
	uint frame;
	int sampleCount;
	int bounceCount;
	vec3 position;
} camera;

hitAttributeEXT vec2 hitCoordinate;

layout (binding = 6, set = 0) readonly buffer Lights
{
	int count;
	Light data[];
} lights;

struct Vertex
{
    vec3 position;
	vec3 normal;
	vec2 textureCoordinates;
	vec3 tangent;
	vec3 bitangent;
    ivec4 boneIDs1;
	vec4 weights1;
};

layout (binding = 7, set = 0) readonly buffer IndexBuffer 
{ 
	uint data[];
} indexBuffer;

layout (binding = 8, set = 0) readonly buffer VertexBuffer 
{ 
	Vertex data[]; 
} vertexBuffer;

struct InstanceData
{
	uint vertexOffset;
	uint indexOffset;
	int material;
};

layout (binding = 9, set = 0) readonly buffer InstanceDataBuffer
{
	InstanceData data[];
} instanceBuffer;

layout(set = 1, binding = material_buffer_binding) uniform sampler2D[bindless_texture_size] textures;

vec3 GetNormalFromMap(vec2 uv, vec3 normal, vec3 tangent, vec3 bitangent, int materialIndex)
{
	mat3 TBN = mat3(tangent, bitangent, normal);

	vec3 raw = normalize(texture(textures[materialIndex * 5 + 1], uv).rgb);
	vec3 tangentNormal = normalize(raw * 2.0 - 1.0);

	return normalize(TBN * tangentNormal);
}

mat4 GetModelMatrix()
{
	mat4 ret = transpose(mat4(gl_ObjectToWorld3x4EXT));
	ret[3][3] = 1.0;

	return ret;
}

vec3 GetGeometricNormal(vec3 a, vec3 b, vec3 c)
{
	return normalize(cross(b - a, c - a));
}

Vertex GetExactVertex(InstanceData data, vec3 barycentric, out vec3 geometricNormal)
{
	ivec3 indices = ivec3(indexBuffer.data[data.indexOffset + 3 * gl_PrimitiveID + 0], indexBuffer.data[data.indexOffset + 3 * gl_PrimitiveID + 1], indexBuffer.data[data.indexOffset + 3 * gl_PrimitiveID + 2]);
	indices += ivec3(data.vertexOffset);

	Vertex a = vertexBuffer.data[indices.x];
	Vertex b = vertexBuffer.data[indices.y];
	Vertex c = vertexBuffer.data[indices.z];

	geometricNormal = GetGeometricNormal(a.position, b.position, c.position);

	Vertex ret;
	ret.position = a.position * barycentric.x + b.position * barycentric.y + c.position * barycentric.z;
	ret.normal = a.normal * barycentric.x + b.normal * barycentric.y + c.normal * barycentric.z;
	ret.textureCoordinates = a.textureCoordinates * barycentric.x + b.textureCoordinates * barycentric.y + c.textureCoordinates * barycentric.z;
	ret.tangent = a.tangent * barycentric.x + b.tangent * barycentric.y + c.tangent * barycentric.z;
	ret.bitangent = a.bitangent * barycentric.x + b.bitangent * barycentric.y + c.bitangent * barycentric.z;

	mat4 model = GetModelMatrix();
	mat3 normalMatrix = mat3(transpose(inverse(model)));

	ret.position = (model * vec4(ret.position, 1.0)).xyz;
	ret.normal = normalize(normalMatrix * ret.normal);
	ret.tangent = normalize(normalMatrix * ret.tangent);
	ret.bitangent = normalize(normalMatrix * ret.bitangent);

	geometricNormal = normalize(normalMatrix * geometricNormal);

	return ret;
}

void main()
{
	if (payload.isActive == 0)
		return;

	vec3 barycentric = vec3(1.0 - hitCoordinate.x - hitCoordinate.y, hitCoordinate.x, hitCoordinate.y);

	InstanceData instance = instanceBuffer.data[gl_InstanceCustomIndexEXT];

	vec3 geometricNormal = vec3(0);

	Vertex vertex = GetExactVertex(instance, barycentric, geometricNormal);
	vec3 position = vertex.position; // copy this to a variable to prevent expensive struct memory reads

	vec3 normal = GetNormalFromMap(vertex.textureCoordinates, vertex.normal, vertex.tangent, vertex.bitangent, instance.material);

	if (dot(normal, vertex.normal) < 0.0)
		normal = vertex.normal;

	payload.normal = geometricNormal;
	payload.origin = position;
	

	vec3 radiance = vec3(0.0);
	vec3 color = texture(textures[instance.material * 5], vertex.textureCoordinates).rgb; // read the albedo here

	for (int i = 0; i < lights.count; i++) // !! should ray trace against a very low poly version of the scene for shadows 
	{
		Light light = lights.data[i];
		vec3 L = GetLightDir(light, position);

        if (LightIsOutOfReach(light, position) || LightIsOutOfRange(light, L))
            continue;

		float dist = GetDistanceToLight(light, position);

        rayQueryEXT rq;

        rayQueryInitializeEXT(rq, TLAS, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, position, 0.0001, L, dist);

        rayQueryProceedEXT(rq);

        if (rayQueryGetIntersectionTypeEXT(rq, true) != gl_RayQueryCommittedIntersectionNoneEXT)
        {
            continue;
        }

		float attenuation = GetAttenuation(light, position);

		float diff = max(dot(normal, L), 0.0);
		vec3 diffuse = diff * color * light.color.rgb * attenuation;

		radiance += (diffuse) * 1 * GetIntensity(light, L); /*diff / payload.pdf*/; // very rough lighting
	}

	// read the objects albedo and perform very basic lighting, then add to payload.color
	payload.color += radiance;
	payload.depth++;
}