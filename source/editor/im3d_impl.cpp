#include "im3d_impl.h"
#include "core/engine.h"
#include "utils/profiler.h"
#include "im3d/im3d.h"
#include "imgui/imgui.h"

Im3dImpl::Im3dImpl(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
}

Im3dImpl::~Im3dImpl()
{
}

bool Im3dImpl::Init()
{
    GfxMeshShadingPipelineDesc desc;
    desc.rasterizer_state.cull_mode = GfxCullMode::None;
    desc.depthstencil_state.depth_write = false;
    desc.depthstencil_state.depth_test = true;
    desc.depthstencil_state.depth_func = GfxCompareFunc::Greater;
    desc.blend_state[0].blend_enable = true;
    desc.blend_state[0].color_src = GfxBlendFactor::SrcAlpha;
    desc.blend_state[0].color_dst = GfxBlendFactor::InvSrcAlpha;
    desc.blend_state[0].alpha_src = GfxBlendFactor::One;
    desc.blend_state[0].alpha_dst = GfxBlendFactor::InvSrcAlpha;
    desc.rt_format[0] = m_pRenderer->GetSwapchain()->GetDesc().backbuffer_format;
    desc.depthstencil_format = GfxFormat::D32F;

    desc.ms = m_pRenderer->GetShader("im3d.hlsl", "ms_main", GfxShaderType::MS, { "POINTS=1" });
    desc.ps = m_pRenderer->GetShader("im3d.hlsl", "ps_main", GfxShaderType::PS, { "POINTS=1" });
    m_pPointPSO = m_pRenderer->GetPipelineState(desc, "Im3d Points PSO");

    desc.ms = m_pRenderer->GetShader("im3d.hlsl", "ms_main", GfxShaderType::MS, { "LINES=1" });
    desc.ps = m_pRenderer->GetShader("im3d.hlsl", "ps_main", GfxShaderType::PS, { "LINES=1" });
    m_pLinePSO = m_pRenderer->GetPipelineState(desc, "Im3d Lines PSO");

    desc.ms = m_pRenderer->GetShader("im3d.hlsl", "ms_main", GfxShaderType::MS, { "TRIANGLES=1" });
    desc.ps = m_pRenderer->GetShader("im3d.hlsl", "ps_main", GfxShaderType::PS, { "TRIANGLES=1" });
    m_pTrianglePSO = m_pRenderer->GetPipelineState(desc, "Im3d Triangles PSO");

    return true;
}

