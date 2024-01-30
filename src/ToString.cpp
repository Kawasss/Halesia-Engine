#include "physics/RigidBody.h"
#include "physics/Shapes.h"

#include "HalesiaEngine.h"
#include "core/Console.h"
#include "core/Object.h"

std::string HalesiaExitCodeToString(HalesiaExitCode exitCode)
{
	switch (exitCode)
	{
	case HALESIA_EXIT_CODE_SUCESS:            return "HALESIA_EXIT_CODE_SUCESS";
	case HALESIA_EXIT_CODE_EXCEPTION:         return "HALESIA_EXIT_CODE_EXCEPTION";
	case HALESIA_EXIT_CODE_UNKNOWN_EXCEPTION: return "HALESIA_EXIT_CODE_UNKNOWN_EXCEPTION";
	}
	return "";
}

std::string RigidBodyTypeToString(RigidBodyType type)
{
	switch (type)
	{
	case RIGID_BODY_DYNAMIC: return "RIGID_BODY_DYNAMIC";
	case RIGID_BODY_STATIC:  return "RIGID_BODY_STATIC";
	case RIGID_BODY_NONE:    return "RIGID_BODY_NONE";
	}
	return "";
}

std::string ShapeTypeToString(ShapeType type)
{
	switch (type)
	{
	case SHAPE_TYPE_BOX:     return "SHAPE_TYPE_BOX";
	case SHAPE_TYPE_CAPSULE: return "SHAPE_TYPE_CAPSULE";
	case SHAPE_TYPE_NONE:    return "SHAPE_TYPE_NONE";
	case SHAPE_TYPE_PLANE:   return "SHAPE_TYPE_PLANE";
	case SHAPE_TYPE_SPHERE:  return "SHAPE_TYPE_SPHERE";
	}
	return "";
}

std::string Console::TokenToString(Token token)
{
	switch (token)
	{
	case LEXER_TOKEN_IDENTIFIER: return "LEXER_TOKEN_IDENTIFIER";
	case LEXER_TOKEN_KEYWORD:    return "LEXER_TOKEN_KEYWORD";
	case LEXER_TOKEN_LITERAL:    return "LEXER_TOKEN_LITERAL";
	case LEXER_TOKEN_OPERATOR:   return "LEXER_TOKEN_OPERATOR";
	case LEXER_TOKEN_SEPERATOR:  return "LEXER_TOKEN_SEPERATOR";
	}
	return "";
}

std::string Console::variableTypeToString(VariableType type)
{
	switch (type)
	{
	case VARIABLE_TYPE_BOOL:   return "VARIABLE_TYPE_BOOL";
	case VARIABLE_TYPE_FLOAT:  return "VARIABLE_TYPE_FLOAT";
	case VARIABLE_TYPE_INT:    return "VARIABLE_TYPE_INT";
	case VARIABLE_TYPE_STRING: return "VARIABLE_TYPE_STRING";
	}
	return "";
}

std::string MessageSeverityToString(MessageSeverity severity)
{
	switch (severity)
	{
	case MESSAGE_SEVERITY_NORMAL:  return "MESSAGE_SEVERITY_NORMAL";
	case MESSAGE_SEVERITY_WARNING: return "MESSAGE_SEVERITY_WARNING";
	case MESSAGE_SEVERITY_ERROR:   return "MESSAGE_SEVERITY_ERROR";
	case MESSAGE_SEVERITY_DEBUG:   return "MESSAGE_SEVERITY_DEBUG";
	}
	return "";
}

std::string ConsoleVariableAccessToString(ConsoleVariableAccess access)
{
	switch (access)
	{
	case CONSOLE_ACCESS_READ_ONLY:  return "CONSOLE_ACCESS_READ_ONLY";
	case CONSOLE_ACCESS_READ_WRITE: return "CONSOLE_ACCESS_READ_WRITE";
	case CONSOLE_ACCESS_WRITE_ONLY: return "CONSOLE_ACCESS_WRITE_ONLY";
	}
	return "";
}

std::string Object::ObjectStateToString(ObjectState state)
{
	switch (state)
	{
	case OBJECT_STATE_DISABLED:  return "OBJECT_STATE_DISABLED";
	case OBJECT_STATE_INVISIBLE: return "OBJECT_STATE_INVISIBLE";
	case OBJECT_STATE_VISIBLE:   return "OBJECT_STATE_VISIBLE";
	}
	return "";
}