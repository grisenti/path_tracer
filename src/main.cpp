#include <SDL3/SDL.h>
#include "log.hpp"
#include <imgui.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_impl_sdl3.h>

int main()
{
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
  {
    ERROR("failed to initialize SDL ({})", SDL_GetError());
    return -1;
  }

  SDL_Window *window;
  SDL_Renderer *renderer;

  if (!SDL_CreateWindowAndRenderer("Path tracer", 1280, 720, 0, &window, &renderer))
  {
    ERROR("couldn't create window/renderer: {}", SDL_GetError());
    return -1;
  }

  //SDL_Texture* framebuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 1280, 720);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  bool running = true;
  while (running)
  {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT)
        running = false;
      if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
        running = false;
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    ImGui::Render();

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

    SDL_RenderPresent(renderer);
  }

  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  //SDL_DestroyTexture(render_texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}