#include "interactive_viewer.hpp"

#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_impl_sdl3.h>

namespace
{

template <std::integral T>
SDL_Rect bounds_to_rect(Bbox2<T> const &bounds)
{
  return {
    .x = as<int>(bounds.min.x),
    .y = as<int>(bounds.min.y),
    .w = as<int>(bounds.max.x - bounds.min.x),
    .h = as<int>(bounds.max.y - bounds.min.y),
  };
}

template <Numeric T>
SDL_FRect bounds_to_frect(Bbox2<T> const &bounds)
{
  return {
    .x = as<float>(bounds.min.x),
    .y = as<float>(bounds.min.y),
    .w = as<float>(bounds.max.x - bounds.min.x),
    .h = as<float>(bounds.max.y - bounds.min.y),
  };
}

}

InteractiveViewer::InteractiveViewer(std::span<const Color3f> image,
    uint32_t image_width, uint32_t image_height)
    : _image(image), _image_width(image_width), _image_height(image_height)
{
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    PT_FATAL("failed to initialize SDL ({})", SDL_GetError());

  if (!SDL_CreateWindowAndRenderer("Path tracer", as<int>(image_width),
          as<int>(image_height), SDL_WINDOW_RESIZABLE, &_window, &_renderer))
  {
    PT_FATAL("couldn't create window/renderer: {}", SDL_GetError());
  }

  _displayed_framebuffer_region = _framebuffer_screen_loc
      = Bbox2u(splat<Point2u>(0), Point2u(image_width, image_height));

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
    {
      return false;
    }
    if (event.type == SDL_EVENT_WINDOW_RESIZED)
    {
      update_framebuffer_location_and_size();
    }
    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
        && event.window.windowID == SDL_GetWindowID(_window))
    {
      return false;
    }
  }

  update_texture();

  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  ImGui::Render();
  SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
  SDL_RenderClear(_renderer);
  auto const srcrect = bounds_to_frect(_displayed_framebuffer_region);
  auto const dstrect = bounds_to_frect(_framebuffer_screen_loc);
  SDL_RenderTexture(_renderer, _framebuffer, &srcrect, &dstrect);
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
  auto const rect = bounds_to_rect(chunk.bounds);
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

void InteractiveViewer::update_framebuffer_location_and_size()
{
  int new_width, new_height;
  SDL_GetWindowSize(_window, &new_width, &new_height);
  auto const window_half_width = new_width / 2;
  auto const window_half_height = new_height / 2;
  auto const image_half_width = as<int>(_image_width) / 2;
  auto const image_half_height = as<int>(_image_height) / 2;

  auto const img_start_pos_x
      = std::abs(std::min(window_half_width - image_half_width, 0));
  auto const img_start_pos_y
      = std::abs(std::min(window_half_height - image_half_height, 0));
  auto const img_end_pos_x = as<int>(_image_width) - img_start_pos_x;
  auto const img_end_pos_y = as<int>(_image_height) - img_start_pos_y;

  auto const new_img_half_width = (img_end_pos_x - img_start_pos_x) / 2;
  auto const new_img_half_height = (img_end_pos_y - img_start_pos_y) / 2;

  _displayed_framebuffer_region.min.x = as<uint32_t>(img_start_pos_x);
  _displayed_framebuffer_region.min.y = as<uint32_t>(img_start_pos_y);
  _displayed_framebuffer_region.max.x = as<uint32_t>(img_end_pos_x);
  _displayed_framebuffer_region.max.y = as<uint32_t>(img_end_pos_y);

  _framebuffer_screen_loc.min.x
      = as<uint32_t>(std::max(window_half_width - new_img_half_width, 0));
  _framebuffer_screen_loc.min.y
      = as<uint32_t>(std::max(window_half_height - new_img_half_height, 0));
  _framebuffer_screen_loc.max.x = as<uint32_t>(
      std::min(window_half_width + new_img_half_width, new_width));
  _framebuffer_screen_loc.max.y = as<uint32_t>(
      std::min(window_half_height + new_img_half_height, new_height));
}