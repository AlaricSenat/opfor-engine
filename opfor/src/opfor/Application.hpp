#pragma once

#include <memory>
#include "ecs/ecs.hpp"
#include "EngineObject.hpp"
#include "Mesh.hpp"
#include "Batch.hpp"
#include <unordered_map>
#include "TextureManager.hpp"
#include <fmt/format.h>
#include "Logger.hpp"
#include "Action.hpp"
#include "ILevel.hpp"
#include "opfor/core/base.hpp"
#include "opfor/core/Window.hpp"
#include "opfor/renderer/Context.hpp"
#include "opfor/renderer/Framebuffer.hpp"
#include "opfor/renderer/Texture.hpp"
#include "opfor/core/events/EngineEvents.hpp"
#include "opfor/core/Input.hpp"
#include "layers/LayerStack.hpp"
#include "renderer/SceneRenderer.hpp"
#include "layers/ImGuiLayer.hpp"

namespace ecs
{
	class SystemManager;
	class EntityManager;
}

struct ModelComponent;
struct LuaScriptComponent;

namespace opfor
{

class Model;

class Application
{
private:

	static inline Application *_Instance;

	UniquePtr<IWindow> _window;
	UniquePtr<IRendererContext> _context;

	ecs::ECSEngine _ecs;

	ecs::EntityManager *_entityManager;
	ecs::SystemManager *_systemManager;

	std::vector<std::unique_ptr<EngineObject>> _engineObjects;

	std::unordered_map<unsigned int, std::unique_ptr<opfor::Model>> _models;
	std::unordered_map<unsigned int, std::unique_ptr<IDrawable>> _meshes;
	std::unordered_map<unsigned int, std::unique_ptr<Batch>> _batches;
	std::unique_ptr<ILevel> _currentLevel;

	using MaterialContainer = std::pair<unsigned int, Material>;

	std::unordered_map<std::string, MaterialContainer> _materials;
	std::unordered_map<std::string, PbrMaterial> _pbrMaterials;

	opfor::SharedPtr<opfor::Framebuffer> _viewportFramebuffer;
	opfor::SharedPtr<opfor::Texture2D> _viewportTexture;

	static unsigned int _nextId;
	static unsigned int _nextMaterialId;

	enum class PlayState {
		Stopped,
		Playing,
		Paused,
	};
	PlayState _isPlaying;

	Callback<size_t> _selectItem;

	LayerStack _LayerStack;

	UniquePtr<SceneRenderer> _SceneRenderer;
	UniquePtr<ImGuiLayer> _ImGuiLayer;

	void InitViewport();

public:
	/// Observer for building the lighting of the level
	/// Call this to rebuild the lighting of the scene
	Action<> OnBuildLighting;

	/// Select an item in the editor
	Action<size_t> OnSelectItem;

	/// Called when the level starts
	Action<> OnStartPlaying;
	/// Called when the level stops
	Action<> OnStopPlaying;

	// Editor
	void OnRebuildModel(ModelComponent &model);
	void OnReloadScript(LuaScriptComponent &script);

	void StartPlaying()
	{
		if (_isPlaying == PlayState::Stopped) {
			_isPlaying = PlayState::Playing;
			OnStartPlaying();
		}
	}

	void PausePlaying()
	{
		if (_isPlaying == PlayState::Playing) {
			_isPlaying = PlayState::Paused;
		}
	}

	void StopPlaying()
	{
		if (_isPlaying == PlayState::Paused ||
			_isPlaying == PlayState::Playing) {
			_isPlaying = PlayState::Stopped;
			OnStopPlaying();
		}
	}

	auto IsPlaying() { return _isPlaying == PlayState::Playing; }

public:
	Application();
	~Application();
	Application(Application const &) = delete;

	void operator=(Application const &) = delete;

	static inline Application &Get()
	{
		return *_Instance;
	}

	int Run();
	void Update();
	void UpdateObjects();
	void UpdateSubobjects(std::vector<EngineObject*> subobjects);

	IWindow *GetWindow() { return _window.get(); }

