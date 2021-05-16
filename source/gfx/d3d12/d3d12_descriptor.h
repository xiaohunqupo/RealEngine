#pragma once

#include "d3d12_header.h"
#include "../i_gfx_descriptor.h"
#include "../i_gfx_buffer.h"

class D3D12Device;

class D3D12ShaderResourceView : public IGfxDescriptor
{
public:
    D3D12ShaderResourceView(D3D12Device* pDevice, IGfxResource* pResource, const GfxShaderResourceViewDesc& desc, const std::string& name);
    ~D3D12ShaderResourceView();

    virtual void* GetHandle() const override { return m_pResource->GetHandle(); }
    virtual uint32_t GetHeapIndex() const override { return m_descriptor.index; }

    bool Create();
private:
    IGfxResource* m_pResource = nullptr;
    GfxShaderResourceViewDesc m_desc = {};
    D3D12Descriptor m_descriptor;
};

class D3D12UnorderedAccessView : public IGfxDescriptor
{
public:
    D3D12UnorderedAccessView(D3D12Device* pDevice, IGfxResource* pResource, const GfxUnorderedAccessViewDesc& desc, const std::string& name);
    ~D3D12UnorderedAccessView();

    virtual void* GetHandle() const override { return m_pResource->GetHandle(); }
    virtual uint32_t GetHeapIndex() const override { return m_descriptor.index; }

    bool Create();
private:
    IGfxResource* m_pResource = nullptr;
    GfxUnorderedAccessViewDesc m_desc = {};
    D3D12Descriptor m_descriptor;
};

class D3D12ConstantBufferView : public IGfxDescriptor
{
public:
public:
    D3D12ConstantBufferView(D3D12Device* pDevice, IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const std::string& name);
    ~D3D12ConstantBufferView();

    virtual void* GetHandle() const override { return m_pBuffer->GetHandle(); }
    virtual uint32_t GetHeapIndex() const override { return m_descriptor.index; }

    bool Create();
private:
    IGfxBuffer* m_pBuffer = nullptr;
    GfxConstantBufferViewDesc m_desc = {};
    D3D12Descriptor m_descriptor;
};

class D3D12Sampler : public IGfxDescriptor
{
public:
    D3D12Sampler(D3D12Device* pDevice, const GfxSamplerDesc& desc, const std::string& name);
    ~D3D12Sampler();

    virtual void* GetHandle() const override { return nullptr; }
    virtual uint32_t GetHeapIndex() const override { return m_descriptor.index; }

    bool Create();
private:
    GfxSamplerDesc m_desc;
    D3D12Descriptor m_descriptor;
};