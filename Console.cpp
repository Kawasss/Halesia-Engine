#include "Console.h"
#include <regex>
#include <iostream>
#include <vector>

std::vector<std::string> Console::messages{};
std::map <std::string, void*> Console::commandVariables{};

void Console::WriteLine(std::string message)
{
	messages.push_back(message);
}

void Console::InterpretCommand(std::string command)
{
	std::string variableName;

	if (command == "exit")
		exit(0);

	bool value = false;
	for (int i = 0; command.c_str()[i] != '\0'; i++)     // dont know if c_str() is necessary
	{
		if (command.c_str()[i] != ' ')                   // iterate through the string till it finds whitespace
			variableName += command.c_str()[i];
		else											 // when the whitespace has been found, it splits the string into two
		{
			value = (int)(command.c_str()[i + 1] - '0'); // turn the ascii version of the number into an integer
			if (commandVariables.count(variableName) == 0)
			{
				Console::WriteLine("!! Failed to find the engine variable \"" + variableName + "\"");
				return;
			}
			*static_cast<int*>(commandVariables[variableName]) = value;
			WriteLine("Set engine variable \"" + variableName + "\" to " + std::to_string(value));
			return;
		}
	}
}