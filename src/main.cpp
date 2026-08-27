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
#include "cli.hpp"
#include "scene.hpp"
#include "core/color.hpp"
#include "integrators.hpp"
#include "core/utils.hpp"
#include "concurrency.hpp"

struct ImageReadChunk
{
  uint32_t x_start;
  uint32_t y_start;
  uint32_t x_end;
  uint32_t y_end;
};

struct ImageWriteChunk
{
  ImageWriteChunk(ImageReadChunk chunk, uint32_t spp)
      : x_start(chunk.x_start), y_start(chunk.y_start), x_end(chunk.x_end),
        y_end(chunk.y_end), spp(spp)
  {
  }

  uint32_t x_start;
  uint32_t y_start;
  uint32_t x_end;
  uint32_t y_end;
  uint32_t spp;
};

std::vector<ImageReadChunk> divide_image(uint32_t width, uint32_t height)
{
  uint32_t constexpr CHUNK_SIZE = 32;
  auto const n_chunks_x = width / CHUNK_SIZE + (width % CHUNK_SIZE != 0);
  auto const n_chunks_y = height / CHUNK_SIZE + (height % CHUNK_SIZE != 0);
  std::vector<ImageReadChunk> result;
  result.reserve(n_chunks_x * n_chunks_y);
  for (uint32_t i = 0; i < n_chunks_y; ++i)
  {
    for (uint32_t j = 0; j < n_chunks_x; ++j)
    {
      ImageReadChunk chunk;
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
  App(ProgramOptions opts)
  {
    ThreadPool::init_global_thread_pool(opts.max_threads);

    init_scene(opts.scene_file);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
      PT_FATAL("failed to initialize SDL ({})", SDL_GetError());

    if (!SDL_CreateWindowAndRenderer("Path tracer", as<int>(scene.image_width),
            as<int>(scene.image_height), 0, &window, &renderer))
    {
      PT_FATAL("couldn't create window/renderer: {}", SDL_GetError());
    }

    framebuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING, as<int>(scene.image_width),
        as<int>(scene.image_height));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    image = std::vector<Color3f>(scene.image_width * scene.image_height);

    render_task = SingleJob::task([&] { render(); });
    ThreadPool::get_global_thread_pool()->add(render_task);
  }

  void init_scene(std::string_view filename)
  {
    try
    {
      scene = Scene::read_from_file(filename);
    }
    catch (SceneParseException const &e)
    {
      std::println("Parsing error: {}", e.what());
      std::abort();
    }
  }

  void render()
  {
    auto const chunks = divide_image(scene.image_width, scene.image_height);
    Timer render_timer;

    size_t iter_spp = 1;
    size_t total_spp = 0;
    auto const render_chunk = [&](size_t i)
    {
      thread_local auto thread_sampler = scene.sampler->clone();
      auto const &chunk = chunks[i];
      for (uint32_t y = chunk.y_start; y < chunk.y_end; ++y)
      {
        for (uint32_t x = chunk.x_start; x < chunk.x_end; ++x)
        {
          for (size_t s = 0; s < iter_spp; ++s)
          {
            auto const ray = scene.camera->camera_ray(
                Point2f(as<float>(x), as<float>(y)));
            auto const c = scene.integrator->Li(scene, *thread_sampler, ray);
            image[y * scene.image_width + x] += c;
          }
        }
      }
      std::lock_guard lock(finished_chunks_mutex);
      finished_chunks.push(ImageWriteChunk(chunk, as<uint32_t>(total_spp)));
    };
    auto const log_spp = std::log2(scene.spp);
    // FUTURE: not a great solution, we can't start unless the previous "layer"
    // is done, could be wasting time
    for (size_t i = 0; i <= as<size_t>(std::ceil(log_spp)); ++i)
    {
      if (i == 0)
        iter_spp = 1;
      else
        iter_spp = std::min<uint32_t>(1 << (i - 1), scene.spp);
      total_spp += iter_spp;
      auto task = RepeatedJob::task(chunks.size(), render_chunk);
      ThreadPool::get_global_thread_pool()->add(task);
      task->execute_remaining_and_wait();
    }

    auto const render_time = render_timer.stop();
    std::println(
        "rendering took {} at {}spp", duration_string(render_time), total_spp);
  }

  std::optional<ImageWriteChunk> next_finished_chunk()
  {
    std::lock_guard lock(finished_chunks_mutex);
    if (!finished_chunks.empty())
    {
      ImageWriteChunk chunk = finished_chunks.front();
      finished_chunks.pop();
      return chunk;
    }
    return {};
  }

  void update_texture_region(ImageWriteChunk const &chunk) const
  {
    SDL_Rect const rect{
      .x = as<int>(chunk.x_start),
      .y = as<int>(chunk.y_start),
      .w = as<int>(chunk.x_end - chunk.x_start),
      .h = as<int>(chunk.y_end - chunk.y_start),
    };
    void *texture_pixels;
    int pitch;
    SDL_LockTexture(framebuffer, &rect, &texture_pixels, &pitch);
    for (size_t i = 0; i < as<size_t>(rect.h); ++i)
    {
      for (size_t j = 0; j < as<size_t>(rect.w); ++j)
      {
        auto const tex_idx = i * (as<size_t>(pitch) / sizeof(uint32_t)) + j;
        auto const image_idx = (as<size_t>(rect.y) + i) * scene.image_width
                               + as<size_t>(rect.x) + j;
        auto const &c = image[image_idx];
        static_cast<uint32_t *>(texture_pixels)[tex_idx]
            = as_32bit_value(to_srgb(c / as<Real>(chunk.spp)));
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
  }

  std::vector<Color3f> image;
  SDL_Texture *framebuffer;
  SDL_Renderer *renderer;
  SDL_Window *window;
  std::shared_ptr<Task> render_task;

  std::mutex finished_chunks_mutex;
  std::queue<ImageWriteChunk> finished_chunks;

  Scene scene;
};

int main(int argc, char **argv)
{
  auto program_opts = parse_options(argc, argv);
  auto app = App(program_opts);
  while (app.main_loop())
  {
  }

  // we don't care if all threads terminate
  std::quick_exit(EXIT_SUCCESS);
}