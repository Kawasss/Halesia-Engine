#include "renderer/Material.h"

Material Material::Create(const MaterialCreateInfo& createInfo)
{
	Material ret{};
	if (!createInfo.albedo.empty())           ret.albedo = new Texture(createInfo.creationObject, createInfo.albedo);
	if (!createInfo.normal.empty())           ret.normal = new Texture(createInfo.creationObject, createInfo.normal, true, TEXTURE_FORMAT_UNORM);
	if (!createInfo.metallic.empty())         ret.metallic = new Texture(createInfo.creationObject, createInfo.metallic);
	if (!createInfo.roughness.empty())        ret.roughness = new Texture(createInfo.creationObject, createInfo.roughness);
	if (!createInfo.ambientOcclusion.empty()) ret.ambientOcclusion = new Texture(createInfo.creationObject, createInfo.ambientOcclusion);
	ret.isLight = createInfo.isLight;
	
	return ret;
}

Texture* Material::operator[](size_t i)
{
	switch (i)
	{
	case 0:
		return albedo;
	case 1:
		return normal;
	case 2:
		return metallic;
	case 3:
		return roughness;
	case 4:
		return ambientOcclusion;
	default:
		return albedo;
	}
}

Texture* Material::operator[](MaterialTexture materialTexture)
{
	switch (materialTexture)
	{
	case MATERIAL_TEXTURE_ALBEDO:
		return albedo;
	case MATERIAL_TEXTURE_NORMAL:
		return normal;
	case MATERIAL_TEXTURE_METALLIC:
		return metallic;
	case MATERIAL_TEXTURE_ROUGHNESS:
		return roughness;
	case MATERIAL_TEXTURE_AMBIENT_OCCLUSION:
		return ambientOcclusion;
	default:
		return albedo; // not returning nullptr because that could crash the program
	}
}

bool Material::HasFinishedLoading()
{
	return albedo->HasFinishedLoading() && normal->HasFinishedLoading() && metallic->HasFinishedLoading() && roughness->HasFinishedLoading() && ambientOcclusion->HasFinishedLoading();
}

void Material::AwaitGeneration()
{
	albedo->AwaitGeneration();
	normal->AwaitGeneration();
	metallic->AwaitGeneration();
	roughness->AwaitGeneration();
	ambientOcclusion->AwaitGeneration();
}

void Material::Destroy() // only delete the textures if they arent the placeholders
{
	if (albedo != Texture::placeholderAlbedo) albedo->Destroy();
	if (normal != Texture::placeholderNormal) normal->Destroy();
	if (metallic != Texture::placeholderMetallic) metallic->Destroy();
	if (roughness != Texture::placeholderRoughness) roughness->Destroy();
	if (ambientOcclusion != Texture::placeholderAmbientOcclusion) ambientOcclusion->Destroy();
}

bool operator==(const Material& lMaterial, const Material& rMaterial)
{
	return lMaterial.albedo == rMaterial.albedo && lMaterial.normal == rMaterial.normal && lMaterial.metallic == rMaterial.metallic && lMaterial.roughness == rMaterial.roughness && lMaterial.ambientOcclusion == rMaterial.ambientOcclusion && lMaterial.isLight == rMaterial.isLight;
}

bool operator!=(const Material& lMaterial, const Material& rMaterial)
{
	return !(lMaterial == rMaterial);
}