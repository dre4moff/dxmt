/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MADEIRA_DLSS_ABI_H
#define MADEIRA_DLSS_ABI_H
#include <stdint.h>

/* Private NGX <-> Madeira D3D12 boundary. Fixed-width fields on x64/ARM64EC.
 * Resources are original ID3D12Resource pointers, never Metal object handles.
 * A successful record appends GPU work to the caller's command list. */
#define MADEIRA_DLSS_ABI_VERSION 1u
struct madeira_dlss_desc {
    uint32_t size, version, flags, reset;
    uint32_t input_width, input_height, output_width, output_height;
    uint64_t color, output, depth, motion, exposure;
    float motion_scale_x, motion_scale_y, jitter_x, jitter_y, pre_exposure;
    uint32_t reserved;
};
#ifdef __cplusplus
static_assert(sizeof(madeira_dlss_desc) == 96);
extern "C" {
#else
_Static_assert(sizeof(struct madeira_dlss_desc) == 96, "DLSS boundary layout");
#endif
typedef int32_t (*madeira_dlss_supported_fn)(void *device);
typedef int32_t (*madeira_dlss_create_fn)(void *list, uint32_t width, uint32_t height,
    uint32_t output_width, uint32_t output_height, uint32_t flags, uint64_t *feature);
typedef int32_t (*madeira_dlss_record_fn)(void *list, uint64_t feature, const struct madeira_dlss_desc *desc);
typedef void (*madeira_dlss_release_fn)(uint64_t feature);
#ifdef __cplusplus
}
#endif
#endif
