#include "editor.h"
#include "imgui_impl.h"
#include "im3d_impl.h"
#include "core/engine.h"
#include "renderer/texture_loader.h"
#include "utils/assert.h"
#include "utils/system.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h" // for dock builder api
#include "ImFileDialog/ImFileDialog.h"
#include "ImGuizmo/ImGuizmo.h"

#include <fstream>

Editor::Editor(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
    m_pImGui = eastl::make_unique<ImGuiImpl>(pRenderer);
    m_pImGui->Init();

    m_pIm3d = eastl::make_unique<Im3dImpl>(pRenderer);
    m_pIm3d->Init();

    ifd::FileDialog::Instance().CreateTexture = [this, pRenderer](uint8_t* data, int w, int h, char fmt) -> void* 
    {
        Texture2D* texture = pRenderer->CreateTexture2D(w, h, 1, fmt == 1 ? GfxFormat::RGBA8SRGB : GfxFormat::BGRA8SRGB, 0, "ImFileDialog Icon");
        pRenderer->UploadTexture(texture->GetTexture(), data);

        m_fileDialogIcons.insert(eastl::make_pair(texture->GetSRV(), texture));

        return texture->GetSRV();
    };

    ifd::FileDialog::Instance().DeleteTexture = [this](void* tex) 
    {
        m_pendingDeletions.push_back((IGfxDescriptor*)tex); //should be deleted in next frame
    };

    eastl::string asset_path = Engine::GetInstance()->GetAssetPath();
    m_pTranslateIcon.reset(pRenderer->CreateTexture2D(asset_path + "ui/translate.png", true));
    m_pRotateIcon.reset(pRenderer->CreateTexture2D(asset_path + "ui/rotate.png", true));
    m_pScaleIcon.reset(pRenderer->CreateTexture2D(asset_path + "ui/scale.png", true));
}

Editor::~Editor()
{
    for (auto iter = m_fileDialogIcons.begin(); iter != m_fileDialogIcons.end(); ++iter)
    {
        delete iter->first;
        delete iter->second;
    }
}

void Editor::NewFrame()
{
    m_pImGui->NewFrame();
    m_pIm3d->NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse && io.MouseClicked[0])
    {
        ImVec2 mousePos = io.MouseClickedPos[0];
        mousePos.x *= io.DisplayFramebufferScale.x;
        mousePos.y *= io.DisplayFramebufferScale.y;

        m_pRenderer->RequestMouseHitTest((uint32_t)mousePos.x, (uint32_t)mousePos.y);
    }
}

void Editor::Tick()
{
    FlushPendingTextureDeletions();

    BuildDockLayout();
    DrawMenu();
    DrawToolBar();
    DrawGizmo();
    DrawFrameStats();

    if (m_bShowRenderer)
    {
        ImGui::Begin("Renderer", &m_bShowRenderer);
        
        m_pRenderer->OnGui();

        ImGui::End();
    }

    if (m_bShowInspector)
    {
        ImGui::Begin("Inspector", &m_bShowInspector);

        World* world = Engine::GetInstance()->GetWorld();
        IVisibleObject* pSelectedObject = world->GetVisibleObject(m_pRenderer->GetMouseHitObjectID());
        if (pSelectedObject)
        {
            pSelectedObject->OnGui();
        }

        ImGui::End();
    }
}

void Editor::Render(IGfxCommandList* pCommandList)
{
    m_pIm3d->Render(pCommandList);
    m_pImGui->Render(pCommandList);
}

