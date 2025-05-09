#include "gfx.h"
#include "core/platform.h"
#include "vulkan/vulkan_device.h"
#include "mock/mock_device.h"
#include "utils/assert.h"
#include "xxHash/xxhash.h"

#if RE_PLATFORM_WINDOWS
#include "d3d12/d3d12_device.h"
#endif

#if RE_PLATFORM_MAC || RE_PLATFORM_IOS
#include "metal/metal_device.h"
#endif

IGfxDevice* CreateGfxDevice(const GfxDeviceDesc& desc)
{
    IGfxDevice* pDevice = nullptr;

    switch (desc.backend)
    {
#if RE_PLATFORM_WINDOWS
    case GfxRenderBackend::D3D12:
        pDevice = new D3D12Device(desc);
        break;
#endif
    case GfxRenderBackend::Vulkan:
        pDevice = new VulkanDevice(desc);
        break;
#if RE_PLATFORM_MAC || RE_PLATFORM_IOS
    case GfxRenderBackend::Metal:
        pDevice = new MetalDevice(desc);
        break;
#endif
    case GfxRenderBackend::Mock:
        pDevice = new MockDevice(desc);
        break;
    default:
        break;
    }

    if (pDevice && !pDevice->Create())
    {
        delete pDevice;
        pDevice = nullptr;
    }
    
    return pDevice;
}

uint32_t GetFormatRowPitch(GfxFormat format, uint32_t width)
{
    switch (format)
    {
    case GfxFormat::RGBA32F:
    case GfxFormat::RGBA32UI:
    case GfxFormat::RGBA32SI:
        return width * 16;
    case GfxFormat::RGB32F:
    case GfxFormat::RGB32UI:
    case GfxFormat::RGB32SI:
        return width * 12;
    case GfxFormat::RGBA16F:
    case GfxFormat::RGBA16UI:
    case GfxFormat::RGBA16SI:
    case GfxFormat::RGBA16UNORM:
    case GfxFormat::RGBA16SNORM:
        return width * 8;
    case GfxFormat::RGBA8UI:
    case GfxFormat::RGBA8SI:
    case GfxFormat::RGBA8UNORM:
    case GfxFormat::RGBA8SNORM:
    case GfxFormat::RGBA8SRGB:
    case GfxFormat::BGRA8UNORM:
    case GfxFormat::BGRA8SRGB:
    case GfxFormat::RGB10A2UNORM:
    case GfxFormat::R11G11B10F:
    case GfxFormat::RGB9E5:
        return width * 4;
    case GfxFormat::RG32F:
    case GfxFormat::RG32UI:
    case GfxFormat::RG32SI:
        return width * 8;
    case GfxFormat::RG16F:
    case GfxFormat::RG16UI:
    case GfxFormat::RG16SI:
    case GfxFormat::RG16UNORM:
    case GfxFormat::RG16SNORM:
        return width * 4;
    case GfxFormat::RG8UI:
    case GfxFormat::RG8SI:
    case GfxFormat::RG8UNORM:
    case GfxFormat::RG8SNORM:
        return width * 2;
    case GfxFormat::R32F:
    case GfxFormat::R32UI:
    case GfxFormat::R32SI:
        return width * 4;
    case GfxFormat::R16F:
    case GfxFormat::R16UI:
    case GfxFormat::R16SI:
    case GfxFormat::R16UNORM:
    case GfxFormat::R16SNORM:
        return width * 2;
    case GfxFormat::R8UI:
    case GfxFormat::R8SI:
    case GfxFormat::R8UNORM:
    case GfxFormat::R8SNORM:
        return width;
    case GfxFormat::BC1UNORM:
    case GfxFormat::BC1SRGB:
    case GfxFormat::BC4UNORM:
    case GfxFormat::BC4SNORM:
        return width / 2;
    case GfxFormat::BC2UNORM:
    case GfxFormat::BC2SRGB:
    case GfxFormat::BC3UNORM:
    case GfxFormat::BC3SRGB:
    case GfxFormat::BC5UNORM:
    case GfxFormat::BC5SNORM:
    case GfxFormat::BC6U16F:
    case GfxFormat::BC6S16F:
    case GfxFormat::BC7UNORM:
    case GfxFormat::BC7SRGB:
        return width;
    default:
        RE_ASSERT(false);
        return 0;
    }
}

