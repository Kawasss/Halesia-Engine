#include "physics/RigidBody.h"
#include "physics/Shapes.h"

#include "HalesiaEngine.h"
#include "core/Console.h"
#include "core/Object.h"

#include "io/FileFormat.h"

const char* NodeTypeToString(NodeType type)
{
	switch (type)
	{
	case NODE_TYPE_NONE:      return "NODE_TYPE_NONE";
	case NODE_TYPE_OBJECT:    return "NODE_TYPE_OBJECT";
	case NODE_TYPE_NAME:      return "NODE_TYPE_NAME";
	case NODE_TYPE_MESH:      return "NODE_TYPE_MESH";
	case NODE_TYPE_RIGIDBODY: return "NODE_TYPE_RIGIDBODY";
	case NODE_TYPE_TRANSFORM: return "NODE_TYPE_TRANSFORM";
	case NODE_TYPE_CAMERA:    return "NODE_TYPE_CAMERA";
	case NODE_TYPE_ARRAY:     return "NODE_TYPE_ARRAY";
	case NODE_TYPE_TEXTURE:   return "NODE_TYPE_TEXTURE";
	case NODE_TYPE_MATERIAL:  return "NODE_TYPE_MATERIAL";
	case NODE_TYPE_METADATA:  return "NODE_TYPE_METADATA";
	case NODE_TYPE_VERTICES:  return "NODE_TYPE_VERTICES";
	case NODE_TYPE_INDICES:   return "NODE_TYPE_INDICES";
	}
	return "NODE_TYPE_UNKNOWN";
}

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
	case RIGID_BODY_DYNAMIC:   return "RIGID_BODY_DYNAMIC";
	case RIGID_BODY_KINEMATIC: return "RIGID_BODY_KINEMATIC";
	case RIGID_BODY_STATIC:    return "RIGID_BODY_STATIC";
	case RIGID_BODY_NONE:      return "RIGID_BODY_NONE";
	}
	return "RIGID_BODY_UNKNOWN";
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
	return "SHAPE_TYPE_UNKNOWN";
}

std::string Console::SeverityToString(Severity severity)
{
	switch (severity)
	{
	case Severity::Normal:  return "MESSAGE_SEVERITY_NORMAL";
	case Severity::Warning: return "MESSAGE_SEVERITY_WARNING";
	case Severity::Error:   return "MESSAGE_SEVERITY_ERROR";
	case Severity::Debug:   return "MESSAGE_SEVERITY_DEBUG";
	}
	return "MESSAGE_SEVERITY_UNKNOWN";
}

std::string Console::VariableAccessToString(Access access)
{
	switch (access)
	{
	case Access::ReadOnly:  return "CONSOLE_ACCESS_READ_ONLY";
	case Access::ReadWrite: return "CONSOLE_ACCESS_READ_WRITE";
	case Access::WriteOnly: return "CONSOLE_ACCESS_WRITE_ONLY";
	}
	return "CONSOLE_ACCESS_UNKNOWN";
}

std::string ObjectStateToString(ObjectState state)
{
	switch (state)
	{
	case OBJECT_STATE_DISABLED:  return "OBJECT_STATE_DISABLED";
	case OBJECT_STATE_INVISIBLE: return "OBJECT_STATE_INVISIBLE";
	case OBJECT_STATE_VISIBLE:   return "OBJECT_STATE_VISIBLE";
	}
	return "OBJECT_STATE_UNKNOWN";
}