void Editor::BuildDockLayout()
{
    m_dockSpace = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    if (m_bResetLayout)
    {
        ImGui::DockBuilderRemoveNode(m_dockSpace);
        ImGui::DockBuilderAddNode(m_dockSpace, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(m_dockSpace, ImGui::GetMainViewport()->WorkSize);

        m_bResetLayout = false;
    }

    if (ImGui::DockBuilderGetNode(m_dockSpace)->IsLeafNode())
    {
        ImGuiID leftNode, rightNode;
        ImGui::DockBuilderSplitNode(m_dockSpace, ImGuiDir_Right, 0.2f, &rightNode, &leftNode);

        ImGui::DockBuilderDockWindow("Renderer", rightNode);
        ImGui::DockBuilderDockWindow("Inspector", rightNode);

        ImGui::DockBuilderFinish(m_dockSpace);
    }
}

void Editor::DrawMenu()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open Scene"))
            {
                ifd::FileDialog::Instance().Open("SceneOpenDialog", "Open Scene", "XML file (*.xml){.xml},.*");
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug"))
        {
            if (ImGui::MenuItem("VSync", "", &m_bVsync))
            {
                m_pRenderer->GetSwapchain()->SetVSyncEnabled(m_bVsync);
            }

            if (ImGui::MenuItem("GPU Driven Stats", "", &m_bShowGpuDrivenStats))
            {
                m_pRenderer->SetGpuDrivenStatsEnabled(m_bShowGpuDrivenStats);
            }

            if (ImGui::MenuItem("Debug View Frustum", "", &m_bViewFrustumLocked))
            {
                Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
                camera->LockViewFrustum(m_bViewFrustumLocked);

                m_lockedViewPos = camera->GetPosition();
                m_lockedViewRotation = camera->GetRotation();
            }

            if (ImGui::MenuItem("Show Meshlets", "", &m_bShowMeshlets))
            {
                m_pRenderer->SetShowMeshletsEnabled(m_bShowMeshlets);
            }

            bool async_compute = m_pRenderer->IsAsyncComputeEnabled();
            if (ImGui::MenuItem("Async Compute", "", &async_compute))
            {
                m_pRenderer->SetAsyncComputeEnabled(async_compute);
            }

            if (ImGui::MenuItem("Reload Shaders"))
            {
                m_pRenderer->ReloadShaders();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::MenuItem("GPU Memory Stats", "", &m_bShowGpuMemoryStats))
            {
                if (m_bShowGpuMemoryStats)
                {
                    CreateGpuMemoryStats();
                }
                else
                {
                    m_pGpuMemoryStats.reset();
                }
            }

            if (ImGui::MenuItem("Render Graph", ""))
            {
                ShowRenderGraph();
            }

            ImGui::MenuItem("Imgui Demo", "", &m_bShowImguiDemo);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            ImGui::MenuItem("Inspector", "", &m_bShowInspector);
            ImGui::MenuItem("Renderer", "", &m_bShowRenderer);

            m_bResetLayout = ImGui::MenuItem("Reset Layout");

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (ifd::FileDialog::Instance().IsDone("SceneOpenDialog"))
    {
        if (ifd::FileDialog::Instance().HasResult())
        {
            eastl::string result = ifd::FileDialog::Instance().GetResult().u8string().c_str();
            Engine::GetInstance()->GetWorld()->LoadScene(result);
        }

        ifd::FileDialog::Instance().Close();
    }

    if (m_bViewFrustumLocked)
    {
        float3 scale = float3(1.0f, 1.0f, 1.0f);
        float4x4 mtxWorld;
        ImGuizmo::RecomposeMatrixFromComponents((const float*)&m_lockedViewPos, (const float*)&m_lockedViewRotation, (const float*)&scale, (float*)&mtxWorld);

        Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
        float4x4 view = camera->GetViewMatrix();
        float4x4 proj = camera->GetNonJitterProjectionMatrix();

        ImGuizmo::OPERATION operation;
        switch (m_selectEditMode)
        {
        case Editor::SelectEditMode::Translate:
            operation = ImGuizmo::TRANSLATE;
            break;
        case Editor::SelectEditMode::Rotate:
            operation = ImGuizmo::ROTATE;
            break;
        case Editor::SelectEditMode::Scale:
            operation = ImGuizmo::SCALE;
            break;
        default:
            RE_ASSERT(false);
            break;
        }
        ImGuizmo::Manipulate((const float*)&view, (const float*)&proj, operation, ImGuizmo::WORLD, (float*)&mtxWorld);

        ImGuizmo::DecomposeMatrixToComponents((const float*)&mtxWorld, (float*)&m_lockedViewPos, (float*)&m_lockedViewRotation, (float*)&scale);

        camera->SetFrustumViewPosition(m_lockedViewPos);

        float4x4 mtxViewFrustum = mul(camera->GetProjectionMatrix(), inverse(mtxWorld));
        camera->UpdateFrustumPlanes(mtxViewFrustum);
    }

    if (m_bShowGpuMemoryStats && m_pGpuMemoryStats)
    {
        ImGui::Begin("GPU Memory Stats", &m_bShowGpuMemoryStats);
        const GfxTextureDesc& desc = m_pGpuMemoryStats->GetTexture()->GetDesc();
        ImGui::Image((ImTextureID)m_pGpuMemoryStats->GetSRV(), ImVec2((float)desc.width, (float)desc.height));
        ImGui::End();
    }

    if (m_bShowImguiDemo)
    {
        ImGui::ShowDemoWindow(&m_bShowImguiDemo);
    }
}

void Editor::DrawToolBar()
{
    ImGui::SetNextWindowPos(ImVec2(0, 20));
    ImGui::SetNextWindowSize(ImVec2(300, 30));

    ImGui::Begin("EditorToolBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

    ImVec4 focusedBG(1.0f, 0.6f, 0.2f, 0.5f);
    ImVec4 normalBG(0.0f, 0.0f, 0.0f, 0.0f);

    if (ImGui::ImageButton("translate_button##editor", (ImTextureID)m_pTranslateIcon->GetSRV(), ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), m_selectEditMode == SelectEditMode::Translate ? focusedBG : normalBG))
    {
        m_selectEditMode = SelectEditMode::Translate;
    }
    
    ImGui::SameLine(0.0f, 0.0f);
    if (ImGui::ImageButton("rotate_button##editor", (ImTextureID)m_pRotateIcon->GetSRV(), ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), m_selectEditMode == SelectEditMode::Rotate ? focusedBG : normalBG))
    {
        m_selectEditMode = SelectEditMode::Rotate;
    }
    
    ImGui::SameLine(0.0f, 0.0f);
    if (ImGui::ImageButton("scale_button##editor", (ImTextureID)m_pScaleIcon->GetSRV(), ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), m_selectEditMode == SelectEditMode::Scale ? focusedBG : normalBG))
    {
        m_selectEditMode = SelectEditMode::Scale;
    }
    ImGui::PopStyleVar();

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::PushItemWidth(150.0f);
    int renderOutput = (int)m_pRenderer->GetOutputType();
    ImGui::Combo("##RenderOutput", &renderOutput, 
        "Default\0Diffuse\0Specular(F0)\0World Normal\0Roughness\0Emissive\0Shading Model\0Custom Data\0AO\0Direct Lighting\0Indirect Specular\0Indirect Diffuse\0Path Tracing\0Physics\0\0", (int)RendererOutput::Max);
    m_pRenderer->SetOutputType((RendererOutput)renderOutput);
    ImGui::PopItemWidth();

    ImGui::End();
}

