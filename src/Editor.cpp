#include <Windows.h>
#include <commdlg.h>

#include <hsl/StackMap.h>

#include "core/Editor.h"
#include "core/Object.h"
#include "core/MeshObject.h"
#include "core/Rigid3DObject.h"
#include "core/LightObject.h"

#include "renderer/gui.h"
#include "renderer/RayTracing.h"

#include "io/SceneLoader.h"
#include "io/SceneWriter.h"

#include "system/Input.h"
#include "system/FileDialog.h"

#include "HalesiaEngine.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui-1.91.7/implot.h>
#include <imgui-1.91.7/imgui.h>
#include <imgui-1.91.7/ImGuizmo.h>
#include <imgui-1.91.7/misc/cpp/imgui_stdlib.h>

#include <glm/gtc/type_ptr.hpp>

constexpr float BAR_WIDTH = 0.15f;
constexpr float LOWER_BAR_HEIGHT = 0.2f;

constexpr float VIEWPORT_WIDTH = 1.0f - BAR_WIDTH * 2.0f;
constexpr float VIEWPORT_HEIGHT = 1.0f - LOWER_BAR_HEIGHT;

constexpr std::string_view SUPPORTED_MESH_FILES = "*.obj;*.glb;*.fbx;";

void EditorCamera::Update(Window* window, float delta)
{
	int viewportWidth  = window->GetWidth()  * VIEWPORT_WIDTH;
	int viewportHeight = window->GetHeight() * VIEWPORT_HEIGHT;

	int viewportX = window->GetWidth()  * BAR_WIDTH;
	int viewportY = window->GetHeight() * LOWER_BAR_HEIGHT;

	int mouseX = 0;
	int mouseY = 0;

	window->GetAbsoluteCursorPosition(mouseX, mouseY);

	bool isInViewport    = (mouseX > viewportX && mouseX < (viewportX + viewportWidth)) && (mouseY > viewportY && mouseY < (viewportY + viewportHeight));
	bool buttonIsPressed = Input::IsKeyPressed(VirtualKey::MiddleMouseButton);

	if (isInViewport && buttonIsPressed)
		active = true;

	else if (!buttonIsPressed)
		active = false;

	if (active)
		DefaultUpdate(window, delta);
}

void Editor::Start()
{
	EngineCore& core = HalesiaEngine::GetInstance()->GetEngineCore();

	renderer = core.renderer;

	camera = AddCustomCamera<EditorCamera>();

	renderer->SetViewportOffsets({ BAR_WIDTH, 0.0f });
	renderer->SetViewportModifiers({ VIEWPORT_WIDTH, VIEWPORT_HEIGHT });

	src = GetFile("Scene file", "*.hsf;*.fbx;*.glb");
	if (src == "")
		return;

	LoadFile();
}

void Editor::Update(float delta)
{
	if (addObject)
	{
		Object* obj = AddObject({ "new object" });

		addObject = false;
	}

	if (pObjectToCopy != nullptr)
	{
		pObjectToCopy->CreateShallowCopy();
		pObjectToCopy = nullptr;
	}
}

void Editor::UpdateGUI(float delta)
{
	HalesiaEngine* engine = HalesiaEngine::GetInstance();
	EngineCore& core = engine->GetEngineCore();

	width = core.window->GetWidth();
	height = core.window->GetHeight();

	ShowUI();
}

void Editor::ShowGizmo()
{
	ImGuizmo::OPERATION mode = ImGuizmo::TRANSLATE;
	switch (gizmoMode)
	{
	case GizmoMode::Translate: break;
	case GizmoMode::Rotate: mode = ImGuizmo::ROTATE; break;
	case GizmoMode::Scale:  mode = ImGuizmo::SCALE;  break;
	}

	Object* obj = selectedObj;

	if (obj == nullptr)
		return;

	glm::mat4 view = glm::lookAtRH(camera->position, camera->position + camera->front, camera->up);
	glm::mat4 proj = glm::perspectiveRH(camera->fov, (float)width / (float)height, 0.001f, 10000.0f);

	glm::vec2 mod = renderer->GetViewportModifier();
	glm::vec2 off = renderer->GetViewportOffset();

	glm::mat4 model = obj->transform.GetModelMatrix();

	ImGuizmo::SetRect(off.x * width, off.y * height, mod.x * width, mod.y * height);
	ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), mode, ImGuizmo::WORLD, glm::value_ptr(model));

	glm::vec3 skew;
	glm::vec4 pers;
	glm::decompose(model, obj->transform.scale, obj->transform.rotation, obj->transform.position, skew, pers);
}

