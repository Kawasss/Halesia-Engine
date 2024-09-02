#include "renderer/Material.h"
#include "io/SceneLoader.h"

Material Material::Create(const MaterialCreateInfo& createInfo)
{
	Material ret{};
	if (!createInfo.albedo.empty())           ret.albedo = new Texture(createInfo.albedo);
	if (!createInfo.normal.empty())           ret.normal = new Texture(createInfo.normal, true, TEXTURE_FORMAT_UNORM);
	if (!createInfo.metallic.empty())         ret.metallic = new Texture(createInfo.metallic);
	if (!createInfo.roughness.empty())        ret.roughness = new Texture(createInfo.roughness);
	if (!createInfo.ambientOcclusion.empty()) ret.ambientOcclusion = new Texture(createInfo.ambientOcclusion);
	ret.isLight = createInfo.isLight;
	
	return ret;
}

Material Material::Create(const MaterialCreationData& createInfo)
{
	Material ret{};
	ret.isLight = createInfo.isLight;
	if (!createInfo.albedoData.empty())           ret.albedo = new Texture(createInfo.albedoData, createInfo.aWidth, createInfo.aHeight);
	if (!createInfo.normalData.empty())           ret.normal = new Texture(createInfo.normalData, createInfo.nWidth, createInfo.nHeight, true, TEXTURE_FORMAT_UNORM);
	if (!createInfo.metallicData.empty())         ret.metallic = new Texture(createInfo.metallicData, createInfo.mWidth, createInfo.mHeight);
	if (!createInfo.roughnessData.empty())        ret.roughness = new Texture(createInfo.roughnessData, createInfo.rWidth, createInfo.rHeight);
	if (!createInfo.ambientOcclusionData.empty()) ret.ambientOcclusion = new Texture(createInfo.ambientOcclusionData, createInfo.aoWidth, createInfo.aoHeight);
	return ret;
}

Texture* Material::operator[](size_t i)
{
	switch (i)
	{
	case 0:  return albedo;
	case 1:  return normal;
	case 2:  return metallic;
	case 3:  return roughness;
	case 4:  return ambientOcclusion;
	default: return albedo;
	}
}

Texture* Material::operator[](MaterialTexture materialTexture)
{
	switch (materialTexture)
	{
	case MATERIAL_TEXTURE_ALBEDO:            return albedo;
	case MATERIAL_TEXTURE_NORMAL:            return normal;
	case MATERIAL_TEXTURE_METALLIC:          return metallic;
	case MATERIAL_TEXTURE_ROUGHNESS:         return roughness;
	case MATERIAL_TEXTURE_AMBIENT_OCCLUSION: return ambientOcclusion;
	default:                                 return albedo; // not returning nullptr because that could crash the program
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
	if (albedo != Texture::placeholderAlbedo) delete albedo;
	if (normal != Texture::placeholderNormal) delete normal;
	if (metallic != Texture::placeholderMetallic) delete metallic;
	if (roughness != Texture::placeholderRoughness) delete roughness;
	if (ambientOcclusion != Texture::placeholderAmbientOcclusion) delete ambientOcclusion;
}

bool operator==(const Material& lMaterial, const Material& rMaterial)
{
	return lMaterial.handle == rMaterial.handle && lMaterial.albedo == rMaterial.albedo && lMaterial.normal == rMaterial.normal && lMaterial.metallic == rMaterial.metallic && lMaterial.roughness == rMaterial.roughness && lMaterial.ambientOcclusion == rMaterial.ambientOcclusion && lMaterial.isLight == rMaterial.isLight;
}

bool operator!=(const Material& lMaterial, const Material& rMaterial)
{
	return !(lMaterial == rMaterial);
}