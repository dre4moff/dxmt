/* SPDX-License-Identifier: GPL-3.0-or-later
 * Batch only adjacent declarations with identical usage and stage masks.
 * Never move a declaration across a bind, draw, dispatch or fence command.
 * Requires the wmtcmd definitions from winemetal_thunks.h. */
#ifndef MADEIRA_WMT_RESOURCE_BATCH_H
#define MADEIRA_WMT_RESOURCE_BATCH_H
#define WMT_RESOURCE_BATCH_CAPACITY 64
struct wmt_resource_batch {
    obj_handle_t resources[WMT_RESOURCE_BATCH_CAPACITY];
    const struct wmtcmd_base *next;
    unsigned count;
    uint64_t usage, stages;
};
static inline struct wmt_resource_batch
wmt_collect_resources(const struct wmtcmd_base *head, int compute) {
    struct wmt_resource_batch batch = {.next = head};
    const unsigned type = compute ? WMTComputeCommandUseResource : WMTRenderCommandUseResource;
    while (batch.next && batch.next->type == type && batch.count < WMT_RESOURCE_BATCH_CAPACITY) {
        obj_handle_t resource;
        uint64_t usage, stages = 0;
        if (compute) {
            const struct wmtcmd_compute_useresource *body = (const void *)batch.next;
            resource = body->resource; usage = body->usage;
        } else {
            const struct wmtcmd_render_useresource *body = (const void *)batch.next;
            resource = body->resource; usage = body->usage; stages = body->stages;
        }
        if (batch.count && (usage != batch.usage || stages != batch.stages)) break;
        batch.usage = usage; batch.stages = stages;
        batch.resources[batch.count++] = resource;
        batch.next = batch.next->next.ptr;
    }
    return batch;
}
#endif
