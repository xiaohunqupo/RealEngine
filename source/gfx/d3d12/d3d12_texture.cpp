#include "d3d12_texture.h"
#include "d3d12_device.h"
#include "d3d12_heap.h"
#include "d3d12ma/D3D12MemAlloc.h"
#include "../gfx.h"
#include "utils/assert.h"
#include "utils/log.h"

D3D12Texture::D3D12Texture(D3D12Device* pDevice, const GfxTextureDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

D3D12Texture::~D3D12Texture()
{
    D3D12Device* pDevice = (D3D12Device*)m_pDevice;
    pDevice->Delete(m_pTexture);
    pDevice->Delete(m_pAllocation);

    for (size_t i = 0; i < m_RTV.size(); ++i)
    {
        pDevice->DeleteRTV(m_RTV[i]);
    }

    for (size_t i = 0; i < m_DSV.size(); ++i)
    {
        pDevice->DeleteDSV(m_DSV[i]);
    }
}

bool D3D12Texture::Create()
{
    ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();

    D3D12MA::Allocator* pAllocator = ((D3D12Device*)m_pDevice)->GetResourceAllocator();

    D3D12_RESOURCE_DESC resourceDesc = d3d12_resource_desc(m_desc);
    D3D12_RESOURCE_DESC1 resourceDesc1 = CD3DX12_RESOURCE_DESC1(resourceDesc);

    D3D12_BARRIER_LAYOUT initial_layout;

    if (m_desc.usage & GfxTextureUsageRenderTarget)
    {
        initial_layout = D3D12_BARRIER_LAYOUT_RENDER_TARGET;
    }
    else if (m_desc.usage & GfxTextureUsageDepthStencil)
    {
        initial_layout = D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
    }
    else if (m_desc.usage & GfxTextureUsageUnorderedAccess)
    {
        initial_layout = D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
    }
    else
    {
        initial_layout = D3D12_BARRIER_LAYOUT_COMMON;
    }

    HRESULT hr;
    
    if (m_desc.heap != nullptr)
    {
        RE_ASSERT(m_desc.alloc_type == GfxAllocationType::Placed);
        RE_ASSERT(m_desc.memory_type == m_desc.heap->GetDesc().memory_type);

        hr = pAllocator->CreateAliasingResource2((D3D12MA::Allocation*)m_desc.heap->GetHandle(), m_desc.heap_offset,
            &resourceDesc1, initial_layout, nullptr, 0, nullptr, IID_PPV_ARGS(&m_pTexture));
    }
    else if (m_desc.alloc_type == GfxAllocationType::Sparse)
    {
        ID3D12Device10* device = (ID3D12Device10*)m_pDevice->GetHandle();
        hr = device->CreateReservedResource2(&resourceDesc, initial_layout, nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(&m_pTexture));
    }
    else
    {
        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = d3d12_heap_type(m_desc.memory_type);
        allocationDesc.Flags = m_desc.alloc_type == GfxAllocationType::Committed ? D3D12MA::ALLOCATION_FLAG_COMMITTED : D3D12MA::ALLOCATION_FLAG_NONE;

        if (m_desc.usage & GfxTextureUsageShared)
        {
            resourceDesc1.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resourceDesc1.Flags |= D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
            allocationDesc.ExtraHeapFlags |= D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER;
        }

        hr = pAllocator->CreateResource3(&allocationDesc, 
            &resourceDesc1, initial_layout, nullptr, 0, nullptr, &m_pAllocation, IID_PPV_ARGS(&m_pTexture));
    }

    if (FAILED(hr))
    {
        RE_ERROR("[D3D12Texture] failed to create {}", m_name);
        return false;
    }

    eastl::wstring name_wstr = string_to_wstring(m_name);
    m_pTexture->SetName(name_wstr.c_str());
    if (m_pAllocation)
    {
        m_pAllocation->SetName(name_wstr.c_str());
    }

    if (m_desc.usage & GfxTextureUsageShared)
    {
        ID3D12Device* device = (ID3D12Device*)m_pDevice->GetHandle();
        hr = device->CreateSharedHandle(m_pTexture, nullptr, GENERIC_ALL, name_wstr.c_str(), &m_sharedHandle);

        if (FAILED(hr))
        {
            RE_ERROR("[D3D12Texture] failed to create shared handle for {}", m_name);
            return false;
        }
    }

    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Texture::GetRTV(uint32_t mip_slice, uint32_t array_slice)
{
    RE_ASSERT(m_desc.usage & GfxTextureUsageRenderTarget);

    if (m_RTV.empty())
    {
        m_RTV.resize(m_desc.mip_levels * m_desc.array_size);
    }

    uint32_t index = m_desc.mip_levels * array_slice + mip_slice;
    if (IsNullDescriptor(m_RTV[index]))
    {
        m_RTV[index] = ((D3D12Device*)m_pDevice)->AllocateRTV();

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = dxgi_format(m_desc.format);

        switch (m_desc.type)
        {
        case GfxTextureType::Texture2D:
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = mip_slice;
            break;
        case GfxTextureType::Texture2DArray:
        case GfxTextureType::TextureCube:
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice = mip_slice;
            rtvDesc.Texture2DArray.FirstArraySlice = array_slice;
            rtvDesc.Texture2DArray.ArraySize = 1;
            break;
        default:
            RE_ASSERT(false);
            break;
        }

        ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();
        pDevice->CreateRenderTargetView(m_pTexture, &rtvDesc, m_RTV[index].cpu_handle);
    }

    return m_RTV[index].cpu_handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Texture::GetDSV(uint32_t mip_slice, uint32_t array_slice)
{
    RE_ASSERT(m_desc.usage & GfxTextureUsageDepthStencil);

    if (m_DSV.empty())
    {
        m_DSV.resize(m_desc.mip_levels * m_desc.array_size);
    }

    uint32_t index = m_desc.mip_levels * array_slice + mip_slice;
    if (IsNullDescriptor(m_DSV[index]))
    {
        m_DSV[index] = ((D3D12Device*)m_pDevice)->AllocateDSV();

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = dxgi_format(m_desc.format);

        switch (m_desc.type)
        {
        case GfxTextureType::Texture2D:
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = mip_slice;
            break;
        case GfxTextureType::Texture2DArray:
        case GfxTextureType::TextureCube:
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice = mip_slice;
            dsvDesc.Texture2DArray.FirstArraySlice = array_slice;
            dsvDesc.Texture2DArray.ArraySize = 1;
            break;
        default:
            RE_ASSERT(false);
            break;
        }

        ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();
        pDevice->CreateDepthStencilView(m_pTexture, &dsvDesc, m_DSV[index].cpu_handle);
    }

    return m_DSV[index].cpu_handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Texture::GetReadOnlyDSV(uint32_t mip_slice, uint32_t array_slice)
{
    RE_ASSERT(m_desc.usage & GfxTextureUsageDepthStencil);

    if (m_readonlyDSV.empty())
    {
        m_readonlyDSV.resize(m_desc.mip_levels * m_desc.array_size);
    }

    uint32_t index = m_desc.mip_levels * array_slice + mip_slice;
    if (IsNullDescriptor(m_readonlyDSV[index]))
    {
        m_readonlyDSV[index] = ((D3D12Device*)m_pDevice)->AllocateDSV();

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = dxgi_format(m_desc.format);
        dsvDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
        if (IsStencilFormat(m_desc.format))
        {
            dsvDesc.Flags |= D3D12_DSV_FLAG_READ_ONLY_STENCIL;
        }

        switch (m_desc.type)
        {
        case GfxTextureType::Texture2D:
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = mip_slice;
            break;
        case GfxTextureType::Texture2DArray:
        case GfxTextureType::TextureCube:
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice = mip_slice;
            dsvDesc.Texture2DArray.FirstArraySlice = array_slice;
            dsvDesc.Texture2DArray.ArraySize = 1;
            break;
        default:
            RE_ASSERT(false);
            break;
        }

        ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();
        pDevice->CreateDepthStencilView(m_pTexture, &dsvDesc, m_readonlyDSV[index].cpu_handle);
    }

    return m_readonlyDSV[index].cpu_handle;
}

uint32_t D3D12Texture::GetRequiredStagingBufferSize() const
{
    ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();

    D3D12_RESOURCE_DESC desc = m_pTexture->GetDesc();
    uint32_t subresource_count = m_desc.mip_levels * m_desc.array_size;

    uint64_t size;
    pDevice->GetCopyableFootprints(&desc, 0, subresource_count, 0, nullptr, nullptr, nullptr, &size);
    return (uint32_t)size;
}

uint32_t D3D12Texture::GetRowPitch(uint32_t mip_level) const
{
    RE_ASSERT(mip_level < m_desc.mip_levels);

    ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();

    D3D12_RESOURCE_DESC desc = m_pTexture->GetDesc();

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    pDevice->GetCopyableFootprints(&desc, mip_level, 1, 0, &footprint, nullptr, nullptr, nullptr);
    return footprint.Footprint.RowPitch;
}

GfxTilingDesc D3D12Texture::GetTilingDesc() const
{
    RE_ASSERT(m_desc.alloc_type == GfxAllocationType::Sparse);

    ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();

    UINT tile_count;
    D3D12_PACKED_MIP_INFO packedMipInfo;
    D3D12_TILE_SHAPE tileShape;
    pDevice->GetResourceTiling(m_pTexture, &tile_count, &packedMipInfo, &tileShape, nullptr, 0, nullptr);

    GfxTilingDesc info;
    info.tile_count = tile_count;
    info.standard_mips = packedMipInfo.NumStandardMips;
    info.tile_width = tileShape.WidthInTexels;
    info.tile_height = tileShape.HeightInTexels;
    info.tile_depth = tileShape.DepthInTexels;
    info.packed_mips = packedMipInfo.NumPackedMips;
    info.packed_mip_tiles = packedMipInfo.NumTilesForPackedMips;

    return info;
}

GfxSubresourceTilingDesc D3D12Texture::GetTilingDesc(uint32_t subresource) const
{
    RE_ASSERT(m_desc.alloc_type == GfxAllocationType::Sparse);

    ID3D12Device* pDevice = (ID3D12Device*)m_pDevice->GetHandle();

    uint32_t subresource_count = 1;
    D3D12_SUBRESOURCE_TILING tiling;
    pDevice->GetResourceTiling(m_pTexture, nullptr, nullptr, nullptr, &subresource_count, subresource, &tiling);

    GfxSubresourceTilingDesc info;
    info.width = tiling.WidthInTiles;
    info.height = tiling.HeightInTiles;
    info.depth = tiling.DepthInTiles;
    info.tile_offset = tiling.StartTileIndexInOverallResource;

    return info;
}
