#include "Console.h"
#include <regex>
#include <iostream>
#include <vector>
#include <mutex>
#include "system/Input.h"



std::vector<std::string> Console::messages{};
std::map<std::string, void*> Console::commandVariables{};
std::map<std::string, MessageSeverity> Console::messageColorBinding{};

bool Console::isOpen = false;

std::mutex writingLinesMutex;
void Console::WriteLine(std::string message, MessageSeverity severity)
{
	std::lock_guard<std::mutex> guard(writingLinesMutex);
	messages.push_back(message);
	messageColorBinding[message] = severity;
}

void Console::InterpretCommand(std::string command)
{
	std::string variableName;

	if (command == "exit")
		exit(0);

	int value = 0;
	for (int i = 0; command.c_str()[i] != '\0'; i++)     // dont know if c_str() is necessary
	{
		if (command.c_str()[i] != ' ')                   // iterate through the string till it finds whitespace
			variableName += command.c_str()[i];
		else											 // when the whitespace has been found, it splits the string into two
		{
			//value = (int)(command.c_str()[i + 1] - '0'); // turn the ascii version of the number into an integer
			std::string seperatedString = command.substr((size_t)i + 1, command.size() - i);
			value = std::stoi(seperatedString);
			if (commandVariables.count(variableName) == 0)
			{
				Console::WriteLine("Failed to find the engine variable \"" + variableName + "\"", MESSAGE_SEVERITY_ERROR);
				return;
			}
			*static_cast<int*>(commandVariables[variableName]) = value;
			WriteLine("Set engine variable \"" + variableName + "\" to " + std::to_string(value));
			return;
		}
	}
}

glm::vec3 Console::GetColorFromMessage(std::string message)
{
	switch (messageColorBinding[message])
	{
	case MESSAGE_SEVERITY_NORMAL:
		return glm::vec3(1);
	case MESSAGE_SEVERITY_WARNING:
		return glm::vec3(1, 1, 0);
	case MESSAGE_SEVERITY_ERROR:
		return glm::vec3(1, 0, 0);
	case MESSAGE_SEVERITY_DEBUG:
		return glm::vec3(.1f, .1f, 1);
	default:
		return glm::vec3(1);
	}
}