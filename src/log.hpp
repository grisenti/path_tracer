#pragma once

#include <print>

#define ERROR(msg, ...) ::std::print("ERROR: " msg, __VA_ARGS__)
#define WARN(msg, ...) ::std::print("warn: " msg, __VA_ARGS__)
#define INFO(msg, ...) ::std::print("info: " msg, __VA_ARGS__)

