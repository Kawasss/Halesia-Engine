#pragma once
#include <optional>
#include <future>

#include "system/Window.h"
#include "system/Input.h"
#include "renderer/Renderer.h"

class Renderer;
class Scene;
class Profiler;
class AnimationManager;

struct HalesiaEngineCreateInfo
{
	Scene* startingScene = nullptr;               //!< the engine will use this scene as its entry point
	Window::CreateInfo windowCreateInfo{};        //!< the engines window will be created with this information
	VirtualKey devConsoleKey = VirtualKey::Tilde; //!< pressing this key will enable the developer console
	bool enableDevConsole = true;                 //!< if true, the developer console can be used by pressing 'devConsoleKey'
	bool playIntro = true;                        //!< the engine will play the intro if true
	RendererFlags renderFlags;                    //!< these flags will modify the renderer

	int argsCount = 0;                            //!< the 'argc' (__argc) of the main function
	char** args;                                  //!< the 'argv' (__argv) of the main function
};

struct EngineCore
{
	Renderer* renderer = nullptr;                 //!< the engines renderer
	Window*   window   = nullptr;                 //!< the engines window
	Scene*    scene    = nullptr;                 //!< the current scene, multiple scenes can exist at once, but only one is active
	Profiler* profiler = nullptr;                 //!< the currently in use profiler
	AnimationManager* animationManager = nullptr; //!< the renderers animation manager
	int maxFPS = -1;                              //!< determines the fps limit of the engine
};

class HalesiaEngine
{
public:
	enum class ExitCode
	{
		
		Success,          //!< succes means that the engine was told to exit or reached the end of its loop, i.e. the game window closes
		UnknownException, //!< an unknown exception occured, since the cause is unknown the engine has to terminate
		Exception,        //!< an error was thrown, this can be user defined or engine defined
	};
	static std::string ExitCodeToString(ExitCode exitCode);               //!< returns the exit code as a string

	static void Exit();                                                   //!< forces the engine to terminate the session

	static void SetCreateInfo(const HalesiaEngineCreateInfo& createInfo); //!< sets the createInfo for the next call to GetInstance()
	static HalesiaEngine* GetInstance();                                  //!< returns the engines instance. If no intance exists, it creates an instance of the engine with create info given by 'SetCreateInfo'
	EngineCore& GetEngineCore();                                          //!< returns the core components of the engine
	ExitCode Run();                                                       //!< this will run the game loop, it returns the exit code if the game was unexpectedly ended
	void LoadScene(Scene* newScene);                                      //!< this will load a new scene and replace the current scene
	void Destroy();                                                       //!< this destroys the engine

	bool pauseGame = false;      //!< if true, all game logic will stop running
	bool playOneFrame = false;   //!< if and the game is paused and this is true, then one frame of game logic will be run
	bool showFPS = false;        //!< shows the games estimated fps (1 / current_frame_time)
	bool showAsyncTimes = false; //!< shows the activity of all threads used in one frame
	bool showObjectData = false; //!< shows all metadata of all objects in the scene
	bool showWindowData = false; //!< shows all data related to the games window

private:
	void CheckInput();                                 //!< checks for certain input needed for the engine, like if the developer console should be opened
	void UpdateAsyncCompletionTimes(float frameDelta); //!< updates the activity of the threads
	void RegisterConsoleVars();                        //!< registers all variables to the console

	void OnLoad(HalesiaEngineCreateInfo& createInfo);  //!< this will load the current instance with the given create info
	void LoadVars();                                   //!< loads any variables stored on the disk
	void OnExit();                                     //!< destroys any of the engines resources and saves certain data to the disk

	void PlayIntro();                          //!< plays the engines intro

	void UpdateRenderer(float delta);          //!< runs renderer once
	void UpdateScene(float delta);             //!< runs the scene once

	void InitializeCoreComponents();           //!< initializes the engines core components as given in 'Engine Core'
	void InitializeSubSystems();               //!< intializes the systems that rely on the engines core

	static HalesiaEngineCreateInfo createInfo; //!< the engines create info
	EngineCore core;                           //!< the engines core

	bool playIntro = true;                     //!< if true, the is played on start up
	bool renderDevConsole = false;             //!< if true, the developer console is rendered
	VirtualKey devConsoleKey;                  //!< the key that triggers the developer console

	float asyncRendererCompletionTime = 0;     //!< time to complete the render thread
	float asyncScriptsCompletionTime = 0;      //!< time to complete the scene thread
	std::vector<float> asyncTimes;             //!< all thread times

	std::future<void> asyncScripts;            //!< the state of the scene thread
	std::future<void> asyncRenderer;           //!< the state of the renderer thread
};