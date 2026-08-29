#include <thread>
#include <vector>
#include <algorithm>
#include <execution>
#include <mutex>
#include <queue>
#include <stb_image_write.h>

#include "cameras.hpp"
#include "cli.hpp"
#include "scene.hpp"
#include "core/color.hpp"
#include "integrators.hpp"
#include "core/utils.hpp"
#include "concurrency.hpp"
#include "interactive_viewer.hpp"

std::vector<Bbox2u> divide_image(uint32_t width, uint32_t height)
{
  uint32_t constexpr CHUNK_SIZE = 32;
  auto const n_chunks_x = width / CHUNK_SIZE + (width % CHUNK_SIZE != 0);
  auto const n_chunks_y = height / CHUNK_SIZE + (height % CHUNK_SIZE != 0);
  std::vector<Bbox2u> result;
  result.reserve(n_chunks_x * n_chunks_y);
  for (uint32_t i = 0; i < n_chunks_y; ++i)
  {
    for (uint32_t j = 0; j < n_chunks_x; ++j)
    {
      auto const x_min = j * CHUNK_SIZE;
      auto const y_min = i * CHUNK_SIZE;
      auto const x_max = std::min(j * CHUNK_SIZE + CHUNK_SIZE, width);
      auto const y_max = std::min(i * CHUNK_SIZE + CHUNK_SIZE, height);
      result.push_back(Bbox2u({ x_min, y_min }, { x_max, y_max }));
    }
  }
  return result;
}

struct App
{
  App(ProgramOptions opts)
  {
    init_scene(opts.scene_file);
    if (opts.load_scene_only)
      std::exit(EXIT_SUCCESS);

    image = std::vector<Color3f>(scene.image_width * scene.image_height);
    image_width = scene.image_width;
    image_height = scene.image_height;
    output_file = opts.image_output;

    if (opts.interactive)
      interactive_viewer.emplace(image, scene.image_width, scene.image_height);
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
      for (uint32_t y = chunk.min.y; y < chunk.max.y; ++y)
      {
        for (uint32_t x = chunk.min.x; x < chunk.max.x; ++x)
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
      if (interactive_viewer)
        interactive_viewer->update_chunk(chunk, as<uint32_t>(total_spp));
    };
    if (interactive_viewer)
    {
      auto const log_spp = std::log2(scene.spp);
      // FUTURE: not a great solution, we can't start unless the previous
      // "layer" is done, could be wasting time
      for (size_t i = 0; i <= as<size_t>(std::ceil(log_spp)); ++i)
      {
        if (i == 0)
          iter_spp = 1;
        else
          iter_spp = std::min<uint32_t>(1 << (i - 1), scene.spp);
        total_spp += iter_spp;
        interactive_viewer->wait_for_pending_updates();
        auto task = RepeatedJob::task(chunks.size(), render_chunk);
        ThreadPool::get_global_thread_pool()->add(task);
        task->execute_remaining_and_wait();
      }
    }
    else
    {
      total_spp = iter_spp = scene.spp;
      auto task = RepeatedJob::task(chunks.size(), render_chunk);
      ThreadPool::get_global_thread_pool()->add(task);
      task->execute_remaining_and_wait();
    }

    auto const render_time = render_timer.check();
    std::println(
        "rendering took {} at {}spp", duration_string(render_time), total_spp);
    if (output_file)
    {
      std::vector<uint32_t> buffer(image.size());
      for (size_t i = 0; i < image.size(); ++i)
      {
        auto const c = image[i];
        auto const c32 = as_32bit_value(to_srgb(c / as<Real>(scene.spp)));
        buffer[i] = c32;
      }
      stbi_write_png(output_file->c_str(), as<int>(image_width),
          as<int>(image_height), 4, buffer.data(),
          as<int>(image_width * sizeof(uint32_t)));
    }
  }

  void run()
  {
    if (interactive_viewer)
    {
      render_task = SingleJob::task([&] { render(); });
      ThreadPool::get_global_thread_pool()->add(render_task);
      while (interactive_viewer->render_frame())
      {
      }
    }
    else
    {
      render();
    }
  }

  std::vector<Color3f> image;
  uint32_t image_width;
  uint32_t image_height;
  std::shared_ptr<Task> render_task;
  std::optional<InteractiveViewer> interactive_viewer;
  std::optional<std::string> output_file;

  Scene scene;
};

int main(int argc, char **argv)
{
  auto program_opts = parse_options(argc, argv);
  ThreadPool::init_global_thread_pool(program_opts.max_threads - 1);
  auto app = App(program_opts);
  app.run();
  // we don't care if all threads terminate
  std::quick_exit(EXIT_SUCCESS);
}