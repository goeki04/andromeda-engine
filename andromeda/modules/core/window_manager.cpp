#include "window_manager.hpp"
#include "stb_image.h"
#include <cstring>
#include <stdexcept>
#include <GL/glew.h>
#include "a_primitives.hpp"
#include <iostream>
namespace Andromeda::Window {
    SDL_Surface* WindowManager::CreateSDLSurface(const char* path)
    {
        i32 w, h, channels;
        unsigned char* pixels = stbi_load(path, &w, &h, &channels, 4);
        if (!pixels) {
            SDL_Log("Failed to load image: %s", stbi_failure_reason());
            return nullptr;
        }

        SDL_Surface* surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
        if (!surface) {
            stbi_image_free(pixels);
            SDL_Log("Failed to create surface: %s", SDL_GetError());
            return nullptr;
        }
        SDL_LockSurface(surface);
        std::memcpy(surface->pixels, pixels, (size_t)w * (size_t)h * 4);
        SDL_UnlockSurface(surface);

        stbi_image_free(pixels);
        return surface;
    }
    void WindowManager::start() {

        // SDL_WindowFlags is a Uint64 in SDL3 - an i16 silently truncates every flag >= 0x8000.
        SDL_WindowFlags windowFlags = 0;
        SDL_Surface* surface = CreateSDLSurface(ASSET_PATH "logo.png");

        windowFlags |= SDL_WINDOW_MAXIMIZED;
        windowFlags |= SDL_WINDOW_RESIZABLE;
        // Without this the window is created at logical size on a scaled display, so the whole
        // UI is rendered below native resolution and looks soft / pixelated.
        windowFlags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        g_Window = SDL_CreateWindow("Andromeda", NULL, NULL, windowFlags | SDL_WINDOW_OPENGL);
        if (!g_Window) {
            throw std::runtime_error("Failed to call SDL_CreateWindow!");
        }

        SDL_GetWindowSizeInPixels(g_Window, &g_WindowWidth, &g_WindowHeight);
        SDL_SetWindowMinimumSize(g_Window, g_WindowWidth, g_WindowHeight);
        SDL_SetWindowIcon(g_Window, surface);
        m_GlContext = SDL_GL_CreateContext(g_Window);
        if (!m_GlContext) {
            throw std::runtime_error("Failed to create SDL_GL context!");
        }
        glewExperimental = true;
        GLenum err = glewInit();
        if (err != GLEW_OK) {
            throw std::runtime_error("Failed to call glewInit!");
        }

        if (!GLEW_ARB_gl_spirv) {
			throw std::runtime_error("OpenGL driver does not support SPIR-V shaders. Please update your graphics drivers.");
        }
    }

    void WindowManager::destroy() {
        SDL_DestroyWindow(g_Window);
        SDL_Quit();
    }
}