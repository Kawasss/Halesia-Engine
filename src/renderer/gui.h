#pragma once
#include <string>
#include <vector>

class Object;
class Win32Window;
class Transform;
class RigidBody;
class Profiler;
class Camera;
struct Mesh;

class GUI
{
public:
	static void AutomaticallyCreateWindows(bool setting);
	static void CreateGUIWindow(const char* name);
	static void EndGUIWindow();

	static void ShowDebugWindow(Profiler* profiler);
	static void ShowDevConsole();
	static void ShowMainMenuBar(bool& showWindowData, bool& showObjMeta, bool& ramGraph, bool& cpuGraph, bool& gpuGraph);
	static void ShowSceneGraph(const std::vector<Object*>& objects, Win32Window* window);
	static void ShowObjectTable(const std::vector<Object*>& objects);
	static void ShowFPS(int fps);
	static void ShowGraph(const std::vector<uint64_t>& buffer, const char* label, float max = 100.0f);
	static void ShowGraph(const std::vector<float>& buffer, const char* label, float max = 100.0f);
	static void ShowPieGraph(std::vector<float>& data, const char* label = nullptr);
	static void ShowChartGraph(size_t item, size_t max, const char* label);
	static void ShowDropdownMenu(std::vector<std::string>& items, std::string& currentItem, int& currentIndex, const char* label);
	
	static void ShowObjectComponents(const std::vector<Object*>& objects, Win32Window* window);
	static void ShowObjectTransform(Transform& transform);
	static void ShowObjectRigidBody(RigidBody& rigidBody);
	static void ShowObjectData(Object* object);
	static void ShowObjectMeshes(std::vector<Mesh>& mesh);

	static void ShowWindowData(Win32Window* window);
	static void ShowCameraData(Camera* camera);

	static void ShowFrameTimeGraph(const std::vector<float>& frameTime, float onePercentLow);
};