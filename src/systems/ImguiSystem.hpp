#pragma once

#include <glm/gtx/matrix_decompose.hpp>
#include <unistd.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "ImGuizmo/ImGuizmo.h"

#include "ecs/System.hpp"

#include "components/DisplayComponent.hpp"
#include "components/PlayerCameraComponent.hpp"
#include "components/SelectedComponent.hpp"
#include "components/TransformComponent.hpp"
#include "components/PointLightComponent.hpp"
#include "components/ModelComponent.hpp"
#include "components/LuaScriptComponent.hpp"

#include "lazy.hpp"
#include "Engine.hpp"
#include "Logger.hpp"
#include "ShaderManager.hpp"
#include "nfd.hpp"

#include "engine/core/Window.hpp"

class ImguiSystem : public ecs::ComponentSystem
{
private:
	opfor::IWindow *_window;

	bool _logAutoScroll;
	ecs::IEntityBase *_currentEntity;

	ImVec2 _ViewportPosition;
	ImVec2 _ViewportSize;

	size_t _SelectedItem = 0;

private:
	void BeginFrame()
	{
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
	}

	void EndFrame()
	{
		ImGui::EndFrame();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	void DrawGuizmoSelectedEnt()
	{
		auto cameraEnt = GetEntities<PlayerCameraComponent>()[0];
		auto camera = cameraEnt->Get<PlayerCameraComponent>();
		auto selectedEnt = GetEntities<TransformComponent, SelectedComponent>();
		if (selectedEnt.size() > 0) {

			auto selected = selectedEnt[0]->Get<TransformComponent>();

			glm::mat4 model(1.0f);
			model = glm::translate(model, selected.position);
			model = glm::scale(model, selected.scale);

			bool changed = false;

			ImGuizmo::BeginFrame();
			ImGuizmo::SetRect(_ViewportPosition.x, _ViewportPosition.y, _ViewportSize.x, _ViewportSize.y);
			{
				std::array<float, 3> rotation{} , translation{}, scale{};
				ImGuizmo::DecomposeMatrixToComponents(&model[0][0],
					translation.data(), rotation.data(), scale.data());

				ImGuizmo::RecomposeMatrixFromComponents(translation.data(), rotation.data(), scale.data(),
					&model[0][0]);
			}

			glm::quat rotation;
			glm::vec3 skew(0.0f);
			glm::vec4 persp(0.0f);
			ImGuizmo::DecomposeMatrixToComponents(&model[0][0], &selected.position[0], &rotation[0], &selected.scale[0]);

			glm::mat4 cpy = model;

			ImGuizmo::SetDrawlist();
			ImGuizmo::Manipulate(&camera.view[0][0], &camera.projection[0][0],
				ImGuizmo::TRANSLATE,
				ImGuizmo::WORLD,
				&model[0][0],
				nullptr,
				nullptr);

			if (cpy != model) {
				changed = true;
			}

			glm::decompose(model, selected.scale, rotation, selected.position, skew, persp);
			if (changed) {
				selectedEnt[0]->Set(selected);
			}

		}
	}

	void BeginDockspace()
	{
		ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
		bool is_open = true;

		ImGuiViewport *viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->GetWorkPos());
		ImGui::SetNextWindowSize(viewport->GetWorkSize());
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar |
									   ImGuiWindowFlags_NoDocking |
									   ImGuiWindowFlags_NoTitleBar |
									   ImGuiWindowFlags_NoCollapse |
									   ImGuiWindowFlags_NoResize |
									   ImGuiWindowFlags_NoMove |
									   ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Dockspace", &is_open, windowFlags);
		ImGui::PopStyleVar();
		ImGui::PopStyleVar(2);

		ImGuiIO &io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
			ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);
		}

	}

	void EndDockspace()
	{
		ImGui::End();
	}

	void MenuBar()
	{
		if (ImGui::BeginMainMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("Close")) {
					opfor::Engine::Get().Close();
				}

				ImGui::Separator();

				if (ImGui::MenuItem("New Level")) {
				}
				if (ImGui::MenuItem("Open Level")) {
					char *outPath = nullptr;
					NFD_OpenDialog(nullptr, getcwd(nullptr, 0), &outPath);
					if (outPath) {
						opfor::Engine::Get().LoadLevel(outPath);
					}
				}
				if (ImGui::MenuItem("Save Level")) {
				}
				if (ImGui::MenuItem("Save Level As...")) {
					char *outPath = nullptr;
					NFD_OpenDialog(nullptr, getcwd(nullptr, 0), &outPath);
				}

				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Lighting")) {
				if (ImGui::MenuItem("Build")) {
					/* Build Lighting for the Level */
					opfor::Engine::Get().OnBuildLighting();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	void Materials()
	{
		auto materialList = opfor::Engine::Get().GetMaterialList();
		std::sort(materialList.begin(), materialList.end());
		ImGui::Begin("Materials");
		if (ImGui::TreeNode("Materials")) {
			for (auto const &m: materialList) {
				ImGui::Text("%s", m.c_str());
			}
			ImGui::TreePop();
		}
		ImGui::End();
	}

	void Log()
	{
		if (!ImGui::Begin("Log")) {
			ImGui::End();
			return ;
		}

		if (ImGui::BeginPopup("Options")) {
			ImGui::Checkbox("Auto-Scroll", &_logAutoScroll);
			ImGui::EndPopup();
		}

		if (ImGui::Button("Options")) {
			ImGui::OpenPopup("Options");
		}
		ImGui::SameLine();
		bool clear = ImGui::Button("Clear");
		if (clear) {
			Logger::Clear();
		}

		ImGui::Separator();
		ImGui::BeginChild("scolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		auto &logs = Logger::GetLog();
		auto &lineOffsets = Logger::GetLineOffsets();

		const char *bufp = logs.data();
		const char *bufendp = logs.data() + logs.size();

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		ImGuiListClipper clipper;
		clipper.Begin(lineOffsets.size());
		while (clipper.Step()) {
			for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++) {
				const char *line_start = bufp + lineOffsets[line_no];
				const char *line_end = (line_no + 1 < static_cast<int>(lineOffsets.size())) ?
					(bufp + lineOffsets[line_no + 1] - 1) : bufendp;
				ImGui::TextUnformatted(line_start, line_end);
			}
		}
		clipper.End();

		ImGui::PopStyleVar();

		if (_logAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
			ImGui::SetScrollHereY(1.0f);
		}

		ImGui::EndChild();

		ImGui::End();
	}

	void SceneHierarchy()
	{
		ImGui::Begin("Scene Hierarchy");

		if (ImGui::BeginPopupContextWindow("hierarchy_popup_menu")) {
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("hierarchy_popup_menu")) {
			if (ImGui::BeginMenu("Add...")) {
				if (ImGui::MenuItem("Entity")) {
					auto ent = opfor::Engine::Get().GetCurrentLevel()->CreateEntity();
					(void)ent;
				}
				if (ImGui::MenuItem("Point Light")) {
					auto ent = opfor::Engine::Get().GetCurrentLevel()->CreateEntity();
					ent->AddComponents<PointLightComponent>();
					(void)ent;
				}
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}

		auto allEnts = GetAllEntities();

		size_t itemIndex = 0;
		for (auto const &ent : allEnts) {
			auto name = ent->GetName();
			name += "##" + std::to_string(itemIndex);
			if (ImGui::Selectable(name.c_str(), _SelectedItem == itemIndex)) {
				Logger::Verbose("Clicked on item {} (prev. {}) (ID. {})\n",
					itemIndex, _SelectedItem, allEnts[itemIndex]->GetId());
				_SelectedItem = itemIndex;
				_currentEntity = allEnts[itemIndex];
				opfor::Engine::Get().OnSelectItem(_SelectedItem);
			}
			itemIndex++;
		}

		ImGui::End();
	}

	void ObjectTransform()
	{
		if (!_currentEntity) { return ; }
		if (!_currentEntity->HasComponents<TransformComponent>()) { return; }

		if (!ImGui::CollapsingHeader("Transform")) { return ; }

		bool changed = false;

		auto &transform = _currentEntity->Get<TransformComponent>();

		std::array<float, 3> rotation{};
		std::array<float, 3> translation{ transform.position.x, transform.position.y, transform.position.z };
		std::array<float, 3> scale{ transform.scale.x, transform.scale.y, transform.scale.z };

		changed += ImGui::InputFloat3("Position", translation.data(), 3);
		changed += ImGui::InputFloat3("Rotation", rotation.data(), 3);
		changed += ImGui::InputFloat3("Scale", scale.data(), 3);

		if (ImGui::Button("Reset")) {
			rotation.fill(0.0f);
			translation.fill(0.0f);
			scale.fill(1.0f);
			changed = true;
		}

		if (changed) {
			transform.position = { translation[0], translation[1], translation[2] };
			transform.scale = { scale[0], scale[1], scale[2] };
		}
	}

	void ObjectMesh()
	{
		if (!_currentEntity) { return ; }
		if (!_currentEntity->HasComponents<ModelComponent>()) { return; }

		if (!ImGui::CollapsingHeader("Static Mesh")) { return ; }

		auto &model = _currentEntity->Get<ModelComponent>();

		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
		ImGui::Columns(2, NULL, true);

		ImGui::Text("File Path");
		ImGui::NextColumn();

		std::string inputFilePath = model.Path;

		if (ImGui::InputText("##FilePath", &inputFilePath)) {
		}
		ImGui::SameLine();
		if (ImGui::Button("...")) {
			char *newPath = nullptr;
			// Open File Dialog
			if (NFD_OpenDialog(nullptr, getcwd(nullptr, 0), &newPath) == NFD_OKAY
				&& newPath) {
				model.Path = std::string(newPath);
				opfor::Engine::Get().OnRebuildModel(model);
			}
		}

		ImGui::Columns(1);
	}

	void ObjectLight()
	{
		if (!_currentEntity) { return ; }
		if (!_currentEntity->HasComponents<PointLightComponent>()) { return; }

		if (!ImGui::CollapsingHeader("Light")) { return ; }

		auto &light = _currentEntity->Get<PointLightComponent>();

		std::array<const char *, 1> lightTypes = { "Point" };
		size_t currentTypeIdx = 0;
		const char *comboLabel = lightTypes[currentTypeIdx];

		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
		ImGui::Columns(2, NULL, true);

		ImGui::Text("Type");
		ImGui::NextColumn();
		if (ImGui::BeginCombo("##LightTypeCombo", comboLabel, 0)) {
			for (size_t i = 0; i < lightTypes.size(); i++) {
				const bool isSelected = currentTypeIdx == i;
				if (ImGui::Selectable(lightTypes[i], isSelected)) {
					currentTypeIdx = i;
				}

				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::NextColumn();

		ImGui::Text("Color");
		ImGui::NextColumn();
		std::array<float, 3> color{ light.Color[0], light.Color[1], light.Color[2] };
		if (ImGui::ColorEdit3("##LightColor", color.data())) {
			light.Color = { color[0], color[1], color[2] };
		}
		ImGui::NextColumn();

		ImGui::Text("Intensity");
		ImGui::NextColumn();
		float intensity = light.Intensity;
		if (ImGui::InputFloat("##LightIntensity", &intensity)) {
			light.Intensity = intensity;
		}

		ImGui::Columns(1);
	}

	void ObjectLuaScript()
	{
		if (!_currentEntity) { return ; }
		if (!_currentEntity->HasComponents<LuaScriptComponent>()) { return; }

		if (!ImGui::CollapsingHeader("Lua Script")) { return ; }

		auto &luaScript = _currentEntity->Get<LuaScriptComponent>();

		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
		ImGui::Columns(2, NULL, true);

		std::string scriptPath = luaScript.Path;
		ImGui::Text("Script");
		ImGui::NextColumn();
		ImGui::InputText("##ScriptPath", &scriptPath);
		ImGui::SameLine();
		if (ImGui::Button("...##LuaScriptPathButton")) {
			char *newPath = nullptr;
			if (NFD_OpenDialog(nullptr, getcwd(nullptr, 0), &newPath) != NFD_CANCEL &&
				newPath != nullptr) {
				luaScript.Path = std::string(newPath);
				opfor::Engine::Get().OnReloadScript(luaScript);
				luaScript.Runtime.PushGlobal("__ENTITY_ID__", _currentEntity->GetId());
			}
		}
	}

	void Properties()
	{
		if (_currentEntity == nullptr) { return ; }

		ImGui::Begin("Properties");
			std::string entityName = _currentEntity->GetName();
			if (ImGui::InputText("##EntityName", &entityName)) {
				_currentEntity->SetName(entityName);
			}

			uuids::uuid const &uuid = _currentEntity->GetUuid();
			ImGui::SameLine();
			ImGui::LabelText("##UUID", "%s", uuids::to_string(uuid).data());
			ImGui::Separator();

			ObjectTransform();
			ObjectMesh();
			ObjectLight();
			ObjectLuaScript();

			ImGui::Separator();
			if (ImGui::Button("Add Component")) {
				ImGui::OpenPopup("add_component_popup");
			}

			if (ImGui::BeginPopupContextItem("add_component_popup")) {
				if (ImGui::MenuItem("Mesh")) {
					auto [ shaderId, shader ] = ShaderManager::Get().Create("shaders/basic.glsl");

					// TODO: Recycle existing shaders
					shader.Bind();
					shader.SetUniform("material.albedo", 0);
					shader.SetUniform("material.metallicRoughness", 1);
					shader.SetUniform("material.normal", 2);
					shader.Unbind();

					_currentEntity->AddComponents<ModelComponent>();

					auto &model = _currentEntity->Get<ModelComponent>();
					model.Shader = shaderId;
				}
				if (ImGui::MenuItem("Transform")) {
					_currentEntity->AddComponents<TransformComponent>();
				}
				if (ImGui::MenuItem("Lua Script")) {
					_currentEntity->AddComponents<LuaScriptComponent>();
				}
				if (ImGui::MenuItem("Child Entity")) {
				}
				ImGui::EndPopup();
			}

		ImGui::End();
	}

	void PlayMenu()
	{
		if (ImGui::Begin("Editor Actions")) {
			ImGui::PushItemWidth(-1);
			if (opfor::Engine::Get().IsPlaying()) {
				if (ImGui::Button("Pause")) {
				}
				ImGui::SameLine();
				if (ImGui::Button("Stop")) {
					opfor::Engine::Get().StopPlaying();
				}
			}
			else {
				if (ImGui::Button("Play")) {
					opfor::Engine::Get().StartPlaying();
				}
			}
			ImGui::PopItemWidth();
		}
		ImGui::End();
	}

	void Viewport()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar;

		if (ImGui::Begin("Viewport", nullptr, windowFlags)) {

			_ViewportSize = ImGui::GetWindowSize();
			_ViewportPosition = ImGui::GetWindowPos();

			ImVec2 winSize = ImGui::GetWindowSize();

			// TODO: Automatically adjust to viewport's aspect ratio.

			//ImGui::Image(reinterpret_cast<void*>(engine::Engine::Instance().GetViewport()), { winSize.x, winSize.x / (16.0f / 9.0f) },
			//	ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 0.0f));
			ImGui::Image(reinterpret_cast<void*>(opfor::Engine::Get().GetViewportTexture()->GetRawHandle()),
				{ winSize.x, winSize.x / (16.0f / 9.0f) },
				ImVec2(0.0f, 1.0f),
				ImVec2(1.0f, 0.0f),
				ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
				ImVec4(1.0f, 1.0f, 1.0f, 0.0f));

			DrawGuizmoSelectedEnt();
		}
		ImGui::End();

		ImGui::PopStyleVar();
	}

	void SetupImGuiStyle()
	{
		ImGuiStyle & style = ImGui::GetStyle();
		ImVec4 * colors = style.Colors;

		/// 0 = FLAT APPEARENCE
		/// 1 = MORE "3D" LOOK
		int is3D = 0;

		colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_TextDisabled]           = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
		colors[ImGuiCol_ChildBg]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
		colors[ImGuiCol_WindowBg]               = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
		colors[ImGuiCol_PopupBg]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
		colors[ImGuiCol_Border]                 = ImVec4(0.12f, 0.12f, 0.12f, 0.71f);
		colors[ImGuiCol_BorderShadow]           = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
		colors[ImGuiCol_FrameBg]                = ImVec4(0.42f, 0.42f, 0.42f, 0.54f);
		colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.42f, 0.42f, 0.42f, 0.40f);
		colors[ImGuiCol_FrameBgActive]          = ImVec4(0.56f, 0.56f, 0.56f, 0.67f);
		colors[ImGuiCol_TitleBg]                = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
		colors[ImGuiCol_TitleBgActive]          = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.17f, 0.17f, 0.17f, 0.90f);
		colors[ImGuiCol_MenuBarBg]              = ImVec4(0.335f, 0.335f, 0.335f, 1.000f);
		colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.24f, 0.24f, 0.24f, 0.53f);
		colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
		colors[ImGuiCol_CheckMark]              = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
		colors[ImGuiCol_SliderGrab]             = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
		colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.64f, 0.64f, 0.64f, 1.00f);
		colors[ImGuiCol_Button]                 = ImVec4(0.54f, 0.54f, 0.54f, 0.35f);
		colors[ImGuiCol_ButtonHovered]          = ImVec4(0.52f, 0.52f, 0.52f, 0.59f);
		colors[ImGuiCol_ButtonActive]           = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
		colors[ImGuiCol_Header]                 = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
		colors[ImGuiCol_HeaderHovered]          = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
		colors[ImGuiCol_HeaderActive]           = ImVec4(0.76f, 0.76f, 0.76f, 0.77f);
		colors[ImGuiCol_Separator]              = ImVec4(0.000f, 0.000f, 0.000f, 0.137f);
		colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.700f, 0.671f, 0.600f, 0.290f);
		colors[ImGuiCol_SeparatorActive]        = ImVec4(0.702f, 0.671f, 0.600f, 0.674f);
		colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
		colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
		colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
		colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
		colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
		colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.73f, 0.73f, 0.73f, 0.35f);
		colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
		colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
		colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);

		style.PopupRounding = 3;

		style.WindowPadding = ImVec2(4, 4);
		style.FramePadding  = ImVec2(6, 4);
		style.ItemSpacing   = ImVec2(6, 2);

		style.ScrollbarSize = 18;

		style.WindowBorderSize = 1;
		style.ChildBorderSize  = 1;
		style.PopupBorderSize  = 1;
		style.FrameBorderSize  = is3D;

		style.WindowRounding    = 0;
		style.ChildRounding     = 0;
		style.FrameRounding     = 0;
		style.ScrollbarRounding = 2;
		style.GrabRounding      = 0;

		#ifdef IMGUI_HAS_DOCK
			style.TabBorderSize = is3D;
			style.TabRounding   = 0;

			colors[ImGuiCol_DockingEmptyBg]     = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
			colors[ImGuiCol_Tab]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
			colors[ImGuiCol_TabHovered]         = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
			colors[ImGuiCol_TabActive]          = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
			colors[ImGuiCol_TabUnfocused]       = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
			colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
			colors[ImGuiCol_DockingPreview]     = ImVec4(0.85f, 0.85f, 0.85f, 0.28f);

			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				style.WindowRounding = 0.0f;
				style.Colors[ImGuiCol_WindowBg].w = 1.0f;
			}
		#endif
	}

