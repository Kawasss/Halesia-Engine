#pragma once
#include <string>
#include <vector>

class FileDialog
{
public:
	struct Filter
	{
		std::string description;
		std::string fileType; // formatted like "*.<file_type>;..."
	};

	// the user can select multiple items
	static std::vector<std::string> RequestFiles(const Filter& filter,   const std::string& start = "");
	static std::vector<std::string> RequestFolders(const Filter& filter, const std::string& start = "");

	// the user can select one (1) item
	static std::string RequestFile(const Filter& filter,   const std::string& start = "");
	static std::string RequestFolder(const Filter& filter, const std::string& start = "");
};