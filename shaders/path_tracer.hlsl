#include "ray_trace.hlsli"
#include "random.hlsli"
#include "importance_sampling.hlsli"
#include "debug.hlsli"

cbuffer PathTracingConstants : register(b0)
{
    uint c_diffuseRT;
    uint c_specularRT;
    uint c_normalRT;
    uint c_emissiveRT;
    
    uint c_depthRT;
    uint c_maxRayLength;
    uint c_currentSampleIndex;
    uint c_outputTexture;
};

float ProbabilityToSampleDiffuse(float3 diffuse, float3 specular)
{
    float lumDiffuse = Luminance(diffuse);
    float lumSpecular = Luminance(specular);
    return lumDiffuse / max(lumDiffuse + lumSpecular, 0.0001);
}

float3 SkyColor(uint2 screenPos)
{
    float3 position = GetWorldPosition(screenPos, 1e-10);
    float3 dir = normalize(position - GetCameraCB().cameraPos);
    
    TextureCube skyTexture = ResourceDescriptorHeap[SceneCB.skyCubeTexture];
    SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
    float3 sky_color = skyTexture.SampleLevel(linearSampler, dir, 0).xyz;

    return sky_color;
}

[numthreads(8, 8, 1)]
void path_tracing(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D diffuseRT = ResourceDescriptorHeap[c_diffuseRT];
    Texture2D specularRT = ResourceDescriptorHeap[c_specularRT];
    Texture2D normalRT = ResourceDescriptorHeap[c_normalRT];
    Texture2D<float3> emissiveRT = ResourceDescriptorHeap[c_emissiveRT];
    Texture2D<float> depthRT = ResourceDescriptorHeap[c_depthRT];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[c_outputTexture];

    float depth = depthRT[dispatchThreadID.xy];
    if (depth == 0.0)
    {
        outputTexture[dispatchThreadID.xy] = float4(SkyColor(dispatchThreadID.xy), 1.0);
        return;
    }

    PRNG rng = PRNG::Create(dispatchThreadID.xy, SceneCB.renderSize);

    float3 position = GetWorldPosition(dispatchThreadID.xy, depth);
    float3 wo = normalize(GetCameraCB().cameraPos - position);

    float3 diffuse = diffuseRT[dispatchThreadID.xy].xyz;
    float3 specular = specularRT[dispatchThreadID.xy].xyz;
    float3 N = DecodeNormal(normalRT[dispatchThreadID.xy].xyz);
    float roughness = normalRT[dispatchThreadID.xy].w;
    float3 emissive = emissiveRT[dispatchThreadID.xy];

    float3 radiance = 0.0;
    float3 throughput = 1.0;
    float pdf = 1.0;
    float roughness_bias = 0.5 * roughness; //reduce fireflies

    rt::RayCone cone = rt::RayCone::FromGBuffer(GetLinearDepth(depth));
    
    for (uint i = 0; i < c_maxRayLength + 1; ++i)
    {
        //direct light
        float3 wi = SceneCB.lightDir;

        RayDesc ray;
        ray.Origin = position + N * 0.01;
        ray.Direction = SampleConeUniform(rng.RandomFloat2(), SceneCB.lightRadius, wi);
        ray.TMin = 0.00001;
        ray.TMax = 1000.0;

        float visibility = rt::TraceVisibilityRay(ray) ? 1.0 : 0.0;
        float NdotL = saturate(dot(N, wi));
        float3 direct_light = DefaultBRDF(wi, wo, N, diffuse, specular, roughness) * visibility * SceneCB.lightColor * NdotL;
        radiance += (direct_light + emissive) * throughput / pdf;

        if (i == c_maxRayLength)
        {
            break;
        }

        //indirect light
        float probDiffuse = ProbabilityToSampleDiffuse(diffuse, specular);
        bool chooseDiffuse = rng.RandomFloat() < probDiffuse;
        if (chooseDiffuse)
        {
            wi = SampleCosHemisphere(rng.RandomFloat2(), N); //pdf : NdotL / M_PI

            float3 diffuse_brdf = DiffuseBRDF(diffuse);
            float NdotL = saturate(dot(N, wi));

            throughput *= diffuse_brdf * NdotL;
            pdf *= (NdotL / M_PI) * probDiffuse;
        }
        else
        {
            #define GGX_VNDF 1

#if GGX_VNDF
            float3 H = SampleGGXVNDF(rng.RandomFloat2(), roughness, N, wo);
#else
            float3 H = SampleGGX(rng.RandomFloat2(), roughness, N);
#endif
            wi = reflect(-wo, H);
            
            roughness = max(roughness, 0.065); //fix reflection artifacts on smooth surface

            float3 F;
            float3 specular_brdf = SpecularBRDF(N, wo, wi, specular, roughness, F);
            float NdotL = saturate(dot(N, wi));

            throughput *= specular_brdf * NdotL;

            float a = roughness * roughness;
            float a2 = a * a;
            float D = D_GGX(N, H, a);
            float NdotH = saturate(dot(N, H));
            float LdotH = saturate(dot(wi, H));
            float NdotV = saturate(dot(N, wo));

#if GGX_VNDF
            float samplePDF = D / (2.0 * (NdotV + sqrt(NdotV * (NdotV - NdotV * a2) + a2)));
#else
            float samplePDF = D * NdotH / (4 * LdotH);
#endif
            pdf *= samplePDF  * (1.0 - probDiffuse);
        }

        ray.Origin = position + N * 0.001;
        ray.Direction = wi;
        ray.TMin = 0.001;
        ray.TMax = 1000.0;

        rt::HitInfo hitInfo;
        if (rt::TraceRay(ray, hitInfo))
        {
            cone.Propagate(0.0, hitInfo.rayT);
            rt::MaterialData material = rt::GetMaterial(ray, hitInfo, cone);

            position = hitInfo.position;
            wo = -wi;

            diffuse = material.diffuse;
            specular = material.specular;
            N = material.worldNormal;
            roughness = lerp(material.roughness, 1, roughness_bias);
            emissive = material.emissive;
            
            roughness_bias = lerp(roughness_bias, 1, 0.5 * material.roughness);
        }
        else
        {
            TextureCube skyTexture = ResourceDescriptorHeap[SceneCB.skyCubeTexture];
            SamplerState linearSampler = SamplerDescriptorHeap[SceneCB.bilinearClampSampler];
            float3 sky_color = skyTexture.SampleLevel(linearSampler, wi, 0).xyz;

            radiance += sky_color * throughput / pdf;
            break;
        }
    }

    if(any(isnan(radiance)) || any(isinf(radiance)))
    {
        radiance = float3(0, 0, 0);
    }

    outputTexture[dispatchThreadID.xy] = float4(clamp(radiance, 0.0, 65504.0), 1.0);
}

cbuffer AccumulationConstants : register(b0)
{
    uint c_currentFrameTexture;
    uint c_historyTexture;
    uint c_accumulationTexture;
    uint c_accumulatedFrames;
    uint c_bAccumulationFinished;
};

[numthreads(8, 8, 1)]
void accumulation(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D currentFrameTexture = ResourceDescriptorHeap[c_currentFrameTexture];
    RWTexture2D<float4> historyTexture = ResourceDescriptorHeap[c_historyTexture];
    RWTexture2D<float4> accumulationTexture = ResourceDescriptorHeap[c_accumulationTexture];
    
    if (c_bAccumulationFinished)
    {
        accumulationTexture[dispatchThreadID.xy] = clamp(historyTexture[dispatchThreadID.xy], 0.0, 65504.0);
    }
    else
    {
        float3 current = currentFrameTexture[dispatchThreadID.xy].xyz;
        float3 history = historyTexture[dispatchThreadID.xy].xyz;
        float3 output = (c_accumulatedFrames * history + current) / (c_accumulatedFrames + 1);
            
        historyTexture[dispatchThreadID.xy] = float4(output, 1.0);
        accumulationTexture[dispatchThreadID.xy] = float4(clamp(output, 0.0, 65504.0), 1.0);
    }
}
