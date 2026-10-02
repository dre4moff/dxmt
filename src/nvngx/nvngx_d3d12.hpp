/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../include/madeira_dlss_abi.h"

struct ID3D12Device;
struct ID3D12GraphicsCommandList;

namespace dxmt {
struct MadeiraNGX12 {
  madeira_dlss_supported_fn supported;
  madeira_dlss_create_fn create;
  madeira_dlss_record_fn record;
  madeira_dlss_release_fn release;
  explicit operator bool() const { return supported && create && record && release; }
};
static MadeiraNGX12 madeira_ngx12() {
  // Resolve the backend actually loaded by the game. Never substitute another
  // runtime or load a second D3D12 engine just to answer NGX discovery.
  HMODULE module = GetModuleHandleW(L"d3d12.dll");
  if (!module) return {};
  return {reinterpret_cast<madeira_dlss_supported_fn>(GetProcAddress(module, "MadeiraD3D12TemporalSupported")),
          reinterpret_cast<madeira_dlss_create_fn>(GetProcAddress(module, "MadeiraD3D12TemporalCreate")),
          reinterpret_cast<madeira_dlss_record_fn>(GetProcAddress(module, "MadeiraD3D12TemporalRecord")),
          reinterpret_cast<madeira_dlss_release_fn>(GetProcAddress(module, "MadeiraD3D12TemporalRelease"))};
}
static bool s_ngx12_initialized;
static NVNGX_RESULT madeira_ngx12_init(ID3D12Device *device) {
  auto api = madeira_ngx12();
  s_ngx12_initialized = api && device && api.supported(device) >= 0;
  Logger::info(str::format("[dlss-metalfx] NGX D3D12 init supported=", s_ngx12_initialized));
  return s_ngx12_initialized ? NVNGX_RESULT_OK : NVNGX_RESULT_FEATURE_NOT_SUPPORTED;
}
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_Init(unsigned long long, const wchar_t *, ID3D12Device *device,
                                          const void *, unsigned int) { return madeira_ngx12_init(device); }
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_Init_Ext(unsigned long long, const wchar_t *, ID3D12Device *device,
                                              unsigned int, const void *) { return madeira_ngx12_init(device); }
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_Init_ProjectID(const char *, unsigned int, const char *, const wchar_t *,
    ID3D12Device *device, unsigned int, const void *) { return madeira_ngx12_init(device); }
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_Init_with_ProjectID(const char *, unsigned int, const char *, const wchar_t *,
    ID3D12Device *device, const void *, unsigned int) { return madeira_ngx12_init(device); }
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_Shutdown() { s_ngx12_initialized = false; return NVNGX_RESULT_OK; }
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_Shutdown1(ID3D12Device *) { return NVSDK_NGX_D3D12_Shutdown(); }
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_GetScratchBufferSize(unsigned int feature,
    const NVNGXParameter *params, size_t *size) { return NVSDK_NGX_D3D11_GetScratchBufferSize(feature, params, size); }
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_AllocateParameters(NVNGXParameter **params) {
  return NVSDK_NGX_D3D11_AllocateParameters(params);
}
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_DestroyParameters(NVNGXParameter *params) {
  return NVSDK_NGX_D3D11_DestroyParameters(params);
}
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_GetCapabilityParameters(NVNGXParameter **params) {
  auto result = NVSDK_NGX_D3D11_GetCapabilityParameters(params);
  if (result == NVNGX_RESULT_OK) {
    (*params)->Set("SuperSampling.Available", s_ngx12_initialized ? 1u : 0u);
    (*params)->Set(NVSDK_NGX_EParameter_SuperSampling_Available, s_ngx12_initialized ? 1u : 0u);
    (*params)->Set("SuperSampling.FeatureInitResult", s_ngx12_initialized ? NVNGX_RESULT_OK : NVNGX_RESULT_FEATURE_NOT_SUPPORTED);
  }
  return result;
}
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_GetParameters(NVNGXParameter **params) {
  return NVSDK_NGX_D3D12_GetCapabilityParameters(params);
}
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_GetFeatureRequirements(IDXGIAdapter *adapter,
    const NVNGX_FeatureDiscoveryInfo *info, NVNGX_FeatureRequirement *requirements) {
  if (!info || !requirements) return NVNGX_RESULT_INVALID_PARAMETER;
  if (!madeira_ngx12()) {
    requirements->FeatureSupported = NVNGX_FEATURE_SUPPORT_RESULT_UNSUPPORTED;
    return NVNGX_RESULT_FEATURE_NOT_SUPPORTED;
  }
  return NVSDK_NGX_D3D11_GetFeatureRequirements(adapter, info, requirements);
}

struct MadeiraDLSS12Feature : CommonFeature {
  uint64_t native;
  uint32_t flags, width, height, output_width, output_height;
  madeira_dlss_record_fn record;
  madeira_dlss_release_fn release;
};
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_CreateFeature(ID3D12GraphicsCommandList *list, unsigned int feature,
    NVNGXParameter *params, unsigned int **out) {
  if (!out) return NVNGX_RESULT_INVALID_PARAMETER;
  *out = nullptr;
  if (!list || !params) return NVNGX_RESULT_INVALID_PARAMETER;
  if (feature != NVNGX_FEATURE_SUPERSAMPLING) return NVNGX_RESULT_FEATURE_NOT_SUPPORTED;
  auto api = madeira_ngx12();
  if (!s_ngx12_initialized || !api) return NVNGX_RESULT_FEATURE_NOT_SUPPORTED;
  auto value = std::make_unique<MadeiraDLSS12Feature>();
  value->feature = NVNGX_FEATURE_SUPERSAMPLING;
  if (NVNGX_FAILED(params->Get(NVNGX_Parameter_Width, &value->width)) ||
      NVNGX_FAILED(params->Get(NVNGX_Parameter_Height, &value->height)) ||
      NVNGX_FAILED(params->Get(NVNGX_Parameter_OutWidth, &value->output_width)) ||
      NVNGX_FAILED(params->Get(NVNGX_Parameter_OutHeight, &value->output_height)))
    return NVNGX_RESULT_INVALID_PARAMETER;
  params->Get(NVNGX_Parameter_DLSS_Feature_Create_Flags, &value->flags);
  auto result = api.create(list, value->width, value->height, value->output_width,
                          value->output_height, value->flags, &value->native);
  if (result < 0) {
    Logger::warn(str::format("[dlss-metalfx] NGX D3D12 create rejected HRESULT=", result));
    return result == int32_t(0x80070057) ? NVNGX_RESULT_INVALID_PARAMETER : NVNGX_RESULT_FEATURE_NOT_SUPPORTED;
  }
  value->record = api.record; value->release = api.release;
  *out = &value.release()->handle;
  return NVNGX_RESULT_OK;
}
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_EvaluateFeature(ID3D12GraphicsCommandList *list,
    const unsigned int *handle, NVNGXParameter *params, void *) {
  if (!list || !handle || !params) return NVNGX_RESULT_INVALID_PARAMETER;
  auto feature = reinterpret_cast<const MadeiraDLSS12Feature *>(handle);
  if (feature->feature != NVNGX_FEATURE_SUPERSAMPLING || !feature->record) return NVNGX_RESULT_INVALID_PARAMETER;
  ID3D12Resource *color = nullptr, *output = nullptr, *depth = nullptr, *motion = nullptr, *exposure = nullptr;
  if (NVNGX_FAILED(params->Get(NVNGX_Parameter_Color, &color)) ||
      NVNGX_FAILED(params->Get(NVNGX_Parameter_Output, &output)) ||
      NVNGX_FAILED(params->Get(NVNGX_Parameter_Depth, &depth)) ||
      NVNGX_FAILED(params->Get(NVNGX_Parameter_MotionVectors, &motion)) ||
      !color || !output || !depth || !motion) return NVNGX_RESULT_INVALID_PARAMETER;
  params->Get(NVNGX_Parameter_ExposureTexture, &exposure);
  madeira_dlss_desc desc{};
  desc.size = sizeof(desc); desc.version = MADEIRA_DLSS_ABI_VERSION; desc.flags = feature->flags;
  desc.color = uint64_t(color); desc.output = uint64_t(output); desc.depth = uint64_t(depth);
  desc.motion = uint64_t(motion); desc.exposure = uint64_t(exposure);
  desc.input_width = feature->width; desc.input_height = feature->height;
  desc.output_width = feature->output_width; desc.output_height = feature->output_height;
  params->Get(NVNGX_Parameter_DLSS_Render_Subrect_Dimensions_Width, &desc.input_width);
  params->Get(NVNGX_Parameter_DLSS_Render_Subrect_Dimensions_Height, &desc.input_height);
  int reset = 0; params->Get(NVNGX_Parameter_Reset, &reset); desc.reset = !!reset;
  desc.motion_scale_x = desc.motion_scale_y = 1.0f; desc.pre_exposure = 1.0f;
  params->Get(NVNGX_Parameter_MV_Scale_X, &desc.motion_scale_x);
  params->Get(NVNGX_Parameter_MV_Scale_Y, &desc.motion_scale_y);
  params->Get(NVNGX_Parameter_Jitter_Offset_X, &desc.jitter_x);
  params->Get(NVNGX_Parameter_Jitter_Offset_Y, &desc.jitter_y);
  params->Get(NVNGX_Parameter_DLSS_Pre_Exposure, &desc.pre_exposure);
  if (desc.pre_exposure == 0.0f) desc.pre_exposure = 1.0f;
  auto result = feature->record(list, feature->native, &desc);
  if (result < 0) {
    static unsigned errors;
    if (errors++ < 4) Logger::warn(str::format("[dlss-metalfx] NGX D3D12 evaluate rejected HRESULT=", result));
    return result == int32_t(0x80070057) ? NVNGX_RESULT_INVALID_PARAMETER : NVNGX_RESULT_FEATURE_NOT_SUPPORTED;
  }
  return NVNGX_RESULT_OK;
}
NVNGX_API NVNGX_RESULT NVSDK_NGX_D3D12_ReleaseFeature(unsigned int *handle) {
  if (!handle) return NVNGX_RESULT_INVALID_PARAMETER;
  auto feature = reinterpret_cast<MadeiraDLSS12Feature *>(handle);
  if (feature->feature != NVNGX_FEATURE_SUPERSAMPLING || !feature->release) return NVNGX_RESULT_INVALID_PARAMETER;
  feature->release(feature->native); delete feature;
  return NVNGX_RESULT_OK;
}
} // namespace dxmt
