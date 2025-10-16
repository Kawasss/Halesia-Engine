#pragma once
#include <string_view>
#include <string>
#include <vector>
#include <array>
#include <span>

class Object;
class Window;
class Transform;
class RigidBody;
class Profiler;
class CameraObject;
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
	static void ShowDropdownMenu(const std::span<const std::string>& items, std::string& currentItem, int& currentIndex, const char* label);
	static void ShowDropdownMenu(const std::span<const std::string_view>& items, std::string_view& currentItem, int& currentIndex, const char* label);

	static void ShowObjectTransform(Transform& transform);

	static void ShowWindowData(Window* window);
	static void ShowCameraData(CameraObject* camera);

	static void ShowFrameTimeGraph(const std::vector<float>& frameTime, float onePercentLow);

	static void ShowInputVector(glm::vec3& val, const std::array<std::string_view, 3>& labels);
	static void ShowInputVector(glm::vec2& val, const std::array<std::string_view, 2>& labels);
};