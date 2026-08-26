#pragma once

#include "common.hpp"

#include <filesystem>

#include "shapes.hpp"

TriangleMesh read_obj_mesh(std::filesystem::path const &path,
    std::optional<Transform3D> const &transform);
