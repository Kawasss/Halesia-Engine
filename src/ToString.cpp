#include "physics/RigidBody.h"
#include "physics/Shapes.h"

#include "HalesiaEngine.h"
#include "core/Console.h"
#include "core/Object.h"

#include "renderer/Light.h"
#include "renderer/RenderPipeline.h"

#include "io/FileFormat.h"

std::string_view RenderModeToString(RenderMode mode)
{
	switch (mode)
	{
	case RenderMode::DontCare:           return "DontCare";
	case RenderMode::Albedo:             return "Albedo";
	case RenderMode::Normal:             return "Normal";
	case RenderMode::Metallic:           return "Metallic";
	case RenderMode::Roughness:          return "Roughness";
	case RenderMode::AmbientOcclusion:   return "AmbientOcclusion";
	case RenderMode::Polygon:            return "Polygon";
	case RenderMode::UV:                 return "UV";
	case RenderMode::GlobalIllumination: return "GlobalIllumation";
	}
	return "Unknown";
}

std::string_view Object::InheritTypeToString(InheritType type)
{
	switch (type)
	{
	case InheritType::Base:    return "Base";
	case InheritType::Light:   return "Light";
	case InheritType::Mesh:    return "Mesh";
	case InheritType::Rigid3D: return "Rigid3D";
	case InheritType::Script:  return "Script";
	}
	return "Unknown";
}

Object::InheritType Object::StringToInheritType(const std::string_view& str)
{
	if (str == "Base")    return InheritType::Base;
	if (str == "Light")   return InheritType::Light;
	if (str == "Mesh")    return InheritType::Mesh;
	if (str == "Rigid3D") return InheritType::Rigid3D;
	return InheritType::TypeCount; // fallback value
}

Light::Type Light::StringToType(const std::string_view& str)
{
	if (str == "Directional") return Type::Directional;
	if (str == "Point")       return Type::Point;
	if (str == "Spot")        return Type::Spot;
	return Type::Spot;
}

std::string_view Light::TypeToString(Type type)
{
	switch (type)
	{
	case Type::Directional: return "Directional";
	case Type::Point:       return "Point";
	case Type::Spot:        return "Spot";
	}
	return "Unknown";
}

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

std::string HalesiaEngine::ExitCodeToString(HalesiaEngine::ExitCode exitCode)
{
	switch (exitCode)
	{
	case HalesiaEngine::ExitCode::Success:          return "HalesiaEngine::ExitCode::Success";
	case HalesiaEngine::ExitCode::Exception:        return "HalesiaEngine::ExitCode::Exception";
	case HalesiaEngine::ExitCode::UnknownException: return "HalesiaEngine::ExitCode::UnknownException";
	}
	return "HalesiaEngine::ExitCode::Unknown";
}

std::string RigidBody::TypeToString(Type type)
{
	switch (type)
	{
	case Type::Dynamic:   return "RigidBody::Type::Dynamic";
	case Type::Kinematic: return "RigidBody::Type::Kinematic";
	case Type::Static:    return "RigidBody::Type::Static";
	case Type::None:      return "RigidBody::Type::None";
	}
	return "RigidBody::Type::Unknown";
}

std::string Shape::TypeToString(Shape::Type type)
{
	switch (type)
	{
	case Shape::Type::Box:     return "SHAPE_TYPE_BOX";
	case Shape::Type::Capsule: return "SHAPE_TYPE_CAPSULE";
	case Shape::Type::None:    return "SHAPE_TYPE_NONE";
	case Shape::Type::Plane:   return "SHAPE_TYPE_PLANE";
	case Shape::Type::Sphere:  return "SHAPE_TYPE_SPHERE";
	}
	return "SHAPE_TYPE_UNKNOWN";
}

std::string Console::SeverityToString(Severity severity)
{
	switch (severity)
	{
	case Severity::Normal:  return "Console::Severity::Normal";
	case Severity::Warning: return "Console::Severity::Warning";
	case Severity::Error:   return "Console::Severity::Error";
	case Severity::Debug:   return "Console::Severity::Debug";
	}
	return "Console::Severity::Unknown";
}

std::string Console::VariableAccessToString(Access access)
{
	switch (access)
	{
	case Access::ReadOnly:  return "Console::Access::ReadOnly";
	case Access::ReadWrite: return "Console::Access::ReadWrite";
	case Access::WriteOnly: return "Console::Access::WriteOnly";
	}
	return "Console::Access::Unknown";
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