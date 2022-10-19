#include "common.hlsli"
#include "debug.hlsli"

// the intersection point of three planes 
float3 IntersectPlanes(float4 p0, float4 p1, float4 p2)
{
    float3 n12 = cross(p1.xyz, p2.xyz);
    float3 n20 = cross(p2.xyz, p0.xyz);
    float3 n01 = cross(p0.xyz, p1.xyz);

    float3 r = -p0.w * n12 - p1.w * n20 - p2.w * n01;
    return r * (1 / dot(p0.xyz, n12));
}

[numthreads(1, 1, 1)]
void main()
{
    const float4 left = GetCameraCB().culling.planes[0];
    const float4 right = GetCameraCB().culling.planes[1];
    const float4 top = GetCameraCB().culling.planes[2];
    const float4 bottom = GetCameraCB().culling.planes[3];
    const float4 far = GetCameraCB().culling.planes[4];
    const float4 near = GetCameraCB().culling.planes[5];
    
    const float3 color = float3(1, 0, 0);
    
    float3 v1 = IntersectPlanes(left, top, far);
    float3 v2 = IntersectPlanes(left, top, near);
    float3 v3 = IntersectPlanes(right, top, far);
    float3 v4 = IntersectPlanes(right, top, near);
    float3 v5 = IntersectPlanes(right, bottom, far);
    float3 v6 = IntersectPlanes(right, bottom, near);
    float3 v7 = IntersectPlanes(left, bottom, far);
    float3 v8 = IntersectPlanes(left, bottom, near);

    debug::DrawLine(v1, v2, color);
    debug::DrawLine(v3, v4, color);
    debug::DrawLine(v5, v6, color);
    debug::DrawLine(v7, v8, color);
    
    /*
    debug::DrawLine(v1, v3, color);
    debug::DrawLine(v3, v5, color);
    debug::DrawLine(v5, v7, color);
    debug::DrawLine(v7, v1, color);
    */
    
    debug::DrawLine(v2, v4, color);
    debug::DrawLine(v4, v6, color);
    debug::DrawLine(v6, v8, color);
    debug::DrawLine(v8, v2, color);
}