module;

#include <Windows.h>

//#define IMGUI_IMPLEMENTATION
#define IMGUI_DEFINE_MATH_OPERATORS
#define IMSPINNER_DEMO
#include <imgui-1.91.7/implot.h>
#include <imgui-1.91.7/imgui.h>
#include <imgui-1.91.7/misc/cpp/imgui_stdlib.h>

#include "renderer/Mesh.h"
#include "renderer/Vulkan.h"

#include "core/Console.h"
#include "core/Object.h"
#include "core/MeshObject.h"
#include "core/Transform.h"

#include <hsl/StackMap.h>

module Renderer.Gui;

import std;

import HalesiaEngine;

import Core.Profiler;
import Core.Rigid3DObject;
import Core.LightObject;
import Core.CameraObject;
import Core.Scene;

import Physics.RigidBody;
import Physics.Shapes;

import System.Input;
import System.Window;

import Renderer;

inline void InputFloat(const std::string& name, float& value, float width)
{
	ImGui::Text(name.c_str());
	ImGui::SameLine();
	ImGui::PushItemWidth(width * 8);
	ImGui::InputFloat(("##" + name).c_str(), &value);
	ImGui::PopItemWidth();
}

inline bool createWindow = true;
void GUI::AutomaticallyCreateWindows(bool setting)
{
	createWindow = setting;
}

void GUI::CreateGUIWindow(const char* name)
{
	ImGui::Begin(name);
}

void GUI::EndGUIWindow()
{
	ImGui::End();
}

void GUI::ShowWindowData(Window* window)
{
	static std::array<std::string, 2> modes = { "WINDOW_MODE_WINDOWED", "WINDOW_MODE_BORDERLESS_WINDOWED" };
	static hsl::StackMap<std::string, Window::Mode, 2> stringToMode = { { "WINDOW_MODE_WINDOWED", Window::Mode::Windowed }, { "WINDOW_MODE_BORDERLESS_WINDOWED", Window::Mode::BorderlessWindowed } };
	static std::string currentMode;
	static int modeIndex = -1;
	static bool lockCursor = false;
	switch (window->GetWindowMode())
	{
	case Window::Mode::Windowed:
		modeIndex = 0;
		break;
	case Window::Mode::BorderlessWindowed:
		modeIndex = 1;
		break;
	}
	currentMode = modes[modeIndex];
	lockCursor = window->CursorIsLocked();

	int x = window->GetX(), y = window->GetY();
	int width = window->GetWidth(), height = window->GetHeight();
	if (createWindow)
		ImGui::Begin("game window");

	ImGui::Text("mode:       ");
	ImGui::SameLine();
	ShowDropdownMenu(modes, currentMode, modeIndex, "##modeSelect");

	ImGui::Text("width:      ");
	ImGui::SameLine();
	bool changedDimensions = ImGui::InputInt("##windowWidth", &width);

	ImGui::Text("height:     ");
	ImGui::SameLine();
	changedDimensions = ImGui::InputInt("##windowHeight", &height);

	ImGui::Text("x:          ");
	ImGui::SameLine();
	bool changedCoord = ImGui::InputInt("##windowx", &x);

	ImGui::Text("y:          ");
	ImGui::SameLine();
	changedCoord = ImGui::InputInt("##windowy", &y);

	ImGui::Text("lock cursor:");
	ImGui::SameLine();
	if (ImGui::Checkbox("##lockcursor", &lockCursor))
	{
		if (!window->CursorIsLocked())
			window->LockCursor();
		else if (window->CursorIsLocked())
			window->UnlockCursor();
		lockCursor = window->CursorIsLocked();
	}

	ImGui::NewLine();

	int relX, relY, absX, absY;
	window->GetRelativeCursorPosition(relX, relY);
	window->GetAbsoluteCursorPosition(absX, absY);
	ImGui::Text(
		"rel. cursor: %i, %i\n"
		"abs. cursor: %i, %i\n"
		, relX, relY, absX, absY
	);

	ImGui::Text(
		"dropped file: %i\n"
		"maximized:    %i\n"
		"resized:      %i\n"
		, (int)window->ContainsDroppedFile(), (int)window->IsMaximized(), (int)window->resized);

	if (changedCoord)
		window->SetXAndY(x, y);
	if (changedDimensions)
		window->SetWidthAndHeight(width, height); // should only be called if the width and / or height has changed
	if (stringToMode[currentMode] != window->GetWindowMode())
		window->SetWindowMode(stringToMode[currentMode]);

	if (createWindow)
		ImGui::End();
}

