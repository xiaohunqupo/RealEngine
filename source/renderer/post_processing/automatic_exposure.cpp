#include "automatic_exposure.h"
#include "../renderer.h"
#include "utils/gui_util.h"

#define A_CPU
#include "ffx_a.h"
#include "ffx_spd.h"

AutomaticExposure::AutomaticExposure(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxComputePipelineDesc desc;
    desc.cs = pRenderer->GetShader("automatic_exposure.hlsl", "luminance_reduction", GfxShaderType::CS);
    m_pLuminanceReductionPSO = pRenderer->GetPipelineState(desc, "Luminance Reduction PSO");
    
    desc.cs = m_pRenderer->GetShader("automatic_exposure_histogram.hlsl", "histogram_reduction", GfxShaderType::CS);
    m_pHistogramReductionPSO = pRenderer->GetPipelineState(desc, "Histogram Reduction PSO");

    m_pPreviousEV100.reset(pRenderer->CreateTexture2D(1, 1, 1, GfxFormat::R16F, GfxTextureUsageUnorderedAccess, "AutomaticExposure::m_pPreviousEV100"));
}

void AutomaticExposure::OnGui()
{
    if (ImGui::CollapsingHeader("Auto Exposure"))
    {
        ImGui::Combo("Exposure Mode##Exposure", (int*)&m_exposuremode, "Automatic\0AutomaticHistogram\0Manual\0\0");
        ImGui::Combo("Metering Mode##Exposure", (int*)&m_meteringMode, "Average\0Spot\0CenterWeighted\00");
        ImGui::SliderFloat("Min Luminance##Exposure", &m_minLuminance, 0.0f, 1.0f, "%.3f");
        ImGui::SliderFloat("Max Luminance##Exposure", &m_maxLuminance, 0.3f, 10.0f, "%.3f");
        ImGui::SliderFloat("Eye Adaption Speed##Exposure", &m_adaptionSpeed, 0.01f, 5.0f, "%.2f");
        if (m_exposuremode == ExposureMode::AutomaticHistogram)
        {
            ImGui::SliderFloat("Low Percentile##Exposure", &m_histogramLowPercentile, 0.0f, 0.49f, "%.2f");
            ImGui::SliderFloat("High Percentile##Exposure", &m_histogramHighPercentile, 0.51f, 1.0f, "%.2f");
        }
        ImGui::Checkbox("Show EV100##Exposure", &m_bDebugEV100);
    }
}

