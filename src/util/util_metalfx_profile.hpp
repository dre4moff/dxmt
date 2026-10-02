/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace dxmt {
struct MetalFXOutput {
  float factor;
  uint32_t width, height;
};
inline uint32_t metalFXOutputLimit(const std::string &value) {
  char *end;
  const unsigned long number = std::strtoul(value.c_str(), &end, 10);
  return end != value.c_str() && !*end && number >= 240 && number <= 4096 ? uint32_t(number) : 0;
}
inline MetalFXOutput metalFXOutput(uint32_t width, uint32_t height, float requested,
                                   uint32_t limit_width, uint32_t limit_height) {
  if (!width || !height || !std::isfinite(requested) || requested <= 1)
    return {1, width, height};
  double factor = requested;
  if (limit_width && limit_height) {
    factor = std::min({factor, double(limit_width) / width, double(limit_height) / height});
    // Do not mutate a game's buffers to shrink an already-large input.
    // Bypass MetalFX rather than supersampling above the selected output.
    if (factor <= 1) return {1, width, height};
  }
  uint32_t output_width = uint32_t(std::min(double(UINT32_MAX), width * factor));
  uint32_t output_height = uint32_t(std::min(double(UINT32_MAX), height * factor));
  if (limit_width && limit_height) {
    output_width = std::min(output_width, limit_width);
    output_height = std::min(output_height, limit_height);
  }
  if (output_width <= width || output_height <= height) return {1, width, height};
  return {float(factor), output_width, output_height};
}
} // namespace dxmt
