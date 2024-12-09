#include "world.h"
#include "gltf_loader.h"
#include "sky_sphere.h"
#include "directional_light.h"
#include "point_light.h"
#include "spot_light.h"
#include "rect_light.h"
#include "static_mesh.h"
#include "mesh_material.h"
#include "utils/assert.h"
#include "utils/string.h"
#include "utils/profiler.h"
#include "utils/log.h"
#include "utils/gui_util.h"
#include "utils/parallel_for.h"
#include "tinyxml2/tinyxml2.h"
#include "EASTL/atomic.h"

World::World()
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    m_pCamera = eastl::make_unique<Camera>();
    m_pPhysicsSystem.reset(CreatePhysicsSystem(PhysicsEngine::Jolt));
    m_pPhysicsSystem->Initialize();

    m_boxShape.reset(m_pPhysicsSystem->CreateBoxShape(float3(1.0f, 1.0f, 1.0f)));
    m_sphereShape.reset(m_pPhysicsSystem->CreateSphereShape(1.0f));
}

World::~World() = default;

void World::LoadScene(const eastl::string& file)
{
    RE_INFO("Loading Scene : {}", file);

    tinyxml2::XMLDocument doc;
    if (tinyxml2::XML_SUCCESS != doc.LoadFile(file.c_str()))
    {
        return;
    }

    ClearScene();

    tinyxml2::XMLNode* root_node = doc.FirstChild();
    RE_ASSERT(root_node != nullptr && strcmp(root_node->Value(), "scene") == 0);

    for (tinyxml2::XMLElement* element = root_node->FirstChildElement(); element != nullptr; element = (tinyxml2::XMLElement*)element->NextSibling())
    {
        CreateVisibleObject(element);
    }

    m_pPhysicsSystem->OptimizeTLAS();
}

void World::SaveScene(const eastl::string& file)
{
}

void World::AddObject(IVisibleObject* object)
{
    RE_ASSERT(object != nullptr);

    object->SetID((uint32_t)m_objects.size());
    m_objects.push_back(eastl::unique_ptr<IVisibleObject>(object));
}

void World::AddLight(ILight* light)
{
    RE_ASSERT(light != nullptr);
    m_lights.push_back(eastl::unique_ptr<ILight>(light));
}

void World::Tick(float delta_time)
{
    CPU_EVENT("Tick", "World::Tick");

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    PhysicsTest(pRenderer);

    m_pPhysicsSystem->Tick(delta_time);
    m_pCamera->Tick(delta_time);

    for (auto iter = m_objects.begin(); iter != m_objects.end(); ++iter)
    {
        (*iter)->Tick(delta_time);
    }

    for (auto iter = m_lights.begin(); iter != m_lights.end(); ++iter)
    {
        (*iter)->Tick(delta_time);
    }

    if (pRenderer->GetOutputType() != RendererOutput::Physics)
    {
        eastl::vector<IVisibleObject*> visibleObjects(m_objects.size());
        eastl::atomic<uint32_t> visibleCount{ 0 };

        ParallelFor((uint32_t)m_objects.size(), [&](uint32_t i)
            {
                if (m_objects[i]->FrustumCull(m_pCamera->GetFrustumPlanes(), 6))
                {
                    uint32_t index = visibleCount.fetch_add(1);
                    visibleObjects[index] = m_objects[i].get();
                }
            });

        visibleObjects.resize(visibleCount);

        for (auto iter = visibleObjects.begin(); iter != visibleObjects.end(); ++iter)
        {
            (*iter)->Render(pRenderer);
        }
    }
}

IVisibleObject* World::GetVisibleObject(uint32_t index) const
{
    if (index >= m_objects.size())
    {
        return nullptr;
    }

    return m_objects[index].get();
}

ILight* World::GetPrimaryLight() const
{
    RE_ASSERT(m_pPrimaryLight != nullptr);
    return m_pPrimaryLight;
}

void World::ClearScene()
{
    m_objects.clear();
    m_lights.clear();
    m_pPrimaryLight = nullptr;
}

inline float3 str_to_float3(const eastl::string& str)
{
    eastl::vector<float> v;
    v.reserve(3);
    string_to_float_array(str, v);
    return float3(v[0], v[1], v[2]);
}

inline void LoadVisibleObject(tinyxml2::XMLElement* element, IVisibleObject* object)
{
    const tinyxml2::XMLAttribute* position = element->FindAttribute("position");
    if (position)
    {
        object->SetPosition(str_to_float3(position->Value()));
    }

    const tinyxml2::XMLAttribute* rotation = element->FindAttribute("rotation");
    if (rotation)
    {
        object->SetRotation(rotation_quat(str_to_float3(rotation->Value())));
    }

    const tinyxml2::XMLAttribute* scale = element->FindAttribute("scale");
    if (scale)
    {
        object->SetScale(str_to_float3(scale->Value()));
    }
}

