#include "renderer/Material.h"

#include "io/CreationData.h"

Material Material::Create(const MaterialCreateInfo& createInfo)
{
	Material ret{};

	if (!createInfo.albedo.empty())           
		ret.albedo = new Texture(createInfo.albedo);

	if (!createInfo.normal.empty())           
		ret.normal = new Texture(createInfo.normal, true, TEXTURE_FORMAT_UNORM);

	if (!createInfo.metallic.empty())         
		ret.metallic = new Texture(createInfo.metallic);

	if (!createInfo.roughness.empty())        
		ret.roughness = new Texture(createInfo.roughness);

	if (!createInfo.ambientOcclusion.empty()) 
		ret.ambientOcclusion = new Texture(createInfo.ambientOcclusion);

	ret.isLight = createInfo.isLight;
	
	return ret;
}

Material Material::Create(const MaterialCreationData& createInfo)
{
	Material ret{};
	if (createInfo.IsDefault())
		return ret;

	ret.isLight = createInfo.isLight;
	if (!createInfo.albedo.IsDefault())      
		ret.albedo = new Texture(createInfo.albedo.data.data, createInfo.albedo.width, createInfo.albedo.height);

	if (!createInfo.normal.IsDefault())      
		ret.normal = new Texture(createInfo.normal.data.data, createInfo.normal.width, createInfo.normal.height, true, TEXTURE_FORMAT_UNORM);

	if (!createInfo.metallic.IsDefault())    
		ret.metallic = new Texture(createInfo.metallic.data.data, createInfo.metallic.width, createInfo.metallic.height);

	if (!createInfo.roughness.IsDefault())   
		ret.roughness = new Texture(createInfo.roughness.data.data, createInfo.roughness.width, createInfo.roughness.height);

	if (!createInfo.ambientOccl.IsDefault()) 
		ret.ambientOcclusion = new Texture(createInfo.ambientOccl.data.data, createInfo.ambientOccl.width, createInfo.ambientOccl.height);

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

	// reset all textures to their default values, marking the material as unused
	albedo = Texture::placeholderAlbedo;
	normal = Texture::placeholderNormal;
	metallic = Texture::placeholderMetallic;
	roughness = Texture::placeholderRoughness;
	ambientOcclusion = Texture::placeholderAmbientOcclusion;
	isLight = false;
}

void Material::AddReference()
{
	referenceCount++;
}

void Material::RemoveReference()
{
	referenceCount--;
	if (referenceCount <= 0)
		Destroy();
}

bool operator==(const Material& lMaterial, const Material& rMaterial)
{
	return lMaterial.handle == rMaterial.handle && lMaterial.albedo == rMaterial.albedo && lMaterial.normal == rMaterial.normal && lMaterial.metallic == rMaterial.metallic && lMaterial.roughness == rMaterial.roughness && lMaterial.ambientOcclusion == rMaterial.ambientOcclusion && lMaterial.isLight == rMaterial.isLight;
}

bool operator!=(const Material& lMaterial, const Material& rMaterial)
{
	return !(lMaterial == rMaterial);
}