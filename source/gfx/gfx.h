#pragma once

#include "gfx_defines.h"
#include "gfx_device.h"
#include "gfx_buffer.h"
#include "gfx_texture.h"
#include "gfx_command_list.h"
#include "gfx_fence.h"
#include "gfx_shader.h"
#include "gfx_pipeline_state.h"
#include "gfx_swapchain.h"
#include "gfx_descriptor.h"
#include "gfx_heap.h"
#include "gfx_rt_blas.h"
#include "gfx_rt_tlas.h"

IGfxDevice* CreateGfxDevice(const GfxDeviceDesc& desc);
uint32_t GetFormatRowPitch(GfxFormat format, uint32_t width);
uint32_t GetFormatBlockWidth(GfxFormat format);
uint32_t GetFormatBlockHeight(GfxFormat format);
uint32_t GetFormatComponentNum(GfxFormat format);
bool IsDepthFormat(GfxFormat format);
bool IsStencilFormat(GfxFormat format);
bool IsSRGBFormat(GfxFormat format);
uint32_t CalcSubresource(const GfxTextureDesc& desc, uint32_t mip, uint32_t slice);
void DecomposeSubresource(const GfxTextureDesc& desc, uint32_t subresource, uint32_t& mip, uint32_t& slice);

class ScopedGpuEvent
{
public:
    ScopedGpuEvent(IGfxCommandList* pCommandList, const eastl::string& event_name, const eastl::string& file = "", const eastl::string& function = "", uint32_t line = 0) :
        m_pCommandList(pCommandList)
    {
        pCommandList->BeginEvent(event_name, file, function, line);
    }

    ~ScopedGpuEvent()
    {
        m_pCommandList->EndEvent();
    }

private:
    IGfxCommandList* m_pCommandList;
};