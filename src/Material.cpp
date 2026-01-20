#include "renderer/Material.h"
#include "renderer/Texture.h"

#include "io/CreationData.h"

std::array<TextureType, 5> Material::pbrTextures =
{
	TextureType::Albedo,
	TextureType::Normal,
	TextureType::Metallic,
	TextureType::Roughness,
	TextureType::AmbientOcclusion,
};

Material::Material()
{
	albedo = Texture::placeholderAlbedo;
	normal = Texture::placeholderNormal;
	metallic = Texture::placeholderMetallic;
	roughness = Texture::placeholderRoughness;
	ambientOcclusion = Texture::placeholderAmbientOcclusion;
}

Material::Material(Texture* al, Texture* no, Texture* me, Texture* ro, Texture* ao) : albedo(al), normal(no), metallic(me), roughness(ro), ambientOcclusion(ao)
{

}

Material Material::Create(const MaterialCreateInfo& createInfo)
{
	Material ret{};

	if (!createInfo.albedo.empty())           
		ret.albedo = Texture::LoadFromForeignFormat(createInfo.albedo, Texture::Type::Albedo, false);

	if (!createInfo.normal.empty())           
		ret.normal = Texture::LoadFromForeignFormat(createInfo.normal, Texture::Type::Normal, false);

	if (!createInfo.metallic.empty())         
		ret.metallic = Texture::LoadFromForeignFormat(createInfo.metallic, Texture::Type::Metallic, false);

	if (!createInfo.roughness.empty())        
		ret.roughness = Texture::LoadFromForeignFormat(createInfo.roughness, Texture::Type::Roughness, false);

	if (!createInfo.ambientOcclusion.empty()) 
		ret.ambientOcclusion = Texture::LoadFromForeignFormat(createInfo.ambientOcclusion, Texture::Type::AmbientOcclusion, false);

	ret.isLight = createInfo.isLight;
	ret.EnsurePointerSafety();
	return ret;
}

Material Material::Create(const MaterialCreationData& createInfo)
{
	Material ret{};
	if (createInfo.IsDefault())
		return ret;

	ret.isLight = createInfo.isLight;
	if (!createInfo.albedo.IsDefault())      
		ret.albedo = Texture::LoadFromInternalFormat(createInfo.albedo.data.data, false);

	if (!createInfo.normal.IsDefault())      
		ret.normal = Texture::LoadFromInternalFormat(createInfo.normal.data.data, false);

	if (!createInfo.metallic.IsDefault())    
		ret.metallic = Texture::LoadFromInternalFormat(createInfo.metallic.data.data, false);

	if (!createInfo.roughness.IsDefault())   
		ret.roughness = Texture::LoadFromInternalFormat(createInfo.roughness.data.data, false);

	if (!createInfo.ambientOccl.IsDefault()) 
		ret.ambientOcclusion = Texture::LoadFromInternalFormat(createInfo.ambientOccl.data.data, false);

	ret.EnsurePointerSafety();
	return ret;
}

const Texture* Material::operator[](size_t i) const
{
	return GetTexture(i);
}

const Texture* Material::operator[](TextureType materialTexture) const
{
	return this->operator[](static_cast<size_t>(materialTexture));
}

Texture*& Material::GetTexture(size_t i)
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

const Texture* const& Material::GetTexture(size_t i) const
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

int Material::GetReferenceCount() const
{
	return referenceCount;
}

void Material::OverrideReferenceCount(int val)
{ 
	referenceCount = val;
}

void Material::EnsurePointerSafety()
{
	if (albedo == nullptr)
		albedo = Texture::placeholderAlbedo;
	if (normal == nullptr)
		normal = Texture::placeholderNormal;
	if (metallic == nullptr)
		metallic = Texture::placeholderMetallic;
	if (roughness == nullptr)
		roughness = Texture::placeholderRoughness;
	if (ambientOcclusion == nullptr)
		ambientOcclusion = Texture::placeholderAmbientOcclusion;
}

bool operator==(const Material& lMaterial, const Material& rMaterial)
{
	return lMaterial.handle == rMaterial.handle && lMaterial.albedo == rMaterial.albedo && lMaterial.normal == rMaterial.normal && lMaterial.metallic == rMaterial.metallic && lMaterial.roughness == rMaterial.roughness && lMaterial.ambientOcclusion == rMaterial.ambientOcclusion && lMaterial.isLight == rMaterial.isLight;
}

bool operator!=(const Material& lMaterial, const Material& rMaterial)
{
	return !(lMaterial == rMaterial);
}