#pragma once
#include <string_view>
#include <expected>
#include <filesystem>

namespace fs = std::filesystem;

class Scene;
//TODO: move engine settings into this file
class EditorProject
{
public:
	enum class Result
	{
		Success,
		NotFound,
		WrongVersion,
	};

	static constexpr uint32_t VERSION = 1;
	static constexpr std::string_view EXTENSION = ".hproj";

	static std::expected<EditorProject, Result> CreateInFile(const std::string_view& path); // can only create a project in a file does not exist
	static std::expected<EditorProject, Result> LoadFromFile(const std::string_view& path);

	EditorProject() = default;

	void BuildScene(const Scene* scene) const;

	fs::path GetBuildFile() const;
	const fs::path& GetRootDirectory() const;
	std::string_view GetProjectName() const;

	bool IsValid() const;

private:
	EditorProject(const fs::path& file);

	void CreateBuildDirectory() const;

	fs::path root;
	fs::path buildDirectory;
	std::string fileName;
};