public:
	ImguiSystem() : _window(nullptr), _logAutoScroll(true), _currentEntity(nullptr)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		SetupImGuiStyle();

		// Enable Docking
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		auto &io = ImGui::GetIO();

		ImFont *roboto = io.Fonts->AddFontFromFileTTF("thirdparty/imgui/misc/fonts/Roboto-Medium.ttf", 16);
		io.FontDefault = roboto;
	}

	~ImguiSystem()
	{
		ImGui::DestroyContext();
	}

	void OnUpdate(float __unused deltaTime) override
	{
		if (!_window) {
			auto displays = GetEntities<DisplayComponent>();
			_window = displays[0]->Get<DisplayComponent>().window;
			// FIXME: Call the right implementation based on renderer context
			ImGui_ImplGlfw_InitForOpenGL(reinterpret_cast<GLFWwindow*>(_window->GetRawHandle()), true);
			ImGui_ImplOpenGL3_Init("#version 450");
		}

		bool b = true;

		glm::mat4 transform(1.0f);

		BeginFrame();
		BeginDockspace();
			ImGui::ShowDemoWindow(&b);
			Viewport();
			MenuBar();
			PlayMenu();
			Materials();
			Log();
			SceneHierarchy();
			Properties();
		EndDockspace();
		EndFrame();

		if (opfor::Input::GetKeyDown(opfor::KeyCode::Escape)) {
			auto playerEnts = GetEntities<PlayerCameraComponent>();
			if (playerEnts.size() > 0) {
				auto camera = playerEnts[0]->Get<PlayerCameraComponent>();
				auto displayEnt = GetEntities<DisplayComponent>()[0];
				auto display = displayEnt->Get<DisplayComponent>();

				// Enable/Disable input for camera
				camera.useInput = !camera.useInput;
				playerEnts[0]->Set(camera);

				// Enable/Switch cursor
				display.window->HideCursor(camera.useInput);
				displayEnt->Set(display);
			}
		}

		auto mouse = opfor::Input::GetMouseButton(opfor::MouseButton::ButtonRight) == opfor::KeyStatus::Pressed;
		if (mouse) {
			opfor::Engine::Get().GetWindow()->HideCursor(true);
		}
		else {
			opfor::Engine::Get().GetWindow()->HideCursor(false);
		}
	}

