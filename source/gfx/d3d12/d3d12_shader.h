#pragma once

#include "d3d12_header.h"
#include "../gfx_shader.h"

class D3D12Device;

class D3D12Shader : public IGfxShader
{
public:
    D3D12Shader(D3D12Device* pDevice, const GfxShaderDesc& desc, const eastl::string& name);

    virtual void* GetHandle() const override { return nullptr; }
    virtual bool Create(eastl::span<uint8_t> data) override;

    D3D12_SHADER_BYTECODE GetByteCode() const { return { m_data.data(), m_data.size()}; }

private:
    eastl::vector<uint8_t> m_data;
};