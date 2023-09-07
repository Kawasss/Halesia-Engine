#pragma once
#include <string>
#include <vector>
#include <map>

class Console
{
public:
	static std::vector<std::string> messages;
	static std::map<std::string, void*> commandVariables;
	static void WriteLine(std::string message);
	static void InterpretCommand(std::string command);
};