#pragma once
#include <string>
#include <string_view>

/// <summary>
/// a set of functions that wraps around system-specific API calls
/// </summary>
namespace sys
{
	/// <summary>
	/// gets an environment variable.
	/// </summary>
	/// <param name="name">the name of environment variable.</param>
	/// <returns>the value of the environment variable.</returns>
	extern std::string GetEnvVariable(const std::string_view& name);

	/// <summary>
	/// starts a process.
	/// </summary>
	/// <param name="name">the name / path of the process to start.</param>
	/// <param name="args">the command line arguments provided to the process.</param>
	/// <param name="waitForCompletion">if true, waits for the completion of the process.</param>
	/// <returns>if 'waitForCompletion' is true, then it returns true if the process' exit value is 0, else false. If 'waitForCompletion' is false, it will always return true.</returns>
	extern bool StartProcess(const std::string_view& name, std::string args, bool waitForCompletion = true);

	/// <summary>
	/// Opens a file with its assosciated application, i.e. opening a .png opens the photo app with that image loaded.
	/// </summary>
	/// <param name="file">the file to open the default application with.</param>
	/// <returns>false if the file cannot be opened, otherwise true.</returns>
	extern bool OpenFile(const std::string_view& file);
}