void Im3dImpl::NewFrame()
{
    Im3d::AppData& appData = Im3d::GetAppData();
    Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();

    appData.m_deltaTime = Engine::GetInstance()->GetFrameDeltaTime();
    appData.m_viewportSize = Im3d::Vec2((float)m_pRenderer->GetDisplayWidth(), (float)m_pRenderer->GetDisplayHeight());
    appData.m_viewOrigin = camera->GetPosition();
    appData.m_viewDirection = camera->GetForward();
    appData.m_worldUp = Im3d::Vec3(0.0f, 1.0f, 0.0f);
    appData.m_projScaleY = tanf(radians(camera->GetFov()) * 0.5f) * 2.0f;

    Im3d::Vec2 cursorPos(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
    cursorPos.x = (cursorPos.x / appData.m_viewportSize.x) * 2.0f - 1.0f;
    cursorPos.y = (cursorPos.y / appData.m_viewportSize.y) * 2.0f - 1.0f;
    cursorPos.y = -cursorPos.y;

    float3 rayOrigin, rayDirection;
    rayOrigin = appData.m_viewOrigin;
    rayDirection.x = cursorPos.x / camera->GetNonJitterProjectionMatrix()[0][0];
    rayDirection.y = cursorPos.y / camera->GetNonJitterPrevProjectionMatrix()[1][1];
    rayDirection.z = -1.0f;
    rayDirection = mul(camera->GetWorldMatrix(), float4(normalize(rayDirection), 0.0f)).xyz();

    appData.m_cursorRayOrigin = rayOrigin;
    appData.m_cursorRayDirection = rayDirection;

    bool ctrlDown = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
    appData.m_keyDown[Im3d::Key_L] = ctrlDown && ImGui::IsKeyDown(ImGuiKey_L);
    appData.m_keyDown[Im3d::Key_T] = ctrlDown && ImGui::IsKeyDown(ImGuiKey_T);
    appData.m_keyDown[Im3d::Key_R] = ctrlDown && ImGui::IsKeyDown(ImGuiKey_R);
    appData.m_keyDown[Im3d::Key_S] = ctrlDown && ImGui::IsKeyDown(ImGuiKey_S);
    appData.m_keyDown[Im3d::Mouse_Left] = ImGui::IsKeyDown(ImGuiKey_MouseLeft);

    appData.m_snapTranslation = ctrlDown ? 0.1f : 0.0f;
    appData.m_snapRotation = ctrlDown ? radians(30.0f) : 0.0f;
    appData.m_snapScale = ctrlDown ? 0.5f : 0.0f;

    Im3d::NewFrame();
}

void Im3dImpl::Render(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "Im3d");

    Im3d::EndFrame();

    uint32_t frameIndex = m_pRenderer->GetFrameID() % GFX_MAX_INFLIGHT_FRAMES;
    uint vertexCount = GetVertexCount();

    if (m_pVertexBuffer[frameIndex] == nullptr || m_pVertexBuffer[frameIndex]->GetBuffer()->GetDesc().size < vertexCount * sizeof(Im3d::VertexData))
    {
        m_pVertexBuffer[frameIndex].reset(m_pRenderer->CreateRawBuffer(nullptr, (vertexCount + 1000) * sizeof(Im3d::VertexData), "Im3d VB", GfxMemoryType::CpuToGpu));
    }

    uint32_t vertexBufferOffset = 0;

    for (unsigned int i = 0; i < Im3d::GetDrawListCount(); ++i)
    {
        const Im3d::DrawList& drawList = Im3d::GetDrawLists()[i];

        memcpy((char*)m_pVertexBuffer[frameIndex]->GetBuffer()->GetCpuAddress() + vertexBufferOffset,
            drawList.m_vertexData, sizeof(Im3d::VertexData) * drawList.m_vertexCount);

        uint32_t primitveCount = 0;

        switch (drawList.m_primType)
        {
        case Im3d::DrawPrimitive_Points:
            primitveCount = drawList.m_vertexCount;
            pCommandList->SetPipelineState(m_pPointPSO);
            break;
        case Im3d::DrawPrimitive_Lines:
            primitveCount = drawList.m_vertexCount / 2;
            pCommandList->SetPipelineState(m_pLinePSO);
            break;
        case Im3d::DrawPrimitive_Triangles:
            primitveCount = drawList.m_vertexCount / 3;
            pCommandList->SetPipelineState(m_pTrianglePSO);
            break;
        default:
            break;
        }

        uint32_t cb[] = 
        {
            primitveCount,
            m_pVertexBuffer[frameIndex]->GetSRV()->GetHeapIndex(),
            vertexBufferOffset,
        };

        pCommandList->SetGraphicsConstants(0, cb, sizeof(cb));
        pCommandList->DispatchMesh(DivideRoudingUp(primitveCount, 64), 1, 1);

        vertexBufferOffset += sizeof(Im3d::VertexData) * drawList.m_vertexCount;
    }
    
    void DrawTextDrawListsImGui(const Im3d::TextDrawList _textDrawLists[], Im3d::U32 _count);
    DrawTextDrawListsImGui(Im3d::GetTextDrawLists(), Im3d::GetTextDrawListCount());
}

uint32_t Im3dImpl::GetVertexCount() const
{
    uint32_t vertexCount = 0;

    for (unsigned int i = 0; i < Im3d::GetDrawListCount(); ++i)
    {
        const Im3d::DrawList& drawList = Im3d::GetDrawLists()[i];
        vertexCount += drawList.m_vertexCount;
    }

    return vertexCount;
}