void Editor::MainThreadUpdate(float delta)
{
	if (save)
	{
		SaveToFile();
		save = false;
	}

	if (!queuedMeshChange.isApplied)
		queuedMeshChange.path = GetFile("Mesh file", SUPPORTED_MESH_FILES.data());
	else return;

	if (queuedMeshChange.path.empty())
	{
		queuedMeshChange.isApplied = true;
		return;
	}

	Object* obj = queuedMeshChange.object;
	
	MeshCreationData creationData = assetImport::LoadFirstMesh(queuedMeshChange.path);

	if (obj->IsType(Object::InheritType::Mesh))
	{
		MeshObject* ptr = dynamic_cast<MeshObject*>(obj);

		uint32_t matIndex = ptr->mesh.GetMaterialIndex();

		ptr->mesh.Destroy();
		ptr->mesh.Create(creationData);
		ptr->mesh.SetMaterial(Mesh::materials[matIndex]);
	}

	queuedMeshChange.isApplied = true;

	if (loadFile)
	{
		src = GetFile("Scene file", "*.hsf;*.fbx;");
		if (src != "")
		{
			DestroyCurrentScene();
			LoadFile();
		}
		loadFile = false;
	}
}

void Editor::ShowDefaultRightClick()
{
	static bool gizmoButton = false;

	if (!ImGui::BeginPopupContextVoid("##default_right_click"))
		return;

	selectionData.show = selectionData.show || ImGui::MenuItem("Add object");

	ImGui::SeparatorText("Gizmo");

	if (ImGui::MenuItem("Translate", nullptr, gizmoMode == GizmoMode::Translate)) 
		gizmoMode = GizmoMode::Translate;
	if (ImGui::MenuItem("Rotate", nullptr, gizmoMode == GizmoMode::Rotate))   
		gizmoMode = GizmoMode::Rotate;
	if (ImGui::MenuItem("Scale", nullptr, gizmoMode == GizmoMode::Scale))    
		gizmoMode = GizmoMode::Scale;

	ImGui::EndPopup();
}

void Editor::ShowUI()
{
	if (showUI)
	{
		ShowLowerBar();

		StartRightBar();

		ShowRenderPipelines();
		ShowSelectedObject();

		EndRightBar();

		ShowSideBars();

		if (selectionData.show)
			ShowAddObjectWindow();
	}

	ShowMenuBar();
	ShowGizmo();
	ShowDefaultRightClick();
	ShowMaterialWindow();
}

void Editor::ShowSideBars()
{
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.03f, 0.03f, 1));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.03f, 0.03f, 0.03f, 1));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.02f, 0.02f, 1));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 5);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	ImGuiStyle& style = ImGui::GetStyle();

	ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetFrameHeight() + style.FramePadding.y));
	ImGui::SetNextWindowSize(ImVec2(width * BAR_WIDTH, height - ImGui::GetFrameHeight() - style.FramePadding.y));

	ImGui::Begin("scene graph", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
	ImGui::BeginChild(1);

	for (int i = 0; i < allObjects.size(); i++)
	{
		ShowObjectWithChildren(allObjects[i]);
	}
	ImGui::EndChild();
	ImGui::End();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
}