uint32_t GetFormatBlockWidth(GfxFormat format)
{
    if (format >= GfxFormat::BC1UNORM && format <= GfxFormat::BC7SRGB)
    {
        return 4;
    }

    return 1;
}

uint32_t GetFormatBlockHeight(GfxFormat format)
{
    if (format >= GfxFormat::BC1UNORM && format <= GfxFormat::BC7SRGB)
    {
        return 4;
    }

    return 1;
}

uint32_t GetFormatComponentNum(GfxFormat format)
{
    switch (format)
    {
    case GfxFormat::RGBA32F:
    case GfxFormat::RGBA32UI:
    case GfxFormat::RGBA32SI:
    case GfxFormat::RGBA16F:
    case GfxFormat::RGBA16UI:
    case GfxFormat::RGBA16SI:
    case GfxFormat::RGBA16UNORM:
    case GfxFormat::RGBA16SNORM:
    case GfxFormat::RGBA8UI:
    case GfxFormat::RGBA8SI:
    case GfxFormat::RGBA8UNORM:
    case GfxFormat::RGBA8SNORM:
    case GfxFormat::RGBA8SRGB:
    case GfxFormat::BGRA8UNORM:
    case GfxFormat::BGRA8SRGB:
    case GfxFormat::RGB10A2UI:
    case GfxFormat::RGB10A2UNORM:
        return 4;
    case GfxFormat::RGB32F:
    case GfxFormat::RGB32UI:
    case GfxFormat::RGB32SI:
    case GfxFormat::R11G11B10F:
    case GfxFormat::RGB9E5:
        return 3;
    case GfxFormat::RG32F:
    case GfxFormat::RG32UI:
    case GfxFormat::RG32SI:
    case GfxFormat::RG16F:
    case GfxFormat::RG16UI:
    case GfxFormat::RG16SI:
    case GfxFormat::RG16UNORM:
    case GfxFormat::RG16SNORM:
    case GfxFormat::RG8UI:
    case GfxFormat::RG8SI:
    case GfxFormat::RG8UNORM:
    case GfxFormat::RG8SNORM:
        return 2;
    case GfxFormat::R32F:
    case GfxFormat::R32UI:
    case GfxFormat::R32SI:
    case GfxFormat::R16F:
    case GfxFormat::R16UI:
    case GfxFormat::R16SI:
    case GfxFormat::R16UNORM:
    case GfxFormat::R16SNORM:
    case GfxFormat::R8UI:
    case GfxFormat::R8SI:
    case GfxFormat::R8UNORM:
    case GfxFormat::R8SNORM:
        return 1;
    default:
        RE_ASSERT(false);
        return 0;
    }
}

bool IsDepthFormat(GfxFormat format)
{
    return format == GfxFormat::D32FS8 || format == GfxFormat::D32F || format == GfxFormat::D16;
}

bool IsStencilFormat(GfxFormat format)
{
    return format == GfxFormat::D32FS8;
}

bool IsSRGBFormat(GfxFormat format)
{
    return format == GfxFormat::RGBA8SRGB || format == GfxFormat::BGRA8SRGB;
}

uint32_t CalcSubresource(const GfxTextureDesc& desc, uint32_t mip, uint32_t slice)
{
    return mip + desc.mip_levels * slice;
}

void DecomposeSubresource(const GfxTextureDesc& desc, uint32_t subresource, uint32_t& mip, uint32_t& slice)
{
    mip = subresource % desc.mip_levels;
    slice = (subresource / desc.mip_levels) % desc.array_size;
}
