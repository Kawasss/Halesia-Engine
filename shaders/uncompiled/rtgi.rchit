#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_ray_tracing_position_fetch : require
#extension GL_EXT_shader_16bit_storage : enable

#include "include/light.glsl"

DECLARE_EXTERNAL_SET(1)

layout(location = 0) rayPayloadInEXT Payload {
	vec3 origin;
	vec3 direction;
	vec3 color;

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
    ivec4 boneIDs1;
	vec4 weights1;
};

layout (binding = 7, set = 0) readonly buffer IndexBuffer 
{ 
	uint16_t data[];
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

mat4 GetModelMatrix()
{
	mat4 ret = transpose(mat4(gl_ObjectToWorld3x4EXT));
	ret[3][3] = 1.0;

	return ret;
}

Vertex GetExactVertex(InstanceData data, vec3 barycentric)
{
	ivec3 indices = ivec3(indexBuffer.data[data.indexOffset + 3 * gl_PrimitiveID + 0], indexBuffer.data[data.indexOffset + 3 * gl_PrimitiveID + 1], indexBuffer.data[data.indexOffset + 3 * gl_PrimitiveID + 2]);
	indices += ivec3(data.vertexOffset);

	Vertex vertex0 = vertexBuffer.data[indices.x];
	Vertex vertex1 = vertexBuffer.data[indices.y];
	Vertex vertex2 = vertexBuffer.data[indices.z];

	Vertex ret;
	ret.position = vertex0.position * barycentric.x + vertex1.position * barycentric.y + vertex2.position * barycentric.z;
	ret.normal = vertex0.normal * barycentric.x + vertex1.normal * barycentric.y + vertex2.normal * barycentric.z;
	ret.textureCoordinates = vertex0.textureCoordinates * barycentric.x + vertex1.textureCoordinates * barycentric.y + vertex2.textureCoordinates * barycentric.z;

	mat4 model = GetModelMatrix();

	ret.position = (model * vec4(ret.position, 1.0)).xyz;
	ret.normal = normalize(mat3(transpose(inverse(model))) * ret.normal);

	return ret;
}

void main()
{
	if (payload.isActive == 0)
		return;

	vec3 barycentric = vec3(1.0 - hitCoordinate.x - hitCoordinate.y, hitCoordinate.x, hitCoordinate.y);

	InstanceData instance = instanceBuffer.data[gl_InstanceCustomIndexEXT];

	Vertex vertex = GetExactVertex(instance, barycentric);

	payload.direction = vertex.normal;
	payload.origin = vertex.position;

	vec3 radiance = vec3(0.0);
	vec3 color = texture(textures[instance.material * 5], vertex.textureCoordinates).rgb; // read the albedo here

	for (int i = 0; i < lights.count; i++) // !! should ray trace against a very low poly version of the scene for shadows 
	{
		Light light = lights.data[i];
		vec3 L = GetLightDir(light, vertex.position);

        if (LightIsOutOfReach(light, vertex.position) || LightIsOutOfRange(light, L))
            continue;

		float attenuation = GetAttenuation(light, vertex.position);

		float diff = max(dot(vertex.normal, L), 0.0);
		vec3 diffuse = diff * color * light.color.rgb * attenuation;

		radiance += (diffuse) * 1 * GetIntensity(light, L); /*diff / payload.pdf*/; // very rough lighting
	}

	// read the objects albedo and perform very basic lighting, then add to payload.color
	payload.color += radiance;
	payload.depth++;
}