void GUI::ShowObjectTransform(Transform& transform)
{
	ImGui::Text("position:");
	ImGui::SameLine();
	ImGui::InputFloat3("##pos", glm::value_ptr(transform.position));

	glm::vec3 rot = glm::eulerAngles(transform.rotation);
	rot = glm::degrees(rot);

	ImGui::Text("rotation:");
	ImGui::SameLine();
	ImGui::InputFloat3("##rot", glm::value_ptr(transform.rotation));
	
	rot = glm::radians(rot);

	transform.rotation = glm::quat(rot);

	ImGui::Text("scale:   ");
	ImGui::SameLine();
	ImGui::InputFloat3("##scale", glm::value_ptr(transform.scale));
}

void GUI::ShowDropdownMenu(const std::span<const std::string>& items, std::string& currentItem, int& currentIndex, const char* label)
{
	if (!ImGui::BeginCombo(label, currentItem.c_str()))
		return;

	for (int i = 0; i < items.size(); i++)
	{
		bool isSelected = items[i] == currentItem;
		if (ImGui::Selectable(items[i].c_str(), &isSelected))
		{
			currentItem = items[i];
			currentIndex = i;
		}
		if (isSelected)
			ImGui::SetItemDefaultFocus();
	}
	ImGui::EndCombo();
}

void GUI::ShowDropdownMenu(const std::span<const std::string_view>& items, std::string_view& currentItem, int& currentIndex, const char* label)
{
	if (!ImGui::BeginCombo(label, currentItem.data()))
		return;

	for (int i = 0; i < items.size(); i++)
	{
		bool isSelected = items[i] == currentItem;
		if (ImGui::Selectable(items[i].data(), &isSelected))
		{
			currentItem = items[i];
			currentIndex = i;
		}
		if (isSelected)
			ImGui::SetItemDefaultFocus();
	}
	ImGui::EndCombo();
}

void GUI::ShowDevConsole()
{
	if (!Console::isOpen)
		return;
		
	if (createWindow)
		ImGui::Begin("Dev Console", nullptr, ImGuiWindowFlags_NoCollapse);
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	colors[ImGuiCol_WindowBg] = ImVec4(0.01f, 0.01f, 0.01f, 0.9f);
	colors[ImGuiCol_Border] = ImVec4(0.05f, 0.05f, 0.05f, 1);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.05f, 1);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.05f, 0.05f, 0.05f, 1);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.49f, 0.68f, 1);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.56f, 0.49f, 0.68f, 1);

	style.WindowRounding = 5;
	style.WindowBorderSize = 2;

	ShowDevConsoleContent();

	if (createWindow)
		ImGui::End();
}

void GUI::ShowDevConsoleContent()
{
	Console::LockMessages();
	for (const Console::Message& message : Console::messages)
	{
		Console::Color color = Console::GetColorFromMessage(message);
		ImGui::TextColored(ImVec4(color.r, color.g, color.b, 1), message.text.c_str());
	}
	Console::UnlockMessages();

	std::string result = "";
	ImGui::InputTextWithHint("##input", "Console commands...", &result);

	if (Input::IsKeyPressed(VirtualKey::Return) && !result.empty()) // if enter is pressed place the input value into the optional variable
		Console::InterpretCommand(result);
}

void GUI::ShowFPS(int FPS)
{
	if (createWindow)
		ImGui::Begin("##FPS Counter", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);

	ImGui::SetWindowPos(ImVec2(0, 0/*ImGui::GetWindowSize().y * 0.2f*/));
	ImGui::SetWindowSize(ImVec2(ImGui::GetWindowSize().x * 2, ImGui::GetWindowSize().y));
	std::string text = std::to_string(FPS) + " FPS";
	ImGui::Text(text.c_str());

	if (createWindow)
		ImGui::End();
}

inline void SetImGuiColors()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 5;
	style.WindowBorderSize = 2;

	ImVec4* colors = style.Colors;

	colors[ImGuiCol_WindowBg] = ImVec4(0.01f, 0.01f, 0.01f, 0.9f);
	colors[ImGuiCol_Border] = ImVec4(0.05f, 0.05f, 0.05f, 1);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.05f, 1);
}

