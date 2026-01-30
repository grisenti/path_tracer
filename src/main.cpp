#include <SDL3/SDL.h>
#include "log.hpp"
#include <imgui.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_impl_sdl3.h>
#include <thread>

#include <vector>

#include <gmath/vec.hpp>

struct App
{
  App(uint32_t image_w, uint32_t image_h) : image_width(image_w),
                                            image_height(image_h)
  {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
      ERROR("failed to initialize SDL ({})", SDL_GetError());
      std::exit(-1);
    }

    if (!SDL_CreateWindowAndRenderer("Path tracer", image_width, image_height,
        0, &window,
        &renderer))
    {
      ERROR("couldn't create window/renderer: {}", SDL_GetError());
      std::exit(-1);
    }

    framebuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING, image_width, image_height);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    image = std::vector<uint32_t>(image_width * image_height);
    for (auto &p : image)
    {
      p = 0xff0000ff;
    }

    render_thread = std::thread([&]
    {
      render();
    });
  }

  void render()
  {
    std::print("hello");
    render_done = true;
  }


  bool main_loop()
  {
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT)
        return false;
      if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.
          windowID == SDL_GetWindowID(window))
        return false;
    }

    if (render_done)
    {
      SDL_UpdateTexture(framebuffer, nullptr, image.data(),
          1280 * sizeof(uint32_t));
      render_done = false;
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    ImGui::Render();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, framebuffer, nullptr, nullptr);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);

    return true;
  }

  ~App()
  {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyTexture(framebuffer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    render_thread.join();
  }

  uint32_t image_width;
  uint32_t image_height;
  std::vector<uint32_t> image;
  SDL_Texture *framebuffer;
  SDL_Renderer *renderer;
  SDL_Window *window;
  std::thread render_thread;
  std::atomic<bool> render_done = false;
};

int main()
{
  auto app = App(1280, 720);

  while (app.main_loop());

  return 0;
}