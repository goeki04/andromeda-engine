#ifndef SDL_MAIN_USE_CALLBACKS
#define SDL_MAIN_USE_CALLBACKS 1
#endif
#include "a_andromeda_subsystems_include.hpp"
#include "a_environment.hpp"
Andromeda::Window::WindowManager windowManager;
Andromeda::InputSystem inputManager;
Andromeda::Renderer renderer;
Andromeda::ResourceManager resourceManager;
Andromeda::Editor::Editor editor;
Andromeda::SceneManager sceneManager;
Andromeda::NetworkManager networkManager;
Andromeda::ParticleBindingSystem particleBindingSystem;
Andromeda::ParticleGraphSystem particleGraphSystem;
Andromeda::OS::OS_Manager os_Manager;

SDL_AppResult SDL_Init() {
    SDL_SetAppMetadata("ESP32", "1.0", "ESP32.goeki.com");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
}

std::string g_ProjectPath = "";

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    Andromeda::Log::init();
    A_INFO("Andromeda starting up");

    if (argc > 1) {
        for (int32_t i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "-project" && i + 1 < argc) {
                g_ProjectPath = argv[i + 1];
            }
        }
    }
    else {
#if defined(_WIN32)
        auto base = Andromeda::getEnvironmentVariable("APPDATA");
#else
        auto base = Andromeda::getEnvironmentVariable("XDG_DATA_HOME");
        if (!base) {
            if (const auto home = Andromeda::getEnvironmentVariable("HOME")) {
                base = *home + "/.local/share";
            }
        }
#endif
        assert(base && "could not determine a user data directory");
        if (base) {
            g_ProjectPath = (std::filesystem::path(*base) / "Andromeda" / "DefaultProject").string();
        }
    }
	std::filesystem::create_directories(g_ProjectPath);
    SDL_Init();
    Andromeda::SystemManager::getInstance().addSubsystem(&os_Manager);
    Andromeda::SystemManager::getInstance().addSubsystem(&windowManager);
    Andromeda::SystemManager::getInstance().addSubsystem(&inputManager);
    Andromeda::SystemManager::getInstance().addSubsystem(&resourceManager);
    Andromeda::SystemManager::getInstance().addSubsystem(&sceneManager);
    Andromeda::SystemManager::getInstance().addSubsystem(&renderer);
    Andromeda::SystemManager::getInstance().addSubsystem(&editor);
	Andromeda::SystemManager::getInstance().addSubsystem(&networkManager);
	Andromeda::SystemManager::getInstance().addSubsystem(&particleBindingSystem);
	Andromeda::SystemManager::getInstance().addSubsystem(&particleGraphSystem);
	Andromeda::WeatherService weatherService;
	weatherService.getLiveWeatherData();
    Andromeda::SystemManager::getInstance().startSubsystems();
    const std::string scenePath = (std::filesystem::path(g_ProjectPath) / "scene.json").string();

    if (std::filesystem::exists(scenePath)) {
       Andromeda::SceneSerializer::load(scenePath, sceneManager.m_Registry, resourceManager);
    }

    return SDL_APP_CONTINUE;
}
  
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS; 
    }
    Andromeda::SystemManager::getInstance().updateEvent(event);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    Andromeda::SystemManager::getInstance().updateSubsystems();
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    Andromeda::SystemManager::getInstance().destroy();
}