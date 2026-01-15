module;

export module Core.EditorProject;

import std;

import Core.Scene;

namespace fs = std::filesystem;

//TODO: move engine settings into this file
export class EditorProject
{
public:
	enum class Result
	{
		Success,
		NotFound,
		WrongVersion,
	};

	struct UncheckedFile
	{
		std::uint32_t version = 0;
		std::string workingDirectory;
		std::string buildDirectory;

		bool AssignValueToIdentifier(const std::string_view& identifier, const std::string_view& value); // returns false if the identifier is invalid or the value not be parsed
	};

	static constexpr std::uint32_t INVALID_VERSION = 0;
	static constexpr std::uint32_t VERSION = 1;
	static constexpr std::string_view EXTENSION = ".hproj";

	static std::expected<EditorProject, Result> CreateInFile(const std::string_view& path); // can only create a project in a file that does not exist
	static std::expected<EditorProject, Result> LoadFromFile(const std::string_view& path);

	EditorProject() = default;

	void BuildScene(const Scene* scene) const;

	fs::path GetBuildFile() const;
	const fs::path& GetWorkingDirectory() const;
	std::string_view GetProjectName() const;

	bool IsValid() const;

private:
	EditorProject(const fs::path& file, const fs::path& workingDir, const fs::path& buildDir); // the build and working directory are ALWAYS relative to the project file path

	void CreateBuildDirectory() const;

	static UncheckedFile ProcessData(const std::string_view& data);

	fs::path root;
	fs::path buildDirectory;
	std::string fileName;
};