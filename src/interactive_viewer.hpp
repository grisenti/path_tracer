#pragma once

#include "common.hpp"
#include "core/bbox.hpp"

#include <mutex>
#include <condition_variable>
#include <queue>

#include "core/color.hpp"

struct SDL_Texture;
struct SDL_Renderer;
struct SDL_Window;

class InteractiveViewer
{
public:
  InteractiveViewer(std::span<const Color3f> image, uint32_t image_width,
      uint32_t image_height);

  // the values in the image inside bounds cannot change until
  // `wait_for_pending_updates` is called
  void update_chunk(Bbox2u const &bounds, uint32_t spp);

  // cannot be called by the same thread that calls render_frame()
  void wait_for_pending_updates();

  bool render_frame();

  ~InteractiveViewer();

private:
  struct ImageWriteChunk
  {
    Bbox2u bounds;
    uint32_t spp;
  };

  std::span<const Color3f> _image;
  uint32_t _image_width;
  uint32_t _image_height;
  SDL_Texture *_framebuffer;
  SDL_Renderer *_renderer;
  SDL_Window *_window;

  std::mutex _finished_chunks_mutex;
  std::condition_variable _cv;
  uint32_t _pending_updates_count = 0;
  std::queue<ImageWriteChunk> _finished_chunks;

  std::optional<ImageWriteChunk> next_finished_chunk();
  void update_texture_region(ImageWriteChunk const &chunk);
  void update_texture();
};