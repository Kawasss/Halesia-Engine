#include <regex>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <time.h>
#include <chrono>

#include "core/Console.h"

#include "system/Input.h"
#include "system/CriticalSection.h"

#include "io/IO.h"

#include "interpreter/Lexer.hpp"
#include "interpreter/Parser.hpp"
#include "interpreter/Interpreter.hpp"

std::vector<std::string>                         Console::messages{};
std::unordered_map<std::string, MessageSeverity> Console::messageColorBinding{};

bool Console::isOpen = false;
bool Console::init   = false;

Parser parser;
Interpreter interpreter;

inline void writeline(FunctionBody* body)
{
	std::string text = interpreter.GetStringFromStack(body->parameters[0]);
	Console::WriteLine(text);
}

inline std::string GetTimeAsString()
{
	
	std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::stringstream stream;
	std::tm ltime;

	localtime_s(&ltime, &t);
	stream << '[' << std::put_time(&ltime, "%H:%M:%S") << "] ";
	return stream.str();
}

inline void WriteMessageWithColor(const std::string& message, MessageSeverity severity)
{
	switch (severity)
	{
	case MESSAGE_SEVERITY_DEBUG:
		std::cout << "\x1B[34m" << message << "\033[0m\n";
		break;
	case MESSAGE_SEVERITY_ERROR:
		std::cout << "\x1B[31m" << message << "\033[0m\n";
		break;
	case MESSAGE_SEVERITY_NORMAL:
		std::cout << message << '\n';
		break;
	case MESSAGE_SEVERITY_WARNING:
		std::cout << "\x1B[33m" << message << "\033[0m\n";
		break;
	}
}

void Console::Init()
{
	std::ifstream file("cfg/init.cfg", std::ios::ate | std::ios::binary);
	std::cout << "Initialised console with" << (file.good() ? "" : "out") << " init.cfg" << "\n";
	if (!file.good())
		return;

	interpreter.SetInstructions({ Instruction(INSTRUCTION_TYPE_PUSH_SCOPE) }, {});
	interpreter.Execute();
	InterpretCommand(IO::ReadFile("cfg/init.cfg", true).data());
	init = true;
}

void Console::BindExternFunctions()
{
	interpreter.ConnectExternalFunction(parser.GetOperandByName("writeline").id, &writeline);
}

win32::CriticalSection writingLinesCritSection;
void Console::WriteLine(std::string message, MessageSeverity severity)
{
	win32::CriticalLockGuard guard(writingLinesCritSection);
	message = GetTimeAsString() + message;

	messages.push_back(message);
	messageColorBinding[message] = severity;

	#ifdef _DEBUG
	WriteMessageWithColor(message, severity);
	#endif
}

void Console::InterpretCommand(std::string command)
{
	if (!command.empty() && command.back() != ';') command += ';';
	const std::vector<Lexer::Token> lexTokens = Lexer::LexInput(command);
	parser.SetTokens(lexTokens);
	parser.Parse();

	if (!init)
		BindExternFunctions();

	parser.instructions.erase(parser.instructions.begin(), parser.instructions.begin() + 2);
	interpreter.SetInstructions(parser.instructions, parser.functions);
	parser.ClearInstructions();
	interpreter.Execute();
}

void Console::BindVariableToExternVariable(std::string externalVariable, void* variable)
{
	Operand op = parser.GetOperandByName(externalVariable);
	interpreter.ConnectExternalVariable(op.id, variable, ByteSizeOf(op.type));
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