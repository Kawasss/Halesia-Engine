#pragma once
#include <string>
#include <optional>
#include <vector>

class Object;
class Win32Window;
class Transform;
class RigidBody;

class GUI
{
public:
	static std::optional<std::string> ShowDevConsole();
	static void ShowMainMenuBar(bool& showObjMeta, bool& ramGraph, bool& cpuGraph, bool& gpuGraph);
	static void ShowSceneGraph(const std::vector<Object*>& objects, Win32Window* window);
	static void ShowObjectTable(const std::vector<Object*>& objects);
	static void ShowFPS(int fps);
	static void ShowGraph(const std::vector<uint64_t>& buffer, const char* label);
	static void ShowGraph(const std::vector<float>& buffer, const char* label);
	static void ShowPieGraph(std::vector<float>& data, const char* label = nullptr);
	static void ShowDropdownMenu(std::vector<std::string>& items, std::string& currentItem, int& currentIndex, const char* label);
	
	static void ShowObjectComponents(const std::vector<Object*>& objects, Win32Window* window);
	static void ShowObjectTransform(Transform& transform);
	static void ShowObjectRigidBody(RigidBody& rigidBody);
};