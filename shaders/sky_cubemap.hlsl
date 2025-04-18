#include "common.hlsli"
#include "atmosphere.hlsli"

cbuffer CB : register(b0)
{
    uint c_cubeTextureUAV;
    uint c_cubeTextureSize;
    float c_rcpTextureSize;
    uint c_hdriTexture;
};

static const float3 views[6] =
{
    float3(1.0f, 0.0f, 0.0f),  // X+
    float3(-1.0f, 0.0f, 0.0f), // X-
    float3(0.0f, 1.0f, 0.0f),  // Y+
    float3(0.0f, -1.0f, 0.0f), // Y-
    float3(0.0f, 0.0f, 1.0f),  // Z+
    float3(0.0f, 0.0f, -1.0f), // Z-
};

static const float3 ups[6] =
{
    float3(0.0f, 1.0f, 0.0f),  // X+
    float3(0.0f, 1.0f, 0.0f),  // X-
    float3(0.0f, 0.0f, -1.0f), // Y+
    float3(0.0f, 0.0f, 1.0f),  // Y-
    float3(0.0f, 1.0f, 0.0f),  // Z+
    float3(0.0f, 1.0f, 0.0f),  // Z-
};

static const float3 rights[6] =
{
    float3(0.0f, 0.0f, -1.0f), // X+
    float3(0.0f, 0.0f, 1.0f),  // X-
    float3(1.0f, 0.0f, 0.0f),  // Y+
    float3(1.0f, 0.0f, 0.0f),  // Y-
    float3(1.0f, 0.0f, 0.0f),  // Z+
    float3(-1.0f, 0.0f, 0.0f), // Z-
};

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (any(dispatchThreadID.xy >= c_cubeTextureSize.xx))
    {
        return;
    }

    float2 uv = (float2(dispatchThreadID.xy) + 0.5) * c_rcpTextureSize;
    float2 xy = uv * 2.0f - float2(1.0f, 1.0f);
    xy.y = -xy.y;

    uint slice = dispatchThreadID.z;
    float3 dir = normalize(ups[slice] * xy.y + rights[slice] * xy.x + views[slice]);

#if HDRI_TEXTURE
    Texture2D hdriTexture = ResourceDescriptorHeap[c_hdriTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];

    float3 V = -dir;
    float u = (atan2(V.x, V.z) / M_PI + 1.0) * 0.5;
    float v = 1.0 - acos(V.y) / M_PI;
    float3 color = hdriTexture.SampleLevel(linearSampler, float2(u, v), 0).xyz;
#else
    float3 rayStart = float3(0, 0, 0);
    float3 rayDir = dir;
    float rayLength = INFINITY;

    bool PlanetShadow = false;
    if (PlanetShadow)
    {
        float2 planetIntersection = PlanetIntersection(rayStart, rayDir);
        if (planetIntersection.x > 0)
        {
            rayLength = min(rayLength, planetIntersection.x);
        }
    }

    float3 lightDir = SceneCB.lightDir;
    float3 lightColor = SceneCB.lightColor;

    float3 transmittance;
    float3 color = IntegrateScattering(rayStart, rayDir, rayLength, lightDir, lightColor, 64, transmittance);
#endif

    RWTexture2DArray<float3> cubeTexture = ResourceDescriptorHeap[c_cubeTextureUAV];
    cubeTexture[dispatchThreadID] = color;
}