RGHandle AutomaticExposure::AddPass(RenderGraph* pRenderGraph, RGHandle sceneColorRT, uint32_t width, uint32_t height)
{
    RENDER_GRAPH_EVENT(pRenderGraph, "AutomaticExposure");

    RGHandle avgLuminanceRT;

    if (m_exposuremode == ExposureMode::Automatic)
    {
        ComputeLuminanceSize(width, height);

        struct InitLuminanceData
        {
            RGHandle input;
            RGHandle output;
        };

        RGHandle luminanceRT;

        auto init_luminance_pass = pRenderGraph->AddPass<InitLuminanceData>("Init Luminance", RenderPassType::Compute,
            [&](InitLuminanceData& data, RGBuilder& builder)
            {
                data.input = builder.Read(sceneColorRT);

                RGTexture::Desc desc;
                desc.width = m_luminanceSize.x;
                desc.height = m_luminanceSize.y;
                desc.mip_levels = m_luminanceMips;
                desc.format = GfxFormat::RG16F;

                luminanceRT = builder.Create<RGTexture>(desc, "Average Luminance");
                data.output = builder.Write(luminanceRT);
            },
            [=](const InitLuminanceData& data, IGfxCommandList* pCommandList)
            {
                InitLuminance(pCommandList,
                    pRenderGraph->GetTexture(data.input), 
                    pRenderGraph->GetTexture(data.output));
            });

        struct LuminanceReductionData
        {
            RGHandle luminanceRT;
        };

        auto luminance_reduction_pass = pRenderGraph->AddPass<LuminanceReductionData>("Luminance Reduction", RenderPassType::Compute,
            [&](LuminanceReductionData& data, RGBuilder& builder)
            {
                data.luminanceRT = builder.Read(init_luminance_pass->output);

                for (uint32_t i = 1; i < m_luminanceMips; ++i)
                {
                    data.luminanceRT = builder.Write(luminanceRT, i);
                }
            },
            [=](const LuminanceReductionData& data, IGfxCommandList* pCommandList)
            {
                ReduceLuminance(pCommandList, pRenderGraph->GetTexture(data.luminanceRT));
            });

        avgLuminanceRT = luminance_reduction_pass->luminanceRT;
    }
    else if (m_exposuremode == ExposureMode::AutomaticHistogram)
    {
        struct BuildHistogramData
        {
            RGHandle inputTexture;
            RGHandle histogramBuffer;
        };

        auto build_histogram_pass = pRenderGraph->AddPass<BuildHistogramData>("Build Histogram", RenderPassType::Compute,
            [&](BuildHistogramData& data, RGBuilder& builder)
            {
                data.inputTexture = builder.Read(sceneColorRT);

                RGBuffer::Desc desc;
                desc.stride = 4;
                desc.size = desc.stride * 256; //256 bins
                desc.format = GfxFormat::R32F;
                desc.usage = GfxBufferUsageRawBuffer;
                data.histogramBuffer = builder.Create<RGBuffer>(desc, "Luminance Histogram");
                data.histogramBuffer = builder.Write(data.histogramBuffer);
            },
            [=](const BuildHistogramData& data, IGfxCommandList* pCommandList)
            {
                BuildHistogram(pCommandList, 
                    pRenderGraph->GetTexture(data.inputTexture), 
                    pRenderGraph->GetBuffer(data.histogramBuffer),
                    width, height);
            });

        struct HistogramReductionData
        {
            RGHandle histogramBuffer;
            RGHandle avgLuminanceTexture;
        };

        auto histogram_reduction_pass = pRenderGraph->AddPass<HistogramReductionData>("Histogram Reduction", RenderPassType::Compute,
            [&](HistogramReductionData& data, RGBuilder& builder)
            {
                data.histogramBuffer = builder.Read(build_histogram_pass->histogramBuffer);

                RGTexture::Desc desc;
                desc.width = desc.height = 1;
                desc.format = GfxFormat::R16F;
                data.avgLuminanceTexture = builder.Create<RGTexture>(desc, "Average Luminance");
                data.avgLuminanceTexture = builder.Write(data.avgLuminanceTexture);
            },
            [=](const HistogramReductionData& data, IGfxCommandList* pCommandList)
            {
                ReduceHistogram(pCommandList, 
                    pRenderGraph->GetBuffer(data.histogramBuffer),
                    pRenderGraph->GetTexture(data.avgLuminanceTexture));
            });

        avgLuminanceRT = histogram_reduction_pass->avgLuminanceTexture;
    }

    struct ExposureData
    {
        RGHandle avgLuminance;
        RGHandle exposure;
    };

    auto exposure_pass = pRenderGraph->AddPass<ExposureData>("Exposure", RenderPassType::Compute,
        [&](ExposureData& data, RGBuilder& builder)
        {
            if (avgLuminanceRT.IsValid())
            {
                uint32_t mip = m_exposuremode == ExposureMode::Automatic ? m_luminanceMips - 1 : 0;
                data.avgLuminance = builder.Read(avgLuminanceRT, mip);
            }

            RGTexture::Desc desc;
            desc.width = desc.height = 1;
            desc.format = GfxFormat::R16F;
            data.exposure = builder.Create<RGTexture>(desc, "Exposure");
            data.exposure = builder.Write(data.exposure);
        },
        [=](const ExposureData& data, IGfxCommandList* pCommandList)
        {
            Exposure(pCommandList, 
                pRenderGraph->GetTexture(data.avgLuminance),
                pRenderGraph->GetTexture(data.exposure));
        });

    return exposure_pass->exposure;
}

void AutomaticExposure::ComputeLuminanceSize(uint32_t width, uint32_t height)
{
    uint32_t mipsX = (uint32_t)eastl::max(ceilf(log2f((float)width)), 1.0f);
    uint32_t mipsY = (uint32_t)eastl::max(ceilf(log2f((float)height)), 1.0f);

    m_luminanceMips = eastl::max(mipsX, mipsY);
    RE_ASSERT(m_luminanceMips <= 13); //spd limit

    m_luminanceSize.x = 1 << (mipsX - 1);
    m_luminanceSize.y = 1 << (mipsY - 1);
}