inline void LoadLight(tinyxml2::XMLElement* element, ILight* light)
{
    LoadVisibleObject(element, light);

    const tinyxml2::XMLAttribute* intensity = element->FindAttribute("intensity");
    if (intensity)
    {
        light->SetLightIntensity(intensity->FloatValue());
    }

    const tinyxml2::XMLAttribute* color = element->FindAttribute("color");
    if (color)
    {
        light->SetLightColor(str_to_float3(color->Value()));
    }

    const tinyxml2::XMLAttribute* radius = element->FindAttribute("radius");
    if (radius)
    {
        light->SetLightRadius(radius->FloatValue());
    }
}

void World::CreateVisibleObject(tinyxml2::XMLElement* element)
{
    if (strcmp(element->Value(), "light") == 0)
    {
        CreateLight(element);
    }
    else if (strcmp(element->Value(), "camera") == 0)
    {
        CreateCamera(element);
    }
    else if (strcmp(element->Value(), "model") == 0)
    {
        CreateModel(element);
    }
    else if(strcmp(element->Value(), "skysphere") == 0)
    {
        CreateSky(element);
    }
}

void World::CreateLight(tinyxml2::XMLElement* element)
{
    ILight* light = nullptr;

    const tinyxml2::XMLAttribute* type = element->FindAttribute("type");
    RE_ASSERT(type != nullptr);

    if (strcmp(type->Value(), "directional") == 0)
    {
        light = new DirectionalLight();
    }
    else if (strcmp(type->Value(), "point") == 0)
    {
        light = new PointLight();
    }
    else if (strcmp(type->Value(), "spot") == 0)
    {
        light = new SpotLight();
    }
    else if (strcmp(type->Value(), "rect") == 0)
    {
        light = new RectLight();
    }
    else
    {
        RE_ASSERT(false);
    }

    LoadLight(element, light);

    if (!light->Create())
    {
        delete light;
        return;
    }

    AddLight(light);

    const tinyxml2::XMLAttribute* primary = element->FindAttribute("primary");
    if (primary && primary->BoolValue())
    {
        m_pPrimaryLight = light;
    }
}

void World::CreateCamera(tinyxml2::XMLElement* element)
{
    const tinyxml2::XMLAttribute* position = element->FindAttribute("position");
    if (position)
    {
        m_pCamera->SetPosition(str_to_float3(position->Value()));
    }

    const tinyxml2::XMLAttribute* rotation = element->FindAttribute("rotation");
    if (rotation)
    {
        m_pCamera->SetRotation(str_to_float3(rotation->Value()));
    }

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    uint32_t window_width = pRenderer->GetRenderWidth();
    uint32_t window_height = pRenderer->GetRenderHeight();

    m_pCamera->SetPerpective((float)window_width / window_height,
        element->FindAttribute("fov")->FloatValue(),
        element->FindAttribute("znear")->FloatValue());
}

void World::CreateModel(tinyxml2::XMLElement* element)
{
    GLTFLoader loader(this);
    loader.LoadSettings(element);
    loader.Load();
}

void World::CreateSky(tinyxml2::XMLElement* element)
{
    IVisibleObject* object = new SkySphere();
    RE_ASSERT(object != nullptr);

    LoadVisibleObject(element, object);

    if (!object->Create())
    {
        delete object;
        return;
    }

    AddObject(object);
}

void World::PhysicsTest(Renderer* pRenderer)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard && 
        !io.NavActive && 
        (ImGui::IsKeyReleased(ImGuiKey_Space) || ImGui::IsKeyReleased(ImGuiKey_GamepadFaceDown)))
    {
        IPhysicsRigidBody* body = nullptr;
        GLTFLoader loader(this);

        if (pRenderer->GetFrameID() % 2 == 1)
        {
            loader.Load("model/box.gltf");
            body = m_pPhysicsSystem->CreateRigidBody(m_boxShape.get(), PhysicsMotion::Dynamic, PhysicsLayers::DYNAMIC, m_objects.back().get());
        }
        else
        {
            loader.Load("model/sphere.gltf");
            body = m_pPhysicsSystem->CreateRigidBody(m_sphereShape.get(), PhysicsMotion::Dynamic, PhysicsLayers::DYNAMIC, m_objects.back().get());
        }

        body->AddToPhysicsSystem(true);
        body->SetLinearVelocity(m_pCamera->GetForward() * 10.0f);

        StaticMesh* mesh = (StaticMesh*)m_objects.back().get();
        mesh->SetPhysicsBody(body);
        mesh->SetScale(float3(0.3));
        mesh->SetPosition(m_pCamera->GetPosition() + m_pCamera->GetForward() * 1.0f);

        auto WangHash = [](uint x)
        {
            x = (x ^ 61) ^ (x >> 16);
            x *= 9;
            x = x ^ (x >> 4);
            x *= 0x27d4eb2d;
            x = x ^ (x >> 15);
            return x;
        };
        uint hash = WangHash((uint)m_objects.size());

        MeshMaterial* material = mesh->GetMaterial();
        material->m_bPbrMetallicRoughness = true;
        material->m_albedoColor = float3(float(hash & 255), float((hash >> 8) & 255), float((hash >> 16) & 255)) / 255.0f;
        material->m_roughness = float(hash >> 24) / 255.0f;
    }
}