void GUI::ShowPieGraph(std::vector<float>& data, const char* label)
{
	if (createWindow)
		ImGui::Begin(label, nullptr, ImGuiWindowFlags_NoScrollbar);
	SetImGuiColors();

	ImPlot::BeginPlot("##Time Per Async Task", ImVec2(-1, 0), ImPlotFlags_Equal | ImPlotFlags_NoMouseText);
	ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
	ImPlot::SetupAxesLimits(0, 1, 0, 1);
	const char* labels[] = { "Main Thread", "Script Thread", "Renderer Thread" };
	ImPlot::PlotPieChart(labels, data.data(), 3, 0.5, 0.5, 0.5, "%.1f", 180);
	ImPlot::EndPlot();
	if (createWindow)
		ImGui::End();
}

void GUI::ShowGraph(const std::vector<uint64_t>& buffer, const char* label, float max)
{
	if (createWindow)
		ImGui::Begin(label, nullptr, ImGuiWindowFlags_NoScrollbar);
	SetImGuiColors();

	std::string uid = '#' + (std::string)label;

	ImPlot::BeginPlot(uid.c_str(), ImVec2(-1, 0), ImPlotFlags_NoFrame);
	ImPlot::SetupAxisLimits(ImAxis_X1, 0, buffer.size());
	ImPlot::SetupAxisLimits(ImAxis_Y1, 0, max);
	ImPlot::SetupAxes("##x", "##y", ImPlotAxisFlags_NoTickLabels);
	ImPlot::PlotLine(label, buffer.data(), (int)buffer.size());
	ImPlot::EndPlot();
	if (createWindow)
		ImGui::End();
}

void GUI::ShowGraph(const std::vector<float>& buffer, const char* label, float max)
{
	if (createWindow)
		ImGui::Begin(label, nullptr, ImGuiWindowFlags_NoScrollbar);
	SetImGuiColors();

	std::string uid = '#' + (std::string)label;

	ImPlot::BeginPlot(uid.c_str(), ImVec2(-1, 0), ImPlotFlags_NoInputs | ImPlotFlags_NoFrame);
	ImPlot::SetupAxisLimits(ImAxis_X1, 0, buffer.size());
	ImPlot::SetupAxisLimits(ImAxis_Y1, 0, max);
	ImPlot::SetupAxes("##x", "##y", ImPlotAxisFlags_NoTickLabels);
	ImPlot::PlotLine(label, buffer.data(), (int)buffer.size());
	ImPlot::EndPlot();
	if (createWindow)
		ImGui::End();
}

void GUI::ShowChartGraph(size_t item, size_t max, const char* label)
{
	constexpr int CHART_WIDTH = 200, CHART_HEIGHT = 75;

	float relative = (float)item / max;

	if (createWindow)
		ImGui::Begin(label);
	ImGui::BeginChild((uint64_t)label, {CHART_WIDTH, CHART_HEIGHT}, true);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImVec2 pos = ImGui::GetWindowPos();
	ImVec2 endPos = { pos.x + CHART_WIDTH, pos.y + CHART_HEIGHT };
	ImVec2 midPos = { pos.x + CHART_WIDTH * relative, pos.y + CHART_HEIGHT };
	drawList->AddRectFilled(pos, endPos, IM_COL32(43, 43, 43, 255));
	drawList->AddRectFilled(pos, midPos, IM_COL32(100, 100, 200, 255));
	ImGui::Text("%s, %i : %i\nusing %.1f%%", label, item, max, relative * 100);
	ImGui::EndChildFrame();
	if (createWindow)
		ImGui::End();
}

void GUI::ShowCameraData(CameraObject* camera)
{
	
}

void GUI::ShowFrameTimeGraph(const std::vector<float>& frameTime, float onePercentLow)
{
	const std::vector<float> Line60FPS(frameTime.size(), 1000 / 60); // not that efficient since the vector is made every frame
	const std::vector<float> Line30FPS(frameTime.size(), 1000 / 30);
	const std::vector<float> line1PLow(frameTime.size(), onePercentLow);

	if (createWindow)
		ImGui::Begin("frameTime", nullptr, ImGuiWindowFlags_NoScrollbar);
	SetImGuiColors();

	ImPlot::BeginPlot("frame time (ms)", ImVec2(-1, 0), ImPlotFlags_NoInputs | ImPlotFlags_NoFrame);
	ImPlot::SetupAxisLimits(ImAxis_X1, 0, frameTime.size());
	ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 40);
	ImPlot::SetupAxes("##x", "##y", ImPlotAxisFlags_NoTickLabels);
	ImPlot::PlotLine("##frametime", frameTime.data(), (int)frameTime.size());
	ImPlot::PlotLine("##1PLow", line1PLow.data(), (int)line1PLow.size());
	ImPlot::PlotLine("##60fps", Line60FPS.data(), (int)Line60FPS.size());
	ImPlot::PlotLine("##30fps", Line30FPS.data(), (int)Line30FPS.size());
	ImPlot::EndPlot();

	ImGui::Text("current frame time: %.3f ms  1%% low frame time: %.3f ms  1%% low as fps: %.1f", frameTime.back(), onePercentLow, 1000 / onePercentLow);

	if (createWindow)
		ImGui::End();
}