void AutomaticExposure::InitLuminance(IGfxCommandList* pCommandList, RGTexture* input, RGTexture* output)
{
    eastl::vector<eastl::string> defines;

    switch (m_meteringMode)
    {
    case MeteringMode::Average:
        defines.push_back("METERING_MODE_AVERAGE=1");
        break;
    case MeteringMode::Spot:
        defines.push_back("METERING_MODE_SPOT=1");
        break;
    case MeteringMode::CenterWeighted:
        defines.push_back("METERING_MODE_CENTER_WEIGHTED=1");
        break;
    default:
        RE_ASSERT(false);
        break;
    }

    GfxComputePipelineDesc desc;
    desc.cs = m_pRenderer->GetShader("automatic_exposure.hlsl", "init_luminance", GfxShaderType::CS, defines);
    IGfxPipelineState* pso = m_pRenderer->GetPipelineState(desc, "Init Luminance PSO");

    pCommandList->SetPipelineState(pso);

    struct Constants
    {
        uint input;
        uint output;
        float width;
        float height;
        float rcpWidth;
        float rcpHeight;
        float minLuminance;
        float maxLuminance;
    };

    Constants constants = { 
        input->GetSRV()->GetHeapIndex(), output->GetUAV()->GetHeapIndex(),
        (float)m_luminanceSize.x, (float)m_luminanceSize.y,
        1.0f / m_luminanceSize.x, 1.0f / m_luminanceSize.y, 
        m_minLuminance, m_maxLuminance };
    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));

    pCommandList->Dispatch(DivideRoudingUp(m_luminanceSize.x, 8), DivideRoudingUp(m_luminanceSize.y, 8), 1);
}

void AutomaticExposure::ReduceLuminance(IGfxCommandList* pCommandList, RGTexture* texture)
{
    pCommandList->SetPipelineState(m_pLuminanceReductionPSO);

    varAU2(dispatchThreadGroupCountXY);
    varAU2(workGroupOffset); // needed if Left and Top are not 0,0
    varAU2(numWorkGroupsAndMips);
    varAU4(rectInfo) = initAU4(0, 0, m_luminanceSize.x, m_luminanceSize.y); // left, top, width, height
    SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo, m_luminanceMips - 1);

    struct spdConstants
    {
        uint mips;
        uint numWorkGroups;
        uint2 workGroupOffset;

        float2 invInputSize;
        uint c_imgSrc;
        uint c_spdGlobalAtomicUAV;

        //hlsl packing rules : every element in an array is stored in a four-component vector
        uint4 c_imgDst[12]; // do no access MIP [5]
    };

    spdConstants constants = {};
    constants.numWorkGroups = numWorkGroupsAndMips[0];
    constants.mips = numWorkGroupsAndMips[1];
    constants.workGroupOffset[0] = workGroupOffset[0];
    constants.workGroupOffset[1] = workGroupOffset[1];
    constants.invInputSize[0] = 1.0f / m_luminanceSize.x;
    constants.invInputSize[1] = 1.0f / m_luminanceSize.y;

    constants.c_imgSrc = texture->GetSRV()->GetHeapIndex();
    constants.c_spdGlobalAtomicUAV = m_pRenderer->GetSPDCounterBuffer()->GetUAV()->GetHeapIndex();

    for (uint32_t i = 0; i < m_luminanceMips - 1; ++i)
    {
        constants.c_imgDst[i].x = texture->GetUAV(i + 1, 0)->GetHeapIndex();
    }

    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));

    uint32_t dispatchX = dispatchThreadGroupCountXY[0];
    uint32_t dispatchY = dispatchThreadGroupCountXY[1];
    uint32_t dispatchZ = 1; //array slice
    pCommandList->Dispatch(dispatchX, dispatchY, dispatchZ);

    for (uint32_t i = 1; i < m_luminanceMips - 1; ++i)
    {
        //todo : currently RG doesn't hanlde subresource last usage properly, this fixed validation warning
        pCommandList->TextureBarrier(texture->GetTexture(), i, GfxAccessComputeUAV, GfxAccessComputeSRV);
    }
}

