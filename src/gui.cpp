#define IMGUI_IMPLEMENTATION
#define IMGUI_DEFINE_MATH_OPERATORS
#define IMSPINNER_DEMO
#include "implot.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui-1.89.8/imgui-1.89.8/imspinner.h"

#include "renderer/RayTracing.h"
#include "renderer/Mesh.h"
#include "renderer/Renderer.h"
#include "renderer/gui.h"

#include "system/Input.h"
#include "system/Window.h"

#include "physics/RigidBody.h"
#include "physics/Shapes.h"

#include "Console.h"
#include "Object.h"
#include "Transform.h"

inline void ShowInputVector(glm::vec3& vector, std::vector<const char*> labels)
{
	if (labels.size() < 3) return;
	float width = ImGui::CalcTextSize("w").x;
	ImGui::PushItemWidth(width * 8);
	ImGui::InputFloat(labels[0], &vector.x);
	ImGui::SameLine();
	ImGui::InputFloat(labels[1], &vector.y);
	ImGui::SameLine();
	ImGui::InputFloat(labels[2], &vector.z);
	ImGui::PopItemWidth();
}

void GUI::ShowWindowData(Win32Window* window)
{
	static std::vector<std::string> modes = { "WINDOW_MODE_WINDOWED", "WINDOW_MODE_BORDERLESS_WINDOWED" };
	static std::unordered_map<std::string, WindowMode> stringToMode = { { "WINDOW_MODE_WINDOWED", WINDOW_MODE_WINDOWED }, { "WINDOW_MODE_BORDERLESS_WINDOWED", WINDOW_MODE_BORDERLESS_WINDOWED } };
	static std::string currentMode;
	static int modeIndex = -1;
	static bool lockCursor = false;
	switch (window->GetWindowMode())
	{
	case WINDOW_MODE_WINDOWED:
		modeIndex = 0;
		break;
	case WINDOW_MODE_BORDERLESS_WINDOWED:
		modeIndex = 1;
		break;
	}
	currentMode = modes[modeIndex];
	lockCursor = window->CursorIsLocked();

	int x = window->GetX(), y = window->GetY();
	int width = window->GetWidth(), height = window->GetHeight();
	ImGui::Begin("game window");

	ImGui::Text("mode:       ");
	ImGui::SameLine();
	ShowDropdownMenu(modes, currentMode, modeIndex, "##modeSelect");

	ImGui::Text("width:      ");
	ImGui::SameLine();
	ImGui::InputInt("##windowWidth", &width);

	ImGui::Text("height:     ");
	ImGui::SameLine();
	ImGui::InputInt("##windowHeight", &height);

	ImGui::Text("x:          ");
	ImGui::SameLine();
	ImGui::InputInt("##windowx", &x);

	ImGui::Text("y:          ");
	ImGui::SameLine();
	ImGui::InputInt("##windowy", &y);

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
	ImGui::Text("rel. cursor: %i, %i", relX, relY);
	ImGui::Text("abs. cursor: %i, %i", absX, absY);

	window->SetXAndY(x, y);
	window->SetWidthAndHeight(width, height); // should only be called if the width and / or height has changed
	if (stringToMode[currentMode] != window->GetWindowMode())
		window->ChangeWindowMode(stringToMode[currentMode]);

	ImGui::End();
}

void GUI::ShowObjectComponents(const std::vector<Object*>& objects, Win32Window* window)
{
	static std::string currentItem = "None";
	static int objectIndex = -1;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.03f, 0.03f, 1));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.03f, 0.03f, 0.03f, 1));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.02f, 0.02f, 1));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 5);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGuiStyle& style = ImGui::GetStyle();
	ImGui::SetNextWindowPos(ImVec2(window->GetWidth() * 7 / 8, ImGui::GetFrameHeight() + style.FramePadding.y));
	ImGui::SetNextWindowSize(ImVec2(window->GetWidth() / 8, window->GetHeight() - ImGui::GetFrameHeight() - style.FramePadding.y));
	ImGui::Begin("components", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

	std::vector<std::string> items; // not the most optimal way
	for (Object* object : objects)
		items.push_back(object->name);
	ShowDropdownMenu(items, currentItem, objectIndex, "##ObjectComponents");

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;
	if (objectIndex != -1 && ImGui::CollapsingHeader("Transform", flags))
		ShowObjectTransform(objects[objectIndex]->transform);

	if (objectIndex != -1 && ImGui::CollapsingHeader("Rigid body", flags))
		ShowObjectRigidBody(objects[objectIndex]->rigid);

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
	ImGui::End();
}

