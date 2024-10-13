#pragma once
#include "Texture.h"
#include "../ResourceManager.h"

typedef uint64_t Handle;
struct MaterialCreationData;

enum MaterialTexture
{
	MATERIAL_TEXTURE_ALBEDO,
	MATERIAL_TEXTURE_NORMAL,
	MATERIAL_TEXTURE_METALLIC,
	MATERIAL_TEXTURE_ROUGHNESS,
	MATERIAL_TEXTURE_AMBIENT_OCCLUSION
};

inline const std::vector<MaterialTexture> rayTracingMaterialTextures = { MATERIAL_TEXTURE_ALBEDO, MATERIAL_TEXTURE_NORMAL, MATERIAL_TEXTURE_ROUGHNESS, MATERIAL_TEXTURE_METALLIC };
inline const std::vector<MaterialTexture> deferredMaterialTextures = { MATERIAL_TEXTURE_ALBEDO, MATERIAL_TEXTURE_NORMAL };
inline const std::vector<MaterialTexture> PBRMaterialTextures = { MATERIAL_TEXTURE_ALBEDO, MATERIAL_TEXTURE_NORMAL, MATERIAL_TEXTURE_METALLIC, MATERIAL_TEXTURE_ROUGHNESS, MATERIAL_TEXTURE_AMBIENT_OCCLUSION };

struct MaterialCreateInfo
{
	std::string albedo = "";
	std::string normal = "";
	std::string metallic = "";
	std::string roughness = "";
	std::string ambientOcclusion = "";
	bool isLight = false;
};

struct Material
{
	static Material Create(const MaterialCreateInfo& createInfo);
	static Material Create(const MaterialCreationData& createInfo);

	Material() = default;
	Material(Texture* al, Texture* no, Texture* me, Texture* ro, Texture* ao) : albedo(al), normal(no), metallic(me), roughness(ro), ambientOcclusion(ao) {}

	bool HasFinishedLoading();

	void AwaitGeneration();
	void Destroy(); // only delete the textures if they arent the placeholders

	void AddReference();
	void RemoveReference(); // the material will automatically self destruct if referenceCount is 0

	int GetReferenceCount() { return referenceCount; }
	void OverrideReferenceCount(int val) { referenceCount = val; } // should only be called by the renderer itself

	Texture* operator[](size_t i);
	Texture* operator[](MaterialTexture materialTexture);

	Handle handle = ResourceManager::GenerateHandle();

	// dont know if dynamically allocated is necessary since the material will always be used for the lifetime of the mesh, the class is sort of big so not so sure if copying is cheap
	Texture* albedo           = Texture::placeholderAlbedo;
	Texture* normal           = Texture::placeholderNormal;
	Texture* metallic         = Texture::placeholderMetallic;
	Texture* roughness        = Texture::placeholderRoughness;
	Texture* ambientOcclusion = Texture::placeholderAmbientOcclusion;

	bool isLight = false;
	
private:
	int referenceCount = 0;
};

inline extern bool operator==(const Material& lMaterial, const Material& rMaterial);
inline extern bool operator!=(const Material& lMaterial, const Material& rMaterial);