void Editor::ShowObjectWithChildren(Object* object)
{
	std::string rightClickName = "##r_" + std::to_string((uint64_t)object);

	ImVec2 first = ImGui::GetWindowPos() + ImGui::GetCursorPos();

	std::string nodeName = "##n_" + object->name;
	bool success = false; // default state is that the object has no children
	if (!object->GetChildren().empty())
	{
		success = ImGui::TreeNode(nodeName.c_str());
		ImGui::SameLine();
	}

	if (ImGui::Selectable(object->name.c_str()))
		selectedObj = object;
	
	ImVec2 end = ImGui::GetWindowPos() + ImGui::GetCursorPos() + ImVec2(ImGui::GetWindowWidth(), 0.0f);
	ImVec2 mousePos = ImGui::GetMousePos();

	if (ImGui::BeginPopupContextItem(nodeName.c_str(), ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoReopen))
	{
		inSelectPopup = true;
		if (ImGui::MenuItem("add object"))
		{
			selectionData.parent = object;
			selectionData.show = true;
		}

		if (ImGui::MenuItem("copy"))
		{
			pObjectToCopy = object;
		}

		if (ImGui::MenuItem("delete"))
		{
			UIFree(object);
			ImGui::EndPopup();
			return;
		}

		ImGui::EndPopup();
	}

	if (!success)
		return;
	
	const std::vector<Object*>& children = object->GetChildren();
	for (Object* child : children)
	{
		ShowObjectWithChildren(child);
	}
	ImGui::TreePop();
}

void Editor::ShowMaterialWindow()
{
	static MaterialCreateInfo createInfo{};

	ImGui::SetNextWindowSize(ImVec2(-1, -1));
	ImGui::Begin("Material creator");

	ImGui::Text("Albedo:    ");
	ImGui::SameLine();
	if (ImGui::Button(createInfo.albedo.empty() ? "Set path ##albedo_path" : createInfo.albedo.c_str()))
		createInfo.albedo = GetFile("Image file", "*.png;*.jpg;");

	ImGui::Text("Normal:    ");
	ImGui::SameLine();
	if (ImGui::Button(createInfo.normal.empty() ? "Set path ##normal_path" : createInfo.normal.c_str()))
		createInfo.normal = GetFile("Image file", "*.png;*.jpg;");

	ImGui::Text("Roughness: ");
	ImGui::SameLine();
	if (ImGui::Button(createInfo.roughness.empty() ? "Set path ##roughness_path" : createInfo.roughness.c_str()))
		createInfo.roughness = GetFile("Image file", "*.png;*.jpg;");

	ImGui::Text("Metallic:  ");
	ImGui::SameLine();
	if (ImGui::Button(createInfo.metallic.empty() ? "Set path ##metallic_path" : createInfo.metallic.c_str()))
		createInfo.metallic = GetFile("Image file", "*.png;*.jpg;");

	ImGui::Text("Amb. occl.:");
	ImGui::SameLine();
	if (ImGui::Button(createInfo.ambientOcclusion.empty() ? "Set path ##ambient_occlusion_path" : createInfo.ambientOcclusion.c_str()))
		createInfo.ambientOcclusion = GetFile("Image file", "*.png;*.jpg;");

	if (ImGui::Button("Create material"))
	{
		Mesh::AddMaterial(Material::Create(createInfo));

		createInfo.albedo.clear();
		createInfo.normal.clear();
		createInfo.roughness.clear();
		createInfo.metallic.clear();
		createInfo.ambientOcclusion.clear();
	}
	ImGui::End();
}

