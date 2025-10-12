#include <filesystem>
#include <fstream>

#include "core/Console.h"
#include "core/EditorProject.h"
#include "core/Scene.h"
#include "io/SceneWriter.h"

namespace fs = std::filesystem;

static fs::path EnsureCorrectExtension(const std::string_view& path)
{
	return fs::path(path).replace_extension(EditorProject::EXTENSION);
}

std::expected<EditorProject, EditorProject::Result> EditorProject::CreateInFile(const std::string_view& path)
{
	if (fs::exists(path))
		return std::unexpected(Result::NotFound);

	fs::path file = EnsureCorrectExtension(path);
	std::ofstream stream(file, std::ios::beg);
	stream << VERSION;

	EditorProject ret(file);

	return ret;
}

std::expected<EditorProject, EditorProject::Result> EditorProject::LoadFromFile(const std::string_view& path)
{
	if (!fs::exists(path))
		return std::unexpected(Result::NotFound);

	fs::path file = path;
	if (!file.has_extension() || file.extension() != EXTENSION)
		Console::WriteLine("given editor project does not have the correct extension: expected \"{}\", but got \"{}\"", Console::Severity::Warning, EXTENSION, file.extension().string());

	std::ifstream stream(file);

	uint32_t version = 0;
	stream >> version;

	if (version != VERSION)
	{
		Console::WriteLine("wrong version of editor project detected: expected {}, but got {}", Console::Severity::Error, VERSION, version);
		return std::unexpected(Result::WrongVersion);
	}
	return EditorProject(path);
}

static std::string GetFileNameWithoutExtension(const fs::path& file)
{
	std::string ret = file.filename().string();
	if (file.has_extension())
		ret = ret.substr(0, ret.find_first_of('.', 0));

	return ret;
}

EditorProject::EditorProject(const fs::path& file)
{
	root = file.parent_path();
	fileName = GetFileNameWithoutExtension(file);
	buildDirectory = root / ".halesia";
	if (!fs::exists(buildDirectory))
		CreateBuildDirectory();
}

void EditorProject::CreateBuildDirectory() const
{
	fs::create_directory(buildDirectory);
}

void EditorProject::BuildScene(const Scene* scene) const
{
	HSFWriter::WriteSceneToArchive(GetBuildFile().string(), scene);
}

fs::path EditorProject::GetBuildFile() const
{
	return buildDirectory / std::format("build_{}.dat", fileName);
}

const fs::path& EditorProject::GetRootDirectory() const
{
	return root;
}

std::string_view EditorProject::GetProjectName() const
{
	return fileName;
}

bool EditorProject::IsValid() const
{
	return !root.empty();
}