void DrawTextDrawListsImGui(const Im3d::TextDrawList _textDrawLists[], Im3d::U32 _count)
{
    using namespace Im3d;
    
    Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
    const float4x4& viewProj = camera->GetNonJitterViewProjectionMatrix();
    
    // Invisible ImGui window which covers the screen.
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32_BLACK_TRANS);
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y));
    ImGui::Begin("Invisible", nullptr, 0
                 | ImGuiWindowFlags_NoTitleBar
                 | ImGuiWindowFlags_NoResize
                 | ImGuiWindowFlags_NoScrollbar
                 | ImGuiWindowFlags_NoInputs
                 | ImGuiWindowFlags_NoSavedSettings
                 | ImGuiWindowFlags_NoFocusOnAppearing
                 | ImGuiWindowFlags_NoBringToFrontOnFocus);
    
    ImDrawList* imDrawList = ImGui::GetWindowDrawList();
    
    for (U32 i = 0; i < _count; ++i)
    {
        const TextDrawList& textDrawList = Im3d::GetTextDrawLists()[i];
        
        if (textDrawList.m_layerId == Im3d::MakeId("NamedLayer"))
        {
            // The application may group primitives into layers, which can be used to change the draw state (e.g. enable depth testing, use a different shader)
        }
        
        for (U32 j = 0; j < textDrawList.m_textDataCount; ++j)
        {
            const Im3d::TextData& textData = textDrawList.m_textData[j];
            if (textData.m_positionSize.w == 0.0f || textData.m_color.getA() == 0.0f)
            {
                continue;
            }
            
            // Project world -> screen space.
            float4 clip = mul(viewProj, Vec4(textData.m_positionSize.x, textData.m_positionSize.y, textData.m_positionSize.z, 1.0f));
            float2 screen = Vec2(clip.x / clip.w, clip.y / clip.w);
            
            // Cull text which falls offscreen. Note that this doesn't take into account text size but works well enough in practice.
            if (clip.w < 0.0f || screen.x >= 1.0f || screen.y >= 1.0f)
            {
                continue;
            }
            
            // Pixel coordinates for the ImGuiWindow ImGui.
            screen = screen * 0.5f + 0.5f;
            screen.y = 1.0f - screen.y; // screen space origin is reversed by the projection.
            screen = screen * float2(ImGui::GetWindowSize().x, ImGui::GetWindowSize().y);
            
            // All text data is stored in a single buffer; each textData instance has an offset into this buffer.
            const char* text = textDrawList.m_textBuffer + textData.m_textBufferOffset;
            
            // Calculate the final text size in pixels to apply alignment flags correctly.
            ImGui::SetWindowFontScale(textData.m_positionSize.w); // NB no CalcTextSize API which takes a font/size directly...
            ImVec2 textSize = ImGui::CalcTextSize(text, text + textData.m_textLength);
            ImGui::SetWindowFontScale(1.0f);
            
            // Generate a pixel offset based on text flags.
            float2 textOffset = float2(-textSize.x * 0.5f, -textSize.y * 0.5f); // default to center
            if ((textData.m_flags & Im3d::TextFlags_AlignLeft) != 0)
            {
                textOffset.x = -textSize.x;
            }
            else if ((textData.m_flags & Im3d::TextFlags_AlignRight) != 0)
            {
                textOffset.x = 0.0f;
            }
            
            if ((textData.m_flags & Im3d::TextFlags_AlignTop) != 0)
            {
                textOffset.y = -textSize.y;
            }
            else if ((textData.m_flags & Im3d::TextFlags_AlignBottom) != 0)
            {
                textOffset.y = 0.0f;
            }
            
            // Add text to the window draw list.
            screen = screen + textOffset;
            imDrawList->AddText(nullptr, textData.m_positionSize.w * ImGui::GetFontSize(), ImVec2(screen.x, screen.y), textData.m_color.getABGR(), text, text + textData.m_textLength);
        }
    }
    
    ImGui::End();
    ImGui::PopStyleColor(1);
}