void GUI::ShowObjectRigidBody(RigidBody& rigidBody)
{
	static std::unordered_map<std::string, ShapeType> stringToShape =
	{
		{ "SHAPE_TYPE_BOX", SHAPE_TYPE_BOX },
		{ "SHAPE_TYPE_SPHERE", SHAPE_TYPE_SPHERE },
		{ "SHAPE_TYPE_CAPSULE", SHAPE_TYPE_CAPSULE }
	};
	static std::unordered_map<std::string, RigidBodyType> stringToRigid =
	{
		{ "RIGID_BODY_STATIC", RIGID_BODY_STATIC},
		{ "RIGID_BODY_DYNAMIC", RIGID_BODY_DYNAMIC }
	};

	static int rigidIndex = -1;
	static int shapeIndex = -1;
	static std::vector<std::string> allShapeTypes = { "SHAPE_TYPE_BOX", "SHAPE_TYPE_SPHERE", "SHAPE_TYPE_CAPSULE" };
	static std::vector<std::string> allRigidTypes = { "RIGID_BODY_DYNAMIC", "RIGID_BODY_STATIC" };
	std::string currentRigid = RigidBodyTypeToString(rigidBody.type);
	std::string currentShape = ShapeTypeToString(rigidBody.shape.type);
	glm::vec3 holderExtents = rigidBody.shape.data;

	ImGui::Text("type:   ");
	ImGui::SameLine();
	ShowDropdownMenu(allRigidTypes, currentRigid, rigidIndex, "##RigidType");

	ImGui::Text("shape:  ");
	ImGui::SameLine();
	ShowDropdownMenu(allShapeTypes, currentShape, shapeIndex, "##rigidShapeMenu");

	switch (rigidBody.shape.type)
	{
	case SHAPE_TYPE_BOX:
		ImGui::Text("Extents:");
		ImGui::SameLine();
		ShowInputVector(holderExtents, { "##extentsx", "##extentsy", "##extentsz" });
		break;
	case SHAPE_TYPE_CAPSULE:
		ImGui::Text("Height: ");
		ImGui::SameLine();
		ImGui::InputFloat("##height", &holderExtents.y);
		ImGui::Text("radius: ");
		ImGui::SameLine();
		if (ImGui::InputFloat("##radius", &holderExtents.x))
			holderExtents.y += holderExtents.x; // add radius on top of the half height
		break;
	case SHAPE_TYPE_SPHERE:
		ImGui::Text("radius: ");
		ImGui::SameLine();
		ImGui::InputFloat("##radius", &holderExtents.x);
		break;
	}
	// update the rigidbody shape if it has been changed via the gui
	if (stringToShape[currentShape] != rigidBody.shape.type || holderExtents != rigidBody.shape.data)
	{
		Shape newShape = Shape::GetShapeFromType(stringToShape[currentShape], holderExtents);
		rigidBody.ChangeShape(newShape);
	}
	if (stringToRigid[currentRigid] != rigidBody.type)
	{
		rigidBody.Destroy();
		Object* obj = (Object*)rigidBody.GetUserData();
		rigidBody = RigidBody(rigidBody.shape, stringToRigid[currentRigid], obj->transform.position, obj->transform.rotation);
		rigidBody.SetUserData(obj);
	}
}

void GUI::ShowObjectTransform(Transform& transform)
{
	ImGui::Text("position:");
	ImGui::SameLine();
	ShowInputVector(transform.position, { "##posx", "##posy", "##posz" });

	ImGui::Text("rotation:");
	ImGui::SameLine();
	ShowInputVector(transform.rotation, { "##rotx", "##roty", "##rotz" });

	ImGui::Text("scale:   ");
	ImGui::SameLine();
	ShowInputVector(transform.scale, { "##scalex", "##scaley", "##scalez" });
}

void GUI::ShowDropdownMenu(std::vector<std::string>& items, std::string& currentItem, int& currentIndex, const char* label)
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

