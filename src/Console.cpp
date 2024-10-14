#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

#include "core/Console.h"

#include "system/CriticalSection.h"

#include "io/IO.h"

#include <interpreter/Lexer.hpp>
#include <interpreter/Parser.hpp>
#include <interpreter/Interpreter.hpp>

std::vector<Console::Message> Console::messages{};

bool Console::isOpen = false;
bool Console::init   = false;

Parser parser;
Interpreter interpreter;

win32::CriticalSection writingLinesCritSection;

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

void Console::WriteLine(std::string message, Console::Severity severity)
{
	win32::CriticalLockGuard guard(writingLinesCritSection);
	message = GetTimeAsString() + message;

	messages.push_back({ message, severity });

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