void Editor::ShowMenuBar() // add renderer variables here like taa sample count
{
	HalesiaEngine* engine = HalesiaEngine::GetInstance();

	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.02f, 0.02f, 0.02f, 1));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.02f, 0.02f, 0.02f, 1));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 5);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 2);

	ImGui::BeginMainMenuBar();

	if (ImGui::BeginMenu("file"))
	{
		if (ImGui::MenuItem("Load object"))
		{
			std::string file = GetFile("Object file", "*.obj;");
			ObjectCreationData data = assetImport::LoadObjectFile(file);

			AddObject(data);
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Load file")) loadFile = true;
		if (ImGui::MenuItem("Save file")) save = true;

		ImGui::Separator();

		if (ImGui::MenuItem("Exit"))
			HalesiaEngine::Exit();
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("view"))
	{
		if (ImGui::MenuItem("object metadata")) engine->showObjectData = !engine->showObjectData;
		if (ImGui::MenuItem("window data"))     engine->showWindowData = !engine->showWindowData;
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("renderer"))
	{
		if (ImGui::MenuItem("show albedo"))  RayTracingRenderPipeline::showAlbedo = !RayTracingRenderPipeline::showAlbedo;
		if (ImGui::MenuItem("show normals")) RayTracingRenderPipeline::showNormals = !RayTracingRenderPipeline::showNormals;
		if (ImGui::MenuItem("show unique"))  RayTracingRenderPipeline::showUniquePrimitives = !RayTracingRenderPipeline::showUniquePrimitives;
		ImGui::Separator();
		if (ImGui::MenuItem("show collision boxes")) Renderer::shouldRenderCollisionBoxes = !Renderer::shouldRenderCollisionBoxes;
		ImGui::Separator();
		ImGui::MenuItem("view statistics");
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Add"))
	{
		if (ImGui::MenuItem("Object")) selectionData.show = true;
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("misc."))
	{
		if (ImGui::MenuItem("enable/disable UI"))
		{
			showUI = !showUI;
			if (showUI)
			{
				renderer->SetViewportOffsets({ BAR_WIDTH, 0.0f });
				renderer->SetViewportModifiers({ VIEWPORT_WIDTH, VIEWPORT_HEIGHT });
			}
			else
			{
				renderer->SetViewportOffsets({ 0.0f, 0.0f });
				renderer->SetViewportModifiers({ 1.0f, 1.0f });
			}
		}
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
}

void Editor::StartRightBar()
{
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.03f, 0.03f, 1));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.03f, 0.03f, 0.03f, 1));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.02f, 0.02f, 0.02f, 1));

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 5);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5);

	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.05f, 1);
	colors[ImGuiCol_Header] = ImVec4(0.06f, 0.06f, 0.06f, 1);
	colors[ImGuiCol_FrameBg] = ImVec4(0.06f, 0.06f, 0.06f, 1);
	colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 1);

	ImGui::SetNextWindowPos(ImVec2(width * (1.0f - BAR_WIDTH), ImGui::GetFrameHeight() + style.FramePadding.y));

	ImGui::SetNextWindowPos(ImVec2(width * (1.0f - BAR_WIDTH), ImGui::GetFrameHeight() + style.FramePadding.y));
	ImGui::SetNextWindowSize(ImVec2(width * BAR_WIDTH, height - ImGui::GetFrameHeight() - style.FramePadding.y));

	ImGui::Begin("components", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

}

void Editor::EndRightBar()
{
	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(3);
	ImGui::End();
}

void Editor::ShowSelectedObject()
{
	if (!ImGui::CollapsingHeader("Selected object"))
		return;

	ImGui::BeginChild(2);

	if (selectedObj != nullptr)
	{
		ShowObjectComponents();
	}

	ImGui::EndChild();
}

void Editor::ShowObjectComponents()
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed;

	if (ImGui::CollapsingHeader("Metadata", flags))
		GUI::ShowObjectData(selectedObj);

	if (ImGui::CollapsingHeader("Transform", flags))
		GUI::ShowObjectTransform(selectedObj->transform);

	if (selectedObj->IsType(Object::InheritType::Rigid3D))
	{
		if (ImGui::CollapsingHeader("rigid body", flags))
			ShowObjectRigidBody(dynamic_cast<Rigid3DObject*>(selectedObj)->rigid);
	}

	if (selectedObj->IsType(Object::InheritType::Mesh))
	{
		if (ImGui::CollapsingHeader("mesh", flags))
			ShowObjectMesh(dynamic_cast<MeshObject*>(selectedObj)->mesh);
	}

	if (selectedObj->IsType(Object::InheritType::Light))
	{
		if (ImGui::CollapsingHeader("light", flags))
			ShowObjectLight(dynamic_cast<LightObject*>(selectedObj));
	}
}

void Editor::ShowObjectLight(LightObject* light)
{
	static std::array<std::string_view, 3> lightTypes = { "Directional", "Point", "Spot" };

	std::string_view curr = Light::TypeToString(light->type);
	int currIndex = static_cast<int>(light->type);
	int nextIndex = currIndex;

	ImGui::Text("type:         ");
	ImGui::SameLine();
	GUI::ShowDropdownMenu(lightTypes, curr, nextIndex, "##selected_object_light");

	if (currIndex != nextIndex)
		light->type = Light::StringToType(curr);

	ImGui::Text("cutoff:       ");
	ImGui::SameLine();
	ImGui::InputFloat("##light_cutoff", &light->cutoff);

	ImGui::Text("outer cutoff: ");
	ImGui::SameLine();
	ImGui::InputFloat("##light_outer_cutoff", &light->outerCutoff);

	ImGui::Text("color:        ");
	ImGui::SameLine();
	GUI::ShowInputVector(light->color, { "##color_r", "##color_g", "##color_b" });

	ImGui::Text("direction:    ");
	ImGui::SameLine();
	GUI::ShowInputVector(light->direction, { "##direction_x", "##direction_y", "##direction_z" });
}

