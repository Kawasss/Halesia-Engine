#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <map>

#include "core/Console.h"

#include "system/CriticalSection.h"

import StrUtil;

std::vector<Console::Message> Console::messages;

std::map<std::string, float*> floatCVars;
std::map<std::string, int*>   intCVars;
std::map<std::string, bool*>  boolCVars;

bool Console::isOpen = false;

win32::CriticalSection writingLinesCritSection;

inline std::string GetTimeAsString()
{
	std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::stringstream stream;
	std::tm ltime;

	localtime_s(&ltime, &t);
	stream << '[' << std::put_time(&ltime, "%H:%M:%S") << "] ";
	return stream.str();
}

inline void WriteMessageWithColor(const std::string& message, Console::Severity severity)
{
	switch (severity)
	{
	case Console::Severity::Debug:
		std::cout << "\x1B[34m" << message << "\033[0m\n";
		break;
	case Console::Severity::Error:
		std::cout << "\x1B[31m" << message << "\033[0m\n";
		break;
	case Console::Severity::Normal:
		std::cout << message << '\n';
		break;
	case Console::Severity::Warning:
		std::cout << "\x1B[33m" << message << "\033[0m\n";
		break;
	}
}

inline bool WriteValueToName(const std::string& name, float val)
{
	auto floatIt = floatCVars.find(name);
	if (floatIt != floatCVars.end())
	{
		*floatIt->second = val;
		return true;
	}
	auto intIt = intCVars.find(name);
	if (intIt != intCVars.end())
	{
		*intIt->second = static_cast<int>(val);
		return true;
	}
	auto boolIt = boolCVars.find(name);
	if (boolIt != boolCVars.end())
	{
		*boolIt->second = static_cast<bool>(val);
		return true;
	}
	return false;
}

void Console::AddCVariable(const std::string& name, DataType type, void* ptr)
{
	if (type == DataType::None)
	{
		WriteLine("Could not add cvar", Severity::Error);
		return;
	}

	switch (type)
	{
	case DataType::Float:
		floatCVars.emplace(name, reinterpret_cast<float*>(ptr));
		break;
	case DataType::Int:
		intCVars.emplace(name, reinterpret_cast<int*>(ptr));
		break;
	case DataType::Bool:
		boolCVars.emplace(name, reinterpret_cast<bool*>(ptr));
		break;
	}
}

void Console::LockMessages()
{
	writingLinesCritSection.Lock();
}

void Console::UnlockMessages()
{
	writingLinesCritSection.Unlock();
}

void Console::Init()
{

}

void Console::WriteLine(std::string message, Console::Severity severity)
{
	win32::CriticalLockGuard guard(writingLinesCritSection);
	message = GetTimeAsString() + message;

	messages.emplace_back(message, severity);

	#ifdef _DEBUG
	WriteMessageWithColor(message, severity);
	#endif
}

void Console::InterpretCommand(std::string_view command)
{
	size_t separator = command.find(' ');
	if (separator == std::string::npos)
	{
		WriteLine("Parsing error", Severity::Error);
		return;
	}

	std::string name = command.data();
	name.erase(separator);

	std::string_view value = command.substr(separator + 1);

	float underlying = 0.0f;

	std::optional<float> optVal = strutil::TryStringTo<float>(value);

	if (!optVal.has_value())
	{
		WriteError("Failed to interpret value \"{}\"", value);
		return;
	}

	if (!WriteValueToName(name, underlying))
		WriteLine("Failed to find cvar", Severity::Error);
	else
		WriteLine("set value of cvar", Severity::Debug);
}

Console::Color Console::GetColorFromMessage(const Message& message)
{
	switch (message.severity)
	{
	case Console::Severity::Normal:
		return Color(1);
	case Console::Severity::Warning:
		return Color(1, 1, 0);
	case Console::Severity::Error:
		return Color(1, 0, 0);
	case Console::Severity::Debug:
		return Color(.1f, .1f, 1);
	default:
		return Color(1);
	}
}