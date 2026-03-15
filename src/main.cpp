#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_impl_sdl3.h>

#include <thread>
#include <vector>
#include <algorithm>
#include <execution>
#include <mutex>
#include <queue>

#include <toml++/toml.hpp>

#include "cameras.hpp"
#include "scene.hpp"
#include "color.hpp"
#include "integrators.hpp"
#include "utils.hpp"

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
  for (uint32_t i = 0; i < n_chunks_y; ++i)
  {
    for (uint32_t j = 0; j < n_chunks_x; ++j)
    {
      ImageChunk chunk;
      chunk.x_start = j * CHUNK_SIZE;
      chunk.y_start = i * CHUNK_SIZE;
      chunk.x_end = std::min(j * CHUNK_SIZE + CHUNK_SIZE, width);
      chunk.y_end = std::min(i * CHUNK_SIZE + CHUNK_SIZE, height);
      result.push_back(chunk);
    }
  }
  return result;
}

struct App
{
  App(uint32_t image_w, uint32_t image_h)
      : image_width(image_w), image_height(image_h)
  {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
      PT_FATAL("failed to initialize SDL ({})", SDL_GetError());

    if (!SDL_CreateWindowAndRenderer(
            "Path tracer", image_width, image_height, 0, &window, &renderer))
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

    image = std::vector<Color3f>(image_width * image_height);

    init_scene();

    render_thread = std::thread([&] { render(); });
  }

  void init_scene()
  {
    try
    {
      scene = Scene::read_from_file("cornell.toml");
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
    Timer render_timer;
    std::for_each(std::execution::par_unseq, chunks.begin(), chunks.end(),
        [&](ImageChunk const &chunk)
        {
          for (uint32_t y = chunk.y_start; y < chunk.y_end; ++y)
          {
            for (uint32_t x = chunk.x_start; x < chunk.x_end; ++x)
            {
              for (size_t s = 0; s < 128; ++s)
              {
                auto const ray
                    = scene.camera->camera_ray(Point2f((float)x, (float)y));
                auto const c
                    = scene.integrator->Li(scene, *scene.sampler, ray);
                image[y * image_width + x] += c;
              }
            }
          }
          std::lock_guard lock(finished_chunks_mutex);
          finished_chunks.push(chunk);
        });
    auto const render_time = render_timer.stop();
    std::println("rendering took {}", duration_string(render_time));
  }

  std::optional<ImageChunk> next_finished_chunk()
  {
    std::lock_guard lock(finished_chunks_mutex);
    if (!finished_chunks.empty())
    {
      ImageChunk chunk = finished_chunks.front();
      finished_chunks.pop();
      return chunk;
    }
    return {};
  }

  void update_texture_region(ImageChunk const &chunk) const
  {
    SDL_Rect const rect{
      .x = static_cast<int>(chunk.x_start),
      .y = static_cast<int>(chunk.y_start),
      .w = static_cast<int>(chunk.x_end - chunk.x_start),
      .h = static_cast<int>(chunk.y_end - chunk.y_start),
    };
    void *texture_pixels;
    int pitch;
    SDL_LockTexture(framebuffer, &rect, &texture_pixels, &pitch);
    for (size_t i = 0; i < rect.h; ++i)
    {
      for (size_t j = 0; j < rect.w; ++j)
      {
        auto const tex_idx = i * (pitch / sizeof(uint32_t)) + j;
        auto const image_idx = (rect.y + i) * image_width + rect.x + j;
        auto const &c = image[image_idx];
        static_cast<uint32_t *>(texture_pixels)[tex_idx]
            = as_32bit_value(jodie_reinhard(c));
      }
    }
    SDL_UnlockTexture(framebuffer);
  }

  void update_texture()
  {
    while (auto const chunk = next_finished_chunk())
      update_texture_region(*chunk);
  }

  bool main_loop()
  {
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT)
        return false;
      if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
          && event.window.windowID == SDL_GetWindowID(window))
        return false;
    }

    update_texture();

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
  std::vector<Color3f> image;
  SDL_Texture *framebuffer;
  SDL_Renderer *renderer;
  SDL_Window *window;
  std::thread render_thread;

  std::mutex finished_chunks_mutex;
  std::queue<ImageChunk> finished_chunks;

  Scene scene;
};

int main()
{
  auto app = App(1280, 720);

  while (app.main_loop())
    ;

  return 0;
}