//	void EditTransform(const glm::mat4 &view, const glm::mat4 &proj)
//	{
//		glm::mat4 matrix(1.0f);
//		static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::ROTATE);
//		static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
//		if (ImGui::IsKeyPressed('t'))
//			mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
//		if (ImGui::IsKeyPressed('e'))
//			mCurrentGizmoOperation = ImGuizmo::ROTATE;
//		if (ImGui::IsKeyPressed('r')) // r Key
//			mCurrentGizmoOperation = ImGuizmo::SCALE;
//		if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
//			mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
//		ImGui::SameLine();
//		if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
//			mCurrentGizmoOperation = ImGuizmo::ROTATE;
//		ImGui::SameLine();
//		if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
//			mCurrentGizmoOperation = ImGuizmo::SCALE;
//
//		if (mCurrentGizmoOperation != ImGuizmo::SCALE)
//		{
//			if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
//				mCurrentGizmoMode = ImGuizmo::LOCAL;
//			ImGui::SameLine();
//			if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
//				mCurrentGizmoMode = ImGuizmo::WORLD;
//		}
//		static bool useSnap(false);
//		if (ImGui::IsKeyPressed(83))
//			useSnap = !useSnap;
//		ImGui::Checkbox("", &useSnap);
//		ImGui::SameLine();
//		ImGuiIO& io = ImGui::GetIO();
//		ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
//		ImGuizmo::Manipulate(&view[0][0], &proj[0][0], mCurrentGizmoOperation, mCurrentGizmoMode, &_guizmo[0][0], &_guizmo[0][0], nullptr);
//	}
};