void Editor::DrawGizmo()
{
    World* world = Engine::GetInstance()->GetWorld();

    IVisibleObject* pSelectedObject = world->GetVisibleObject(m_pRenderer->GetMouseHitObjectID());
    if (pSelectedObject == nullptr)
    {
        return;
    }

    float3 pos = pSelectedObject->GetPosition();
    float3 rotation = rotation_angles(pSelectedObject->GetRotation());
    float3 scale = pSelectedObject->GetScale();

    float4x4 mtxWorld;
    ImGuizmo::RecomposeMatrixFromComponents((const float*)&pos, (const float*)&rotation, (const float*)&scale, (float*)&mtxWorld);

    ImGuizmo::OPERATION operation;
    switch (m_selectEditMode)
    {
    case Editor::SelectEditMode::Translate:
        operation = ImGuizmo::TRANSLATE;
        break;
    case Editor::SelectEditMode::Rotate:
        operation = ImGuizmo::ROTATE;
        break;
    case Editor::SelectEditMode::Scale:
        operation = ImGuizmo::SCALE;
        break;
    default:
        RE_ASSERT(false);
        break;
    }

    Camera* pCamera = world->GetCamera();
    float4x4 view = pCamera->GetViewMatrix();
    float4x4 proj = pCamera->GetNonJitterProjectionMatrix();

    ImGuizmo::AllowAxisFlip(false);
    ImGuizmo::Manipulate((const float*)&view, (const float*)&proj, operation, ImGuizmo::WORLD, (float*)&mtxWorld);

    ImGuizmo::DecomposeMatrixToComponents((const float*)&mtxWorld, (float*)&pos, (float*)&rotation, (float*)&scale);
    pSelectedObject->SetPosition(pos);
    pSelectedObject->SetRotation(rotation_quat(rotation));
    pSelectedObject->SetScale(scale);
}

