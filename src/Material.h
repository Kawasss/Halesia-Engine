#pragma once
#include "renderer/Texture.h"

struct Material
{
	// dont know if dynamically allocated is necessary since the material will always be used for the lifetime of the mesh, the class is sort of big so not so sure if copying is cheap
	Texture* albedo = Texture::placeholderAlbedo;
	Texture* normal = Texture::placeholderNormal;
	Texture* metallic = Texture::placeholderMetallic;
	Texture* roughness = Texture::placeholderRoughness;
	Texture* ambientOcclusion = Texture::placeholderAmbientOcclusion;

	Texture* operator[](size_t i)
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

	void Destroy() // only delete the textures if they arent the placeholders
	{
		if (albedo != Texture::placeholderAlbedo) albedo->Destroy();
		if (normal != Texture::placeholderNormal) normal->Destroy();
		if (metallic != Texture::placeholderMetallic) metallic->Destroy();
		if (roughness != Texture::placeholderRoughness) roughness->Destroy();
		if (ambientOcclusion != Texture::placeholderAmbientOcclusion) ambientOcclusion->Destroy();
	}
};

inline bool operator==(const Material& lMaterial, const Material& rMaterial)
{
	return lMaterial.albedo == rMaterial.albedo && lMaterial.normal == rMaterial.normal && lMaterial.metallic == rMaterial.metallic && lMaterial.roughness == rMaterial.roughness && lMaterial.ambientOcclusion == rMaterial.ambientOcclusion;
}