void AutomaticExposure::BuildHistogram(IGfxCommandList* pCommandList, RGTexture* inputTexture, RGBuffer* histogramBuffer, uint32_t width, uint32_t height)
{
    uint32_t clear_value[4] = { 0, 0, 0, 0 };
    pCommandList->ClearUAV(histogramBuffer->GetBuffer(), histogramBuffer->GetUAV(), clear_value);
    pCommandList->BufferBarrier(histogramBuffer->GetBuffer(), GfxAccessClearUAV, GfxAccessComputeUAV);

    eastl::vector<eastl::string> defines;

    switch (m_meteringMode)
    {
    case MeteringMode::Average:
        defines.push_back("METERING_MODE_AVERAGE=1");
        break;
    case MeteringMode::Spot:
        defines.push_back("METERING_MODE_SPOT=1");
        break;
    case MeteringMode::CenterWeighted:
        defines.push_back("METERING_MODE_CENTER_WEIGHTED=1");
        break;
    default:
        RE_ASSERT(false);
        break;
    }

    GfxComputePipelineDesc desc;
    desc.cs = m_pRenderer->GetShader("automatic_exposure_histogram.hlsl", "build_histogram", GfxShaderType::CS, defines);
    IGfxPipelineState* pso = m_pRenderer->GetPipelineState(desc, "Build Histogram PSO");

    pCommandList->SetPipelineState(pso);

    uint32_t half_width = (width + 1) / 2;
    uint32_t half_height = (height + 1) / 2;

    struct BuildHistogramConstants
    {
        uint inputTextureSRV;
        uint histogramBufferUAV;
        uint width;
        uint height;
        float rcpWidth;
        float rcpHeight;
        float minLuminance;
        float maxLuminance;
    };

    BuildHistogramConstants constants = {
        inputTexture->GetSRV()->GetHeapIndex(), histogramBuffer->GetUAV()->GetHeapIndex(),
        half_width, half_height, 1.0f / half_width, 1.0f / half_height,
        m_minLuminance, m_maxLuminance
    };
    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));

    pCommandList->Dispatch(DivideRoudingUp(half_width, 16), DivideRoudingUp(half_height, 16), 1);
}

void AutomaticExposure::ReduceHistogram(IGfxCommandList* pCommandList, RGBuffer* histogramBufferSRV, RGTexture* avgLuminanceUAV)
{
    pCommandList->SetPipelineState(m_pHistogramReductionPSO);

    struct ReduceHistogramConstants
    {
        uint histogramBufferSRV;
        uint avgLuminanceTextureUAV;
        float minLuminance;
        float maxLuminance;
        float lowPercentile;
        float highPercentile;
    };

    ReduceHistogramConstants constants = { 
        histogramBufferSRV->GetSRV()->GetHeapIndex(), avgLuminanceUAV->GetUAV()->GetHeapIndex(),
        m_minLuminance, m_maxLuminance, m_histogramLowPercentile, m_histogramHighPercentile 
    };
    pCommandList->SetComputeConstants(1, &constants, sizeof(constants));

    pCommandList->Dispatch(1, 1, 1);
}

void AutomaticExposure::Exposure(IGfxCommandList* pCommandList, RGTexture* avgLuminance, RGTexture* output)
{
    if (m_bHistoryInvalid)
    {
        m_bHistoryInvalid = false;

        float clear_value[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        pCommandList->ClearUAV(m_pPreviousEV100->GetTexture(), m_pPreviousEV100->GetUAV(), clear_value);
        pCommandList->TextureBarrier(m_pPreviousEV100->GetTexture(), 0, GfxAccessClearUAV, GfxAccessComputeUAV);
    }

    eastl::vector<eastl::string> defines;

    switch (m_exposuremode)
    {
    case ExposureMode::Automatic:
        defines.push_back("EXPOSURE_MODE_AUTO=1");
        break;
    case ExposureMode::AutomaticHistogram:
        defines.push_back("EXPOSURE_MODE_AUTO_HISTOGRAM=1");
        break;
    case ExposureMode::Manual:
        defines.push_back("EXPOSURE_MODE_MANUAL=1");
        break;
    default:
        RE_ASSERT(false);
        break;
    }

    if (m_bDebugEV100)
    {
        defines.push_back("DEBUG_SHOW_EV100=1");
    }

    GfxComputePipelineDesc desc;
    desc.cs = m_pRenderer->GetShader("automatic_exposure.hlsl", "exposure", GfxShaderType::CS, defines);
    IGfxPipelineState* pso = m_pRenderer->GetPipelineState(desc, "Exposure PSO");

    pCommandList->SetPipelineState(pso);

    struct ExpsureConstants
    {
        uint avgLuminanceTexture;
        uint avgLuminanceMip;
        uint exposureTexture;
        uint previousEV100Texture;
        float adaptionSpeed;
    };

    ExpsureConstants root_constants = {
        avgLuminance ? avgLuminance->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE,
        m_exposuremode == ExposureMode::Automatic ? m_luminanceMips - 1 : 0,
        output->GetUAV()->GetHeapIndex(),
        m_pPreviousEV100->GetUAV()->GetHeapIndex(),
        m_adaptionSpeed
    };

    pCommandList->SetComputeConstants(0, &root_constants, sizeof(root_constants));
    pCommandList->Dispatch(1, 1, 1);
}