void Editor::DrawFrameStats()
{
    ImVec2 windowPos(ImGui::GetIO().DisplaySize.x - 200.0f, 50.0f);

    ImGuiDockNode* dockSapce = ImGui::DockBuilderGetNode(m_dockSpace);
    ImGuiDockNode* centralNode = dockSapce->CentralNode;
    if (centralNode)
    {
        windowPos.x = centralNode->Size.x - 200.0f;
    }
    
    ImGui::SetNextWindowPos(windowPos);
    ImGui::SetNextWindowSize(ImVec2(200.0f, 50.0f));
    ImGui::Begin("Frame Stats", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}

void Editor::CreateGpuMemoryStats()
{
    Engine* pEngine = Engine::GetInstance();
    IGfxDevice* pDevice = m_pRenderer->GetDevice();

    if (pDevice->DumpMemoryStats(pEngine->GetWorkPath() + "d3d12ma.json"))
    {
        eastl::string path = pEngine->GetWorkPath();
        eastl::string cmd = "python " + path + "tools/D3d12maDumpVis.py -o " + path + "d3d12ma.png " + path + "d3d12ma.json";

        if (ExecuteCommand(cmd.c_str()) == 0)
        {
            eastl::string file = path + "d3d12ma.png";
            m_pGpuMemoryStats.reset(m_pRenderer->CreateTexture2D(file, true));
        }
    }
}

void Editor::ShowRenderGraph()
{
    Engine* pEngine = Engine::GetInstance();

    eastl::string file = pEngine->GetWorkPath() + "tools/graphviz/rendergraph.html";
    eastl::string graph = m_pRenderer->GetRenderGraph()->Export();
    
    std::ofstream stream;
    stream.open(file.c_str());
    stream << R"(<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Render Graph</title>
  </head>
  <body>
    <script src="viz-standalone.js"></script>
    <script>
        Viz.instance()
            .then(viz => {
                document.body.appendChild(viz.renderSVGElement(`
)";
    stream << graph.c_str();
    stream << R"(
                `));
            })
            .catch(error => {
                console.error(error);
            });
    </script>
  </body>
</html>
)";
    
    stream.close();

    eastl::string command = "start " + file;
    ExecuteCommand(command.c_str());
}

void Editor::FlushPendingTextureDeletions()
{
    for (size_t i = 0; i < m_pendingDeletions.size(); ++i)
    {
        IGfxDescriptor* srv = m_pendingDeletions[i];
        auto iter = m_fileDialogIcons.find(srv);
        RE_ASSERT(iter != m_fileDialogIcons.end());

        Texture2D* texture = iter->second;
        m_fileDialogIcons.erase(srv);

        delete texture;
    }

    m_pendingDeletions.clear();
}

