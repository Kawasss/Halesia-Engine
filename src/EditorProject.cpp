module;

#include "core/Console.h"

module Core.EditorProject;

import std;

import Core.Scene;

import IO.SceneWriter;
import IO;

import System.FileDialog;

import StrUtil;

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
	stream 
		<< "version=" << VERSION << "\r\n"
		<< "workingDirectory=" << "\r\n"
		<< "buildDirectory=" << ".halesia/\r\n";

	EditorProject ret(file, "", file.parent_path() / ".halesia");

	return ret;
}

std::expected<EditorProject, EditorProject::Result> EditorProject::LoadFromFile(const std::string_view& path)
{
	if (!fs::exists(path))
		return std::unexpected(Result::NotFound);

	fs::path file = path;
	if (!file.has_extension() || file.extension() != EXTENSION)
		Console::WriteLine("given editor project does not have the correct extension: expected \"{}\", but got \"{}\"", Console::Severity::Warning, EXTENSION, file.extension().string());

	std::vector<char> data = *IO::ReadFile(path);
	UncheckedFile raw = ProcessData(data.data());
	
	if (raw.version != VERSION)
	{
		Console::WriteLine("wrong version of editor project detected: expected {}, but got {}", Console::Severity::Error, VERSION, raw.version);
		return std::unexpected(Result::WrongVersion);
	}
	return EditorProject(path, raw.workingDirectory, raw.buildDirectory);
}

EditorProject EditorProject::CreateInMemory()
{
	return EditorProject();
}

static std::string GetFileNameWithoutExtension(const fs::path& file)
{
	std::string ret = file.filename().string();
	if (file.has_extension())
		ret = ret.substr(0, ret.find_first_of('.', 0));

	return ret;
}

EditorProject::EditorProject(const fs::path& file, const fs::path& workingDir, const fs::path& buildDir)
{
	ConstructStorage(file, workingDir, buildDir);
}

void EditorProject::Storage::CreateBuildDirectory() const
{
	if (exists)
		fs::create_directory(buildDir);
}

fs::path EditorProject::Storage::GetBuildFile() const
{
	return buildDir / std::format("build_{}.dat", name);
}

fs::path EditorProject::Storage::GetProjectFile() const
{
	return root / std::format("{}{}", name, EXTENSION);
}

bool EditorProject::Storage::ReadyForWriting() const
{
	return fs::exists(buildDir) && fs::exists(GetProjectFile());
}

void EditorProject::ConstructStorage(const fs::path& file, const fs::path& workingDir, const fs::path& buildDir)
{
	fs::path base = file.parent_path();

	storage.name = GetFileNameWithoutExtension(file);
	storage.root = base / workingDir;
	storage.buildDir = base / buildDir;

	if (!fs::exists(storage.buildDir))
		storage.CreateBuildDirectory();

	storage.exists = true;
}

void EditorProject::CreateStorageInFile(const std::string_view& path)
{
	fs::path file = EnsureCorrectExtension(path);
	std::ofstream stream(file, std::ios::beg);
	stream
		<< "version=" << VERSION << "\r\n"
		<< "workingDirectory=" << "\r\n"
		<< "buildDirectory=" << ".halesia/\r\n";

	ConstructStorage(file, "", file.parent_path() / ".halesia");
}

EditorProject::SaveResult EditorProject::BuildScene(const Scene* scene) const
{
	if (!storage.exists)
		return SaveResult::DoesNotExist;

	if (!storage.ReadyForWriting())
		return SaveResult::BadLocation;

	SceneWriter::WriteSceneToArchive(storage.GetBuildFile().string(), scene);
	return SaveResult::Success;
}

fs::path EditorProject::GetBuildFile() const
{
	return storage.GetBuildFile();
}

bool EditorProject::UncheckedFile::AssignValueToIdentifier(const std::string_view& identifier, const std::string_view& value)
{
	if (identifier == "version")
	{
		std::optional<std::uint32_t> raw = strutil::TryStringTo<std::uint32_t>(value);
		if (raw.has_value())
			version = *raw;

		return raw.has_value();
	}
	else if (identifier == "workingDirectory")
	{
		workingDirectory = value;
		return true;
	}
	else if (identifier == "buildDirectory")
	{
		buildDirectory = value;
		return true;
	}

	return false;
}

EditorProject::UncheckedFile EditorProject::ProcessData(const std::string_view& data)
{
	UncheckedFile ret{};

	for (size_t i = 0; i != std::string_view::npos && i < data.size(); i = data.find("\r\n", i))
	{
		if (i != 0) // skip over the \r\n
			i += 2;

		if (i == data.size())
			break;

		size_t endLine = std::min(data.find("\r\n", i), data.size()); // for loop has to find this endline twice but oh well
		std::string_view line = data.substr(i, endLine - i);

		size_t divider = line.find('=');
		std::string_view identifier = line.substr(0, divider);
		std::string_view value = line.substr(divider + 1);

		bool success = ret.AssignValueToIdentifier(identifier, value);
		if (!success)
			Console::WriteLine("Failed to assign value \"{}\" to identifier \"{}\"", Console::Severity::Error, value, identifier);
	}

	return ret;
}

const fs::path& EditorProject::GetWorkingDirectory() const
{
	return storage.root;
}

std::string_view EditorProject::GetProjectName() const
{
	return storage.name;
}

bool EditorProject::IsValid() const
{
	return storage.exists;
}