#include "interactive_viewer.hpp"

#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_impl_sdl3.h>

InteractiveViewer::InteractiveViewer(std::span<const Color3f> image,
    uint32_t image_width, uint32_t image_height)
    : _image(image), _image_width(image_width), _image_height(image_height)
{
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    PT_FATAL("failed to initialize SDL ({})", SDL_GetError());

  if (!SDL_CreateWindowAndRenderer("Path tracer", as<int>(image_width),
          as<int>(image_height), 0, &_window, &_renderer))
  {
    PT_FATAL("couldn't create window/renderer: {}", SDL_GetError());
  }

  _framebuffer = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGBA32,
      SDL_TEXTUREACCESS_STREAMING, as<int>(image_width),
      as<int>(image_height));

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGui_ImplSDL3_InitForSDLRenderer(_window, _renderer);
  ImGui_ImplSDLRenderer3_Init(_renderer);
}

void InteractiveViewer::update_chunk(Bbox2u const &bounds, uint32_t spp)
{
  std::lock_guard lock(_finished_chunks_mutex);
  _finished_chunks.push(ImageWriteChunk{ bounds, spp });
  ++_pending_updates_count;
}

void InteractiveViewer::wait_for_pending_updates()
{
  std::unique_lock lock(_finished_chunks_mutex);
  _cv.wait(lock, [this] { return _pending_updates_count == 0; });
}

bool InteractiveViewer::render_frame()
{
  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    ImGui_ImplSDL3_ProcessEvent(&event);
    if (event.type == SDL_EVENT_QUIT)
      return false;
    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
        && event.window.windowID == SDL_GetWindowID(_window))
      return false;
  }

  update_texture();

  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  ImGui::Render();
  SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
  SDL_RenderClear(_renderer);
  SDL_RenderTexture(_renderer, _framebuffer, nullptr, nullptr);
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), _renderer);
  SDL_RenderPresent(_renderer);

  return true;
}

InteractiveViewer::~InteractiveViewer()
{
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyTexture(_framebuffer);
  SDL_DestroyRenderer(_renderer);
  SDL_DestroyWindow(_window);
  SDL_Quit();
}

std::optional<InteractiveViewer::ImageWriteChunk>
InteractiveViewer::next_finished_chunk()
{
  std::lock_guard lock(_finished_chunks_mutex);
  if (!_finished_chunks.empty())
  {
    ImageWriteChunk chunk = _finished_chunks.front();
    _finished_chunks.pop();
    return chunk;
  }
  return {};
}

void InteractiveViewer::update_texture_region(ImageWriteChunk const &chunk)
{
  SDL_Rect const rect{
    .x = as<int>(chunk.bounds.min.x),
    .y = as<int>(chunk.bounds.min.y),
    .w = as<int>(chunk.bounds.max.x - chunk.bounds.min.x),
    .h = as<int>(chunk.bounds.max.y - chunk.bounds.min.y),
  };
  void *texture_pixels;
  int pitch;
  SDL_LockTexture(_framebuffer, &rect, &texture_pixels, &pitch);
  for (size_t i = 0; i < as<size_t>(rect.h); ++i)
  {
    for (size_t j = 0; j < as<size_t>(rect.w); ++j)
    {
      auto const tex_idx = i * (as<size_t>(pitch) / sizeof(uint32_t)) + j;
      auto const image_idx
          = (as<size_t>(rect.y) + i) * _image_width + as<size_t>(rect.x) + j;
      auto const &c = _image[image_idx];
      static_cast<uint32_t *>(texture_pixels)[tex_idx]
          = as_32bit_value(to_srgb(c / as<Real>(chunk.spp)));
    }
  }
  SDL_UnlockTexture(_framebuffer);
  {
    std::unique_lock lock(_finished_chunks_mutex);
    --_pending_updates_count;
  }
  _cv.notify_all();
}

void InteractiveViewer::update_texture()
{
  while (auto const chunk = next_finished_chunk())
    update_texture_region(*chunk);
}