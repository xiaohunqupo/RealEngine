#pragma once

#include "camera.h"
#include "light.h"
#include "physics/physics.h"

namespace tinyxml2
{
    class XMLElement;
}

class World
{
public:
    World();
    ~World();

    Camera* GetCamera() const { return m_pCamera.get(); }
    IPhysicsSystem* GetPhysicsSystem() const { return m_pPhysicsSystem.get(); }
    class BillboardSpriteRenderer* GetBillboardSpriteRenderer() const { return m_pBillboardSpriteRenderer.get(); }

    void LoadScene(const eastl::string& file);
    void SaveScene(const eastl::string& file);

    void AddObject(IVisibleObject* object);

    void Tick(float delta_time);

    IVisibleObject* GetVisibleObject(uint32_t index) const;
    ILight* GetPrimaryLight() const;

private:
    void ClearScene();

    void CreateVisibleObject(tinyxml2::XMLElement* element);
    void CreateLight(tinyxml2::XMLElement* element);
    void CreateCamera(tinyxml2::XMLElement* element);
    void CreateModel(tinyxml2::XMLElement* element);
    void CreateSky(tinyxml2::XMLElement* element);

    void PhysicsTest(Renderer* pRenderer);

private:
    eastl::unique_ptr<Camera> m_pCamera;
    eastl::unique_ptr<IPhysicsSystem> m_pPhysicsSystem;
    eastl::unique_ptr<class BillboardSpriteRenderer> m_pBillboardSpriteRenderer;

    eastl::vector<eastl::unique_ptr<IVisibleObject>> m_objects;

    ILight* m_pPrimaryLight = nullptr;

    eastl::unique_ptr<IPhysicsShape> m_boxShape;
    eastl::unique_ptr<IPhysicsShape> m_sphereShape;
};