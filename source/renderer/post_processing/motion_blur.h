#pragma once

#include "../render_graph.h"

class MotionBlur
{
public:
    MotionBlur(Renderer* pRenderer);

    RGHandle Render(RenderGraph* pRenderGraph, RGHandle sceneColor, RGHandle sceneDepth, RGHandle velocity, uint32_t width, uint32_t height);

private:
    void PackVelocityDepth(IGfxCommandList* pCommandList, RGTexture* velocity, RGTexture* depth, RGTexture* output);
    void TileMax(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* output, bool vertical_pass);
    void NeighborMax(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* output);
    void ReconstructionFilter(IGfxCommandList* pCommandList, RGTexture* color, RGTexture* velocityDepth, RGTexture* neighborMax, RGTexture* output);

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pPackVelocityPSO = nullptr;
    IGfxPipelineState* m_pTileMaxXPSO = nullptr;
    IGfxPipelineState* m_pTileMaxYPSO = nullptr;
    IGfxPipelineState* m_pNeighborMaxPSO = nullptr;
    IGfxPipelineState* m_pReconstructionPSO = nullptr;

    bool m_bEnable = true;
    uint32_t m_sampleCount = 16;
};