	template <typename T, typename ... ArgTypes>
	[[nodiscard]] T *CreateEngineObject(ArgTypes... args)
	{
		static_assert(std::is_base_of<EngineObject, T>::value && !std::is_same<EngineObject, T>::value,
			"T must be derived from EngineObject");

		auto object = std::make_unique<T>(&_ecs, std::forward<ArgTypes>(args)...);
		auto ret = object.get();

		_engineObjects.push_back(std::move(object));

		return ret;
	}

	template <typename T, typename ... ArgTypes>
	void CreateComponentSystem(ArgTypes... args)
	{
		static_assert(std::is_base_of<ecs::ComponentSystem, T>::value && !std::is_same<ecs::ComponentSystem, T>::value,
			"T must be derived from EngineObject");

		_ecs.GetSystemManager()->InstantiateSystem<T>(std::forward(args)...);
	}

	template <typename ... ArgTypes>
	[[nodiscard]] ecs::IEntity<ArgTypes...> *CreateEntity()
	{
		return _ecs.GetEntityManager()->CreateEntity<ArgTypes...>();
	}

	unsigned int AddMesh(Mesh mesh)
	{
		auto to_ptr = std::make_unique<Mesh>(std::move(mesh));
		_meshes[_nextId] = std::move(to_ptr);

		return _nextId++;
	}

	IDrawable *GetMesh(unsigned int id)
	{
		auto mesh = _meshes.find(id);

		if (mesh != _meshes.end()) {
			return mesh->second.get();
		}

		return nullptr;
	}

	unsigned int AddBatch(std::unique_ptr<Batch> batch)
	{
		_meshes[_nextId] = std::move(batch);

		return _nextId++;
	}

	void AddPbrMaterial(PbrMaterial material)
	{
		if (_pbrMaterials.find(material.Name) != _pbrMaterials.end()) {
			Logger::Warn("Material {} already exists\n", material.Name);
		}

		_pbrMaterials[material.Name] = material;
	}

	std::optional<PbrMaterial const *> GetPbrMaterial(std::string const &name)
	{
		if (_pbrMaterials.find(name) != _pbrMaterials.end()) {
			return std::make_optional(&_pbrMaterials[name]);
		}
		return std::nullopt;
	}

	unsigned int GetMaterialId(std::string const &name)
	{
		return _materials[name].first;
	}

	std::vector<std::string> GetMaterialList() const
	{
		std::vector<std::string> materials;

		materials.resize(_materials.size());
		std::transform(_materials.begin(), _materials.end(), materials.begin(),
			[] (auto mat) { return mat.first; });
		return materials;
	}

	void Close()
	{
		_window->Close();
	}

	unsigned int RegisterModel(opfor::Model model);
	std::optional<opfor::Model const *> GetModel(unsigned int id) const;
	void RemoveModel(unsigned int id);

	void LoadLevel(std::string const &path)
	{
		_currentLevel->Load(path);
	}

	auto GetEntity(unsigned int id)
	{
		return _entityManager->GetEntity(id);
	}

	void DeleteEntity(unsigned int entityId)
	{
		_entityManager->DeleteEntity(entityId);
	}

	void RebuildModel(ModelComponent &model);

	auto &GetCurrentLevel() { return _currentLevel; }

	auto GetViewport() { return _viewportFramebuffer; }
	auto GetViewportTexture() { return _viewportTexture; }

	void OnEvent(Event &);
	bool OnWindowResize(WindowResizeEvent &e);
	bool OnWindowClose(WindowCloseEvent &e);

	void PushLayer(Layer *layer);
	void PushOverlay(Layer *overlay);
	void PopLayer(Layer *layer);
	void PopOverlay(Layer *overlay);

	template <typename ... ArgTypes>
	inline std::vector<ecs::IEntity<ArgTypes...>*> GetEntities()
	{
		return _ecs.GetEntityManager()->GetEntities<ArgTypes...>();
	}

	inline std::vector<ecs::IEntityBase*> GetAllEntities()
	{
		return _ecs.GetEntityManager()->GetAllEntities();
	}

	void RenderImgui();
};

UniquePtr<Application> CreateApplication();

}
