#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_impl_sdl3.h>

#include <thread>
#include <vector>
#include <algorithm>
#include <execution>

#include <toml++/toml.hpp>

#include "cameras.hpp"
#include "scene.hpp"
#include "color.hpp"
#include "integrators.hpp"

struct ImageChunk
{
  uint32_t x_start;
  uint32_t y_start;
  uint32_t x_end;
  uint32_t y_end;
};

std::vector<ImageChunk> divide_image(uint32_t width, uint32_t height)
{
  uint32_t constexpr CHUNK_SIZE = 32;
  auto const n_chunks_x = width / CHUNK_SIZE + (width % CHUNK_SIZE != 0);
  auto const n_chunks_y = height / CHUNK_SIZE + (height % CHUNK_SIZE != 0);
  std::vector<ImageChunk> result;
  result.reserve(n_chunks_x * n_chunks_y);
  for (uint32_t i = 0; i < n_chunks_x; ++i)
  {
    for (uint32_t j = 0; j < n_chunks_y; ++j)
    {
      ImageChunk chunk;
      chunk.x_start = i * CHUNK_SIZE;
      chunk.y_start = j * CHUNK_SIZE;
      chunk.x_end = std::min(i * CHUNK_SIZE + CHUNK_SIZE, width);
      chunk.y_end = std::min(j * CHUNK_SIZE + CHUNK_SIZE, height);
      result.push_back(chunk);
    }
  }
  return result;
}

struct App
{
  App(uint32_t image_w, uint32_t image_h) : image_width(image_w),
                                            image_height(image_h)
  {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
      PT_FATAL("failed to initialize SDL ({})", SDL_GetError());

    if (!SDL_CreateWindowAndRenderer("Path tracer", image_width, image_height,
        0, &window,
        &renderer))
    {
      PT_FATAL("couldn't create window/renderer: {}", SDL_GetError());
    }

    framebuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING, image_width, image_height);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    image = std::vector<uint32_t>(image_width * image_height);

    init_scene();

    render_thread = std::thread([&]
    {
      render();
    });
  }

  void init_scene()
  {
    try
    {
      scene = Scene::read_from_file("test.toml");
    }
    catch (SceneParseException const &e)
    {
      std::println("Parsing error: {}", e.what());
      std::abort();
    }
  }

  void render()
  {
    auto const chunks = divide_image(image_width, image_height);
    std::for_each(std::execution::par_unseq, chunks.begin(), chunks.end(),
        [&](ImageChunk const &chunk)
        {
          for (uint32_t y = chunk.y_start; y < chunk.y_end; ++y)
          {
            for (uint32_t x = chunk.x_start; x < chunk.x_end; ++x)
            {
              auto const ray = scene.camera->camera_ray(Point2f((float)x,
                  (float)y));
              auto const c = scene.integrator->Li(scene, ray);
              image[y * image_width + x] = as_32bit_value(c);
            }
          }
        });
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
          image_width * sizeof(uint32_t));
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

  Scene scene;
};

int main()
{
  auto app = App(1280, 720);

  while (app.main_loop());

  return 0;
}