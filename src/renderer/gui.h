#pragma once
#include <string>
#include <vector>

class Object;
class Window;
class Transform;
class RigidBody;
class Profiler;
class Camera;
class Renderer;
struct Mesh;

class GUI
{
public:
	static void AutomaticallyCreateWindows(bool setting);
	static void CreateGUIWindow(const char* name);
	static void EndGUIWindow();

	static void ShowDebugWindow(Profiler* profiler);
	static void ShowDevConsole();
	static void ShowDevConsoleContent();
	static void ShowObjectTable(const std::vector<Object*>& objects);
	static void ShowFPS(int fps);
	static void ShowGraph(const std::vector<uint64_t>& buffer, const char* label, float max = 100.0f);
	static void ShowGraph(const std::vector<float>& buffer, const char* label, float max = 100.0f);
	static void ShowPieGraph(std::vector<float>& data, const char* label = nullptr);
	static void ShowChartGraph(size_t item, size_t max, const char* label);
	static void ShowDropdownMenu(std::string* items, size_t size, std::string& currentItem, int& currentIndex, const char* label);

	static void ShowObjectSelectMenu(const std::vector<Object*>& objects, Object*& curr, const char* label);
	
	static void ShowObjectComponents(const std::vector<Object*>& objects, Window* window, int index);
	static void ShowObjectTransform(Transform& transform);
	static void ShowObjectRigidBody(RigidBody& rigidBody);
	static void ShowObjectData(Object* object);
	static void ShowObjectMeshes(Mesh& mesh);

	static void ShowWindowData(Window* window);
	static void ShowCameraData(Camera* camera);

	static void ShowFrameTimeGraph(const std::vector<float>& frameTime, float onePercentLow);
};