void GUI::ShowDebugWindow(Profiler* profiler)
{	
	EngineCore& core = HalesiaEngine::GetInstance()->GetEngineCore();

	ImGui::SetNextWindowSize({ 0, 0 });
	CreateGUIWindow("debug");
	SetImGuiColors();
	createWindow = false;
	if (ImGui::CollapsingHeader("renderer"))
	{
		ShowFrameTimeGraph(profiler->GetFrameTime(), profiler->Get1PercentLowFrameTime());
		ShowChartGraph(Renderer::g_indexBuffer.GetSize() / 1024ULL, Renderer::g_indexBuffer.GetMaxSize() / 1024ULL, "index (kb)");
		ImGui::SameLine();
		ShowChartGraph(Renderer::g_vertexBuffer.GetSize() / 1024ULL, Renderer::g_vertexBuffer.GetMaxSize() / 1024ULL, "vertex (kb)");
		ImGui::SameLine();
		ShowChartGraph(Renderer::g_defaultVertexBuffer.GetSize() / 1024ULL, Renderer::g_defaultVertexBuffer.GetMaxSize() / 1024ULL, "d_vertex (kb)");

		float scale = Renderer::internalScale;
		ImGui::Text("Internal resolution:");
		ImGui::SameLine();
		ImGui::SliderFloat("##resslider", &scale, 0.01f, 2, "%.2f");
		if (scale != Renderer::internalScale)
			core.renderer->SetInternalResolutionScale(scale);

		ImGui::Text("received objects: %i  renderered objects: %i", core.renderer->receivedObjects, core.renderer->renderedObjects);

		Vulkan::Context context = Vulkan::GetContext();
		VkPhysicalDeviceProperties properties = context.physicalDevice.Properties();

		size_t total   = Vulkan::allocatedMemory;
		size_t managed = vvm::GetAllocatedByteCount();

		ImGui::Text("GPU: %s   VRAM: %i MB", properties.deviceName, context.physicalDevice.VRAM() / 1024ULL / 1024ULL);
		ImGui::Text("Allocated VRAM: %I64u MB", total / 1024ULL / 1024ULL);

		ImGui::Text("managed:   %I64u MB over %I64u blocks", managed / 1024ULL / 1024ULL, vvm::GetBlockCount());
		ImGui::Text("unmanaged: %I64u MB", (total - managed) / 1024ULL / 1024ULL);
		
		std::map<std::string, uint64_t> timestamps = core.renderer->GetTimestamps();

		std::vector<float> values(timestamps.size());
		std::vector<const char*> labels(timestamps.size() + 1);

		values.resize(timestamps.size());
		labels.resize(timestamps.size() + 1);

		int i = 0;
		for (auto it = timestamps.begin(); it != timestamps.end(); it++)
		{
			labels[i] = it->first.c_str();
			values[i] = it->second * 0.000001f; // nanoseconds to milliseconds
			i++;
		}
		labels.back() = "none";

		float sum = 0.0f;
		for (float val : values)
			sum += val;
		if (sum < 1.0f)
			values.push_back(1.0f - sum);

		/*ImPlot::BeginPlot("timestap queries", ImVec2(-1, 0), ImPlotFlags_NoInputs | ImPlotFlags_NoFrame);
		ImPlot::PlotPieChart(labels.data(), values.data(), values.size(), 0.5, 0.5, 0.5, "%.3f", 180);
		ImPlot::EndPlot();*/
	}

	if (ImGui::CollapsingHeader("scene"))
	{
		ImGui::Text("object count: %i", core.scene->allObjects.size());
		ShowCameraData(core.scene->camera);
	}

	if (ImGui::CollapsingHeader("window"))
	{
		ShowWindowData(core.window);
	}

	if (ImGui::CollapsingHeader("system"))
	{
		ShowGraph(profiler->GetCPU(), "CPU usage %");
		ShowGraph(profiler->GetGPU(), "GPU usage %");
		ShowGraph(profiler->GetRAM(), "RAM usage (MB)", profiler->GetRAM()[0] * 1.2f);
	}

	createWindow = true;
	EndGUIWindow();
}