std::optional<std::string> GUI::ShowDevConsole()
{
	std::optional<std::string> inputText;

	if (Console::isOpen)
	{
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

		for (std::string message : Console::messages)
		{
			glm::vec3 color = Console::GetColorFromMessage(message);
			ImGui::TextColored(ImVec4(color.x, color.y, color.z, 1), message.c_str());
		}

		std::string result = "";
		ImGui::InputTextWithHint("##input", "Console commands...", &result);

		if (Input::IsKeyPressed(VirtualKey::Return)) // if enter is pressed place the input value into the optional variable
			inputText = result;

		ImGui::End();
	}
	return inputText;
}

void GUI::ShowMainMenuBar(bool& showWindowData, bool& showObjMeta, bool& ramGraph, bool& cpuGraph, bool& gpuGraph)
{
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.02f, 0.02f, 0.02f, 1));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.02f, 0.02f, 0.02f, 1));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 5);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 2);
	ImGui::BeginMainMenuBar();
	if (ImGui::BeginMenu("file"))
	{
		ImGui::Text("Load object");
		ImGui::Separator();
		if (ImGui::Button("Exit"))
			exit(0);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("view"))
	{
		if (ImGui::Button("object metadata")) showObjMeta = !showObjMeta;
		if (ImGui::Button("window data")) showWindowData = !showWindowData;
		ImGui::Separator();
		if (ImGui::Button("RAM graph")) ramGraph = !ramGraph;
		if (ImGui::Button("CPU graph")) cpuGraph = !cpuGraph;
		if (ImGui::Button("GPU graph")) gpuGraph = !gpuGraph;
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("renderer"))
	{
		if (ImGui::Button("show albedo"))  RayTracing::showAlbedo = !RayTracing::showAlbedo;
		if (ImGui::Button("show normals")) RayTracing::showNormals = !RayTracing::showNormals;
		if (ImGui::Button("show unique"))  RayTracing::showUniquePrimitives = !RayTracing::showUniquePrimitives;
		ImGui::Separator();
		if (ImGui::Button("show collision boxes")) Renderer::shouldRenderCollisionBoxes = !Renderer::shouldRenderCollisionBoxes;
		ImGui::Separator();
		ImGui::Button("view statistics");
		ImGui::EndMenu();
	}
	ImGui::EndMainMenuBar();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
}

void GUI::ShowSceneGraph(const std::vector<Object*>& objects, Win32Window* window)
{
	static int currentListBoxItem = -1;
	static bool viewMaterial = false;

	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.03f, 0.03f, 1));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.03f, 0.03f, 0.03f, 1));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.02f, 0.02f, 1));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 5);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGuiStyle& style = ImGui::GetStyle();
	ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight() + style.FramePadding.y));
	ImGui::SetNextWindowSize(ImVec2(window->GetWidth() / 8, window->GetHeight() - ImGui::GetFrameHeight() - style.FramePadding.y));
	ImGui::Begin("scene graph", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
	ImGui::BeginChild(1, ImVec2(0, (window->GetHeight() - ImGui::GetFrameHeight() - style.FramePadding.y) / 2));
	for (int i = 0; i < objects.size(); i++)
	{
		if (!ImGui::TreeNode(objects[i]->name.c_str()))
			continue;

		for (int j = 0; j < objects[i]->meshes.size(); j++)
		{
			ImGui::Text(objects[i]->meshes[j].name.c_str());
		}
		ImGui::TreePop();
	}
	ImGui::EndChild();
	ImGui::End();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);

	ShowObjectComponents(objects, window);
}

