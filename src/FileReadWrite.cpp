#include "io/FileMaterial.h"
#include "io/FileArray.h"

#include "renderer/Texture.h"
#include "renderer/Material.h"

FileImage FileImage::CreateFrom(Texture* tex)
{
	FileImage ret;
	ret.data = FileArray<char>::CreateFrom(tex->GetImageData());

	return ret;
}

FileMaterial FileMaterial::CreateFrom(const Material& mat)
{
	FileMaterial ret;

	ret.isLight     = mat.isLight;
	ret.albedo      = FileImage::CreateFrom(mat.albedo);
	ret.normal      = FileImage::CreateFrom(mat.normal);
	ret.roughness   = FileImage::CreateFrom(mat.roughness);
	ret.metallic    = FileImage::CreateFrom(mat.metallic);
	ret.ambientOccl = FileImage::CreateFrom(mat.ambientOcclusion);

	return ret;
}