void Editor::ShowObjectRigidBody(RigidBody& rigid)
{
	static hsl::StackMap<std::string, Shape::Type, 3> stringToShape =
	{
		{ "SHAPE_TYPE_BOX",     Shape::Type::Box     },
		{ "SHAPE_TYPE_SPHERE",  Shape::Type::Sphere  },
		{ "SHAPE_TYPE_CAPSULE", Shape::Type::Capsule },
	};
	static hsl::StackMap<std::string, RigidBody::Type, 3> stringToRigid =
	{
		{ "RIGID_BODY_STATIC",    RigidBody::Type::Static    },
		{ "RIGID_BODY_DYNAMIC",   RigidBody::Type::Dynamic   },
		{ "RIGID_BODY_KINEMATIC", RigidBody::Type::Kinematic },
	};

	static int rigidIndex = -1;
	static int shapeIndex = -1;
	static std::array<std::string, 3> allShapeTypes = { "SHAPE_TYPE_BOX", "SHAPE_TYPE_SPHERE", "SHAPE_TYPE_CAPSULE" };
	static std::array<std::string, 3> allRigidTypes = { "RIGID_BODY_DYNAMIC", "RIGID_BODY_STATIC", "RIGID_BODY_KINEMATIC" };

	std::string currentRigid = RigidBody::TypeToString(rigid.type);
	std::string currentShape = Shape::TypeToString(rigid.shape.type);
	glm::vec3 holderExtents = rigid.shape.data;

	ImGui::Text("type:   ");
	ImGui::SameLine();
	GUI::ShowDropdownMenu(allRigidTypes, currentRigid, rigidIndex, "##RigidType");

	ImGui::Text("shape:  ");
	ImGui::SameLine();
	GUI::ShowDropdownMenu(allShapeTypes, currentShape, shapeIndex, "##rigidShapeMenu");

	switch (rigid.shape.type)
	{
	case Shape::Type::Box:
		ImGui::Text("Extents:");
		ImGui::SameLine();
		GUI::ShowInputVector(holderExtents, { "##extentsx", "##extentsy", "##extentsz" });
		break;
	case Shape::Type::Capsule:
		ImGui::Text("Height: ");
		ImGui::SameLine();
		ImGui::InputFloat("##height", &holderExtents.y);
		ImGui::Text("radius: ");
		ImGui::SameLine();
		if (ImGui::InputFloat("##radius", &holderExtents.x))
			holderExtents.y += holderExtents.x; // add radius on top of the half height
		break;
	case Shape::Type::Sphere:
		ImGui::Text("radius: ");
		ImGui::SameLine();
		ImGui::InputFloat("##radius", &holderExtents.x);
		break;
	}

	ImGui::Text("Force:   %.1f, %.1f, %.1f", rigid.queuedUpForce.x, rigid.queuedUpForce.y, rigid.queuedUpForce.z);

	// update the rigidbody shape if it has been changed via the gui
	if (stringToShape[currentShape] != rigid.shape.type || holderExtents != rigid.shape.data)
	{
		Shape newShape = Shape::GetShapeFromType(stringToShape[currentShape], holderExtents);
		rigid.ChangeShape(newShape);
	}
	if (stringToRigid[currentRigid] != rigid.type)
	{
		rigid.Destroy();
		Object* obj = (Object*)rigid.GetUserData();
		rigid = RigidBody(rigid.shape, stringToRigid[currentRigid], obj->transform.position, obj->transform.rotation);
		rigid.SetUserData(obj);
	}
}

