#pragma once
#include <string>
#include <vector>
#include <map>
#include "glm.h"

enum MessageSeverity
{
	MESSAGE_SEVERITY_NORMAL,  // white
	MESSAGE_SEVERITY_WARNING, // yellow
	MESSAGE_SEVERITY_ERROR,   // red
	MESSAGE_SEVERITY_DEBUG    // blue
};

inline std::string MessageSeverityToString(MessageSeverity severity)
{
	switch (severity)
	{
	case MESSAGE_SEVERITY_NORMAL:
		return "MESSAGE_SEVERITY_NORMAL";
	case MESSAGE_SEVERITY_WARNING:
		return "MESSAGE_SEVERITY_WARNING";
	case MESSAGE_SEVERITY_ERROR:
		return "MESSAGE_SEVERITY_ERROR";
	case MESSAGE_SEVERITY_DEBUG:
		return "MESSAGE_SEVERITY_DEBUG";
	}
	return "";
}

class Console
{
public:
	static std::vector<std::string> messages;
	static std::map<std::string, void*> commandVariables;
	static void WriteLine(std::string message, MessageSeverity severity = MESSAGE_SEVERITY_NORMAL);
	static void InterpretCommand(std::string command = "");
	static glm::vec3 GetColorFromMessage(std::string message);

	static bool isOpen;

private:
	static std::map<std::string, MessageSeverity> messageColorBinding;
};