void GUI::ShowObjectTable(const std::vector<Object*>& objects)
{
	constexpr int columnCount = 9;
	ImGui::Begin("object metadata");
	ImGui::BeginTable("Object metadata", columnCount, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit);
	ImGui::TableHeader("object metadata");

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::Text("name");
	ImGui::TableNextColumn();
	ImGui::Text("handle");
	ImGui::TableNextColumn();
	ImGui::Text("state");
	ImGui::TableNextColumn();
	ImGui::Text("mesh count");
	ImGui::TableNextColumn();
	ImGui::Text("has script");
	ImGui::TableNextColumn();
	ImGui::Text("finished loading");
	ImGui::TableNextColumn();
	ImGui::Text("position");
	ImGui::TableNextColumn();
	ImGui::Text("rotation");
	ImGui::TableNextColumn();
	ImGui::Text("scale");
	for (int i = 0; i < objects.size(); i++)
	{
		Object* currentObj = objects[i];
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::Text(currentObj->name.c_str());
		ImGui::TableNextColumn();
		ImGui::Text(std::to_string(currentObj->handle).c_str());
		ImGui::TableNextColumn();
		ImGui::Text(Object::ObjectStateToString(currentObj->state).c_str());
		ImGui::TableNextColumn();
		ImGui::Text(std::to_string(currentObj->meshes.size()).c_str());
		ImGui::TableNextColumn();
		ImGui::Text(currentObj->HasScript() ? "true" : "false");
		ImGui::TableNextColumn();
		ImGui::Text(currentObj->HasFinishedLoading() ? "true" : "false");
		ImGui::TableNextColumn();
		ImGui::Text(Vec3ToString(currentObj->transform.position).c_str());
		ImGui::TableNextColumn();
		ImGui::Text(Vec3ToString(currentObj->transform.rotation).c_str());
		ImGui::TableNextColumn();
		ImGui::Text(Vec3ToString(currentObj->transform.scale).c_str());
	}

	ImGui::EndTable();
	ImGui::End();
}

void GUI::ShowFPS(int FPS)
{
	ImGui::Begin("##FPS Counter", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);

	ImGui::SetWindowPos(ImVec2(0, 0/*ImGui::GetWindowSize().y * 0.2f*/));
	ImGui::SetWindowSize(ImVec2(ImGui::GetWindowSize().x * 2, ImGui::GetWindowSize().y));
	std::string text = std::to_string(FPS) + " FPS";
	ImGui::Text(text.c_str());

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
	ImGui::Begin(label, nullptr, ImGuiWindowFlags_NoScrollbar);
	SetImGuiColors();

	ImPlot::BeginPlot("##Time Per Async Task", ImVec2(-1, 0), ImPlotFlags_Equal | ImPlotFlags_NoMouseText | ImPlotFlags_NoFrame);
	ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
	ImPlot::SetupAxesLimits(0, 1, 0, 1);
	const char* labels[] = { "Main Thread", "Script Thread", "Renderer Thread" };
	ImPlot::PlotPieChart(labels, data.data(), 3, 0.5, 0.5, 0.5, "%.1f", 180);
	ImPlot::EndPlot();
	ImGui::End();
}

void GUI::ShowGraph(const std::vector<uint64_t>& buffer, const char* label)
{
	ImGui::Begin("RAM Usage", nullptr, ImGuiWindowFlags_NoScrollbar);
	SetImGuiColors();

	ImPlot::BeginPlot("##Ram Usage Over Time", ImVec2(-1, 0), ImPlotFlags_NoInputs | ImPlotFlags_NoFrame | ImPlotFlags_CanvasOnly);
	ImPlot::SetupAxisLimits(ImAxis_X1, 0, 500);
	ImPlot::SetupAxisLimits(ImAxis_Y1, 0, buffer[buffer.size() - 1] * 1.3);
	ImPlot::SetupAxes("##x", "##y", ImPlotAxisFlags_NoTickLabels);
	ImPlot::PlotLine(label, buffer.data(), (int)buffer.size());
	ImPlot::EndPlot();
	ImGui::End();
}

void GUI::ShowGraph(const std::vector<float>& buffer, const char* label)
{
	ImGui::Begin(label, nullptr, ImGuiWindowFlags_NoScrollbar);
	SetImGuiColors();

	ImPlot::BeginPlot("##Ram Usage Over Time", ImVec2(-1, 0), ImPlotFlags_NoInputs | ImPlotFlags_NoFrame | ImPlotFlags_CanvasOnly);
	ImPlot::SetupAxisLimits(ImAxis_X1, 0, 100);
	ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100);
	ImPlot::SetupAxes("##x", "##y", ImPlotAxisFlags_NoTickLabels);
	ImPlot::PlotLine(label, buffer.data(), (int)buffer.size());
	ImPlot::EndPlot();
	ImGui::End();
}