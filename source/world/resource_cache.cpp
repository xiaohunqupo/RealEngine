#include "resource_cache.h"
#include "renderer/renderer.h"
#include "core/engine.h"

ResourceCache* ResourceCache::GetInstance()
{
    static ResourceCache cache;
    return &cache;
}

Texture2D* ResourceCache::GetTexture2D(const eastl::string& file, bool srgb)
{
    auto iter = m_cachedTexture2D.find(file);
    if (iter != m_cachedTexture2D.end())
    {
        iter->second.refCount++;
        return (Texture2D*)iter->second.ptr;
    }

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    Resource texture;
    texture.refCount = 1;
    texture.ptr = pRenderer->CreateTexture2D(file, srgb);
    m_cachedTexture2D.insert(eastl::make_pair(file, texture));

    return (Texture2D*)texture.ptr;
}

void ResourceCache::ReleaseTexture2D(Texture2D* texture)
{
    if (texture == nullptr)
    {
        return;
    }

    for (auto iter = m_cachedTexture2D.begin(); iter != m_cachedTexture2D.end(); ++iter)
    {
        if (iter->second.ptr == texture)
        {
            iter->second.refCount--;
            
            if (iter->second.refCount == 0)
            {
                delete texture;
                m_cachedTexture2D.erase(iter);
            }

            return;
        }
    }

    RE_ASSERT(false);
}

OffsetAllocator::Allocation ResourceCache::GetSceneBuffer(const eastl::string& name, const void* data, uint32_t size)
{
    auto iter = m_cachedSceneBuffer.find(name);
    if (iter != m_cachedSceneBuffer.end())
    {
        iter->second.refCount++;
        return iter->second.allocation;
    }

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    SceneBuffer buffer;
    buffer.refCount = 1;
    buffer.allocation = pRenderer->AllocateSceneStaticBuffer(data, size);
    m_cachedSceneBuffer.insert(eastl::make_pair(name, buffer));

    return buffer.allocation;
}

void ResourceCache::RelaseSceneBuffer(OffsetAllocator::Allocation allocation)
{
    if (allocation.metadata == OffsetAllocator::Allocation::NO_SPACE)
    {
        return;
    }

    for (auto iter = m_cachedSceneBuffer.begin(); iter != m_cachedSceneBuffer.end(); ++iter)
    {
        if (iter->second.allocation.metadata == allocation.metadata &&
            iter->second.allocation.offset == allocation.offset)
        {
            iter->second.refCount--;

            if (iter->second.refCount == 0)
            {
                Engine::GetInstance()->GetRenderer()->FreeSceneStaticBuffer(allocation);
                m_cachedSceneBuffer.erase(iter);
            }

            return;
        }
    }

    RE_ASSERT(false);
}
