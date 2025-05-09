#include "metal_buffer.h"
#include "metal_device.h"
#include "metal_heap.h"
#include "utils/log.h"

MetalBuffer::MetalBuffer(MetalDevice* pDevice, const GfxBufferDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_desc = desc;
    m_name = name;
}

MetalBuffer::~MetalBuffer()
{
    ((MetalDevice*)m_pDevice)->Evict(m_pBuffer);
    ((MetalDevice*)m_pDevice)->Release(m_pBuffer);
}

bool MetalBuffer::Create()
{
    if (m_desc.heap != nullptr)
    {
        RE_ASSERT(m_desc.alloc_type == GfxAllocationType::Placed);
        RE_ASSERT(m_desc.memory_type == m_desc.heap->GetDesc().memory_type);
        RE_ASSERT(m_desc.size + m_desc.heap_offset <= m_desc.heap->GetDesc().size);

        MTL::Heap* heap = (MTL::Heap*)m_desc.heap->GetHandle();
        m_pBuffer = heap->newBuffer(m_desc.size, ToResourceOptions(m_desc.memory_type), m_desc.heap_offset);
    }
    else
    {
        MTL::Device* device = (MTL::Device*)m_pDevice->GetHandle();
        m_pBuffer = device->newBuffer(m_desc.size, ToResourceOptions(m_desc.memory_type));
    }

    if(m_pBuffer == nullptr)
    {
        RE_ERROR("[MetalBuffer] failed to create {}", m_name);
        return false;
    }
    
    ((MetalDevice*)m_pDevice)->MakeResident(m_pBuffer);
    
    SetDebugLabel(m_pBuffer, m_name.c_str());
    
    if(m_desc.memory_type != GfxMemoryType::GpuOnly)
    {
        m_pCpuAddress = m_pBuffer->contents();
    }

    return true;
}

uint64_t MetalBuffer::GetGpuAddress()
{
    return m_pBuffer->gpuAddress();
}

uint32_t MetalBuffer::GetRequiredStagingBufferSize() const
{
    return m_desc.size;
}
