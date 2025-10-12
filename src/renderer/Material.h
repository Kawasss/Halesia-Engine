#pragma once
#include <array>
#include <string>

#include "../io/FwdDclCreationData.h"

class Texture;
using Handle = uint64_t;

enum class TextureType
{
	Albedo = 0,
	Normal = 1,
	Metallic = 2,
	Roughness = 3,
	AmbientOcclusion = 4,
};

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
	static std::array<TextureType, 5> pbrTextures;

	static Material Create(const MaterialCreateInfo& createInfo);
	static Material Create(const MaterialCreationData& createInfo);

	Material();
	Material(Texture* al, Texture* no, Texture* me, Texture* ro, Texture* ao);

	bool HasFinishedLoading();

	void AwaitGeneration();
	void Destroy(); // only delete the textures if they arent the placeholders

	void AddReference();
	void RemoveReference(); // the material will automatically self destruct if referenceCount is 0

	int GetReferenceCount() const;
	void OverrideReferenceCount(int val); // should only be called by the renderer itself

	const Texture* operator[](size_t i) const;
	const Texture* operator[](TextureType materialTexture) const;

	Handle handle = 0;

	// dont know if dynamically allocated is necessary since the material will always be used for the lifetime of the mesh, the class is sort of big so not so sure if copying is cheap
	Texture* albedo = nullptr;
	Texture* normal = nullptr;
	Texture* metallic = nullptr;
	Texture* roughness = nullptr;
	Texture* ambientOcclusion = nullptr;

	bool isLight = false;
	
private:
	const Texture* const& GetTexture(size_t i) const;
	Texture*& GetTexture(size_t i);

	void EnsurePointerSafety();

	int referenceCount = 0;
};

inline extern bool operator==(const Material& lMaterial, const Material& rMaterial);
inline extern bool operator!=(const Material& lMaterial, const Material& rMaterial);