void Editor::ShowObjectMesh(Mesh& mesh)
{
	ImGui::Text
	(
		"Memory:\n"
		"  vertex:   %I64u\n"
		"  d_vertex: %I64u\n"
		"  index:    %I64u\n"
		"BLAS:       %I64u\n"
		"face count: %i\n\n"
		"center:     %.2f, %.2f, %.2f\n"
		"extents:    %.2f, %.2f, %.2f\n",
		mesh.vertexMemory, mesh.defaultVertexMemory, mesh.indexMemory, (uint64_t)mesh.BLAS.get(), mesh.faceCount, mesh.center.x, mesh.center.y, mesh.center.z, mesh.extents.x, mesh.extents.y, mesh.extents.z);

	int index = static_cast<int>(mesh.GetMaterialIndex());

	ImGui::Text("Material: ");
	ImGui::SameLine();
	ImGui::InputInt("##inputmeshmaterialindex", &index);

	if (index != static_cast<int>(mesh.GetMaterialIndex()))
		mesh.SetMaterialIndex(index);

	if (ImGui::Button("Change mesh"))
		QueueMeshChange(selectedObj);
}

void Editor::ShowAddObjectWindow()
{
	ImGui::Begin("Add object");

	int typeCount = static_cast<int>(Object::InheritType::TypeCount);
	for (int i = 0; i < typeCount; i++)
	{
		Object::InheritType type = static_cast<Object::InheritType>(i);
		std::string_view typeAsString = Object::InheritTypeToString(type);

		if (!ImGui::Selectable(typeAsString.data()))
			continue;

		ObjectCreationData data{};
		data.type = static_cast<ObjectCreationData::Type>(type); // should always convert correctly
		AddObject(data, selectionData.parent);

		selectionData.show = false;
	}

	ImGui::End();
}

void Editor::ShowLowerBar()
{
	ImGuiStyle& style = ImGui::GetStyle();

	ImGui::SetNextWindowPos(ImVec2(width * BAR_WIDTH, height * (1.0f - LOWER_BAR_HEIGHT)));
	ImGui::SetNextWindowSize(ImVec2(width * VIEWPORT_WIDTH, height * LOWER_BAR_HEIGHT));

	ImGui::Begin("##LowerBar", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
	ImGui::BeginChild(3);

	GUI::ShowDevConsoleContent();

	ImGui::EndChild();
	ImGui::End();
}

void Editor::ShowRenderPipelines()
{
	if (!ImGui::CollapsingHeader("Render pipelines"))
		return;

	const std::vector<RenderPipeline*> pipelines = renderer->GetAllRenderPipelines();
	for (RenderPipeline* pipeline : pipelines)
	{
		std::string name = renderer->GetRenderPipelineName(pipeline);
		std::string msg = name + ": ";

		ImGui::Text(msg.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Reload"))
			pipeline->ReloadShaders(renderer->GetPipelinePayload(renderer->GetActiveCommandBuffer(), camera));

		std::vector<RenderPipeline::IntVariable> vars = pipeline->GetIntVariables();
		if (vars.empty())
			continue;

		for (const RenderPipeline::IntVariable& var : vars)
		{
			std::string full = std::string(var.name) + ":##" + name;
			ImGui::Text(var.name.data());
			ImGui::SameLine();
			ImGui::InputInt(full.c_str(), var.pValue);
		}
	}
}

void Editor::QueueMeshChange(Object* object)
{
	queuedMeshChange.isApplied = false;
	queuedMeshChange.object = object;
}

void Editor::DestroyCurrentScene()
{
	for (Object* obj : allObjects)
		delete obj;

	for (int i = 1; i < Mesh::materials.size(); i++)
		Mesh::materials[i].Destroy();
	Mesh::materials.resize(1);
}

void Editor::LoadFile()
{
	SceneLoader loader(src);
	loader.LoadScene();

	for (const ObjectCreationData& data : loader.objects)
		AddObject(data);

	for (const MaterialCreationData& data : loader.materials)
		Mesh::AddMaterial(Material::Create(data));
}

void Editor::SaveToFile()
{
	HSFWriter::WriteHSFScene(this, src);
}

std::string Editor::GetFile(const char* desc, const char* type)
{
	FileDialog::Filter filter{};
	filter.description = desc;
	filter.fileType = type;
	
	return FileDialog::RequestFile(filter);
}

void Editor::UIFree(Object* pObject)
{
	if (selectedObj == pObject)
		selectedObj = nullptr;

	if (rightClickedObj == pObject)
		rightClickedObj = nullptr;

	if (pObjectToCopy == pObject)
		pObjectToCopy = nullptr;

	Free(pObject);
}