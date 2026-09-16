// Area downsample and matched-residual resolve adapted from xenmods/DLSSNR-Cost-Scaler.
// Copyright (c) 2026 xen. MIT: see docs/LICENSE-DLSSNR-Cost-Scaler.txt.
// Runs in the existing NR codec domain; never changes the game's SR resolution.
#ifndef NR_EDGE_DEPTH
#define NR_EDGE_DEPTH 0
#endif
#if NR_EDGE_DEPTH
#define ROOT_SIGNATURE \
    "DescriptorTable(SRV(t0)),DescriptorTable(UAV(u0))," \
    "RootConstants(num32BitConstants=24,b0)," \
    "DescriptorTable(SRV(t1)),DescriptorTable(SRV(t2)),DescriptorTable(SRV(t3))"
#else
#define ROOT_SIGNATURE \
    "DescriptorTable(SRV(t0)),DescriptorTable(UAV(u0))," \
    "RootConstants(num32BitConstants=15,b0)," \
    "DescriptorTable(SRV(t1)),DescriptorTable(SRV(t2))"
#endif
Texture2D<float4> Source : register(t0);
Texture2D<float4> SmallInput : register(t1);
Texture2D<float4> NativeColor : register(t2);
#if NR_EDGE_DEPTH
Texture2D<float4> DepthInput : register(t3);
#endif
RWTexture2D<float4> Destination : register(u0);
cbuffer ResampleConstants : register(b0)
{
    uint2 SourceOrigin;
    uint2 SourceExtent;
    uint2 DestinationSize;
    uint FilterMode; // 0 area, 1 residual, 2 exact snapshot, 3 bilinear, 4 direct, 5 zero
    float Sharpness;
    uint2 DestinationOrigin;
    float TransferStrength;
    float ColorStrength;
    float DetailStrength;
    float ColourCoupling;
    float EdgeProtection;
#if NR_EDGE_DEPTH
    uint DepthAvailable;   // c3.w; keep before uint2 values to avoid padding
    uint2 DepthOrigin;     // c4.xy
    uint2 DepthExtent;     // c4.zw
    float EdgeThickness;   // c5.x; 0..6 display pixels
    float EdgeShift;       // c5.y; negative outward, positive inward
    float EdgeSoftness;    // c5.z; 0..1 mask feather
    uint DepthInverted;    // c5.w
#endif
};
float4 LoadClamped(int2 p)
{
    return Source.Load(int3(int2(SourceOrigin) + clamp(p, 0, int2(SourceExtent)-1), 0));
}
float4 SampleSmall(Texture2D<float4> tex, float2 p)
{
    int2 b = int2(floor(p));
    float2 f = frac(p);
    int2 m = int2(SourceExtent)-1;
    return lerp(lerp(tex.Load(int3(clamp(b,0,m),0)),tex.Load(int3(clamp(b+int2(1,0),0,m),0)),f.x),
        lerp(tex.Load(int3(clamp(b+int2(0,1),0,m),0)),tex.Load(int3(clamp(b+1,0,m),0)),f.x),f.y);
}
float4 SampleArea(Texture2D<float4> tex, uint2 pixel)
{
    float2 lo = float2(pixel)*float2(SourceExtent)/float2(DestinationSize);
    float2 hi = float2(pixel+1)*float2(SourceExtent)/float2(DestinationSize);
    float4 sum = 0;
    for (int y=int(floor(lo.y)); y<int(ceil(hi.y)); ++y)
        for (int x=int(floor(lo.x)); x<int(ceil(hi.x)); ++x)
        {
            float2 w = max(min(hi,float2(x+1,y+1))-max(lo,float2(x,y)),0);
            sum += tex.Load(int3(clamp(int2(x,y),0,int2(SourceExtent)-1),0))*w.x*w.y;
        }
    return sum/max((hi.x-lo.x)*(hi.y-lo.y),1e-6);
}
float4 Bilinear(float2 p)
{
    int2 b = int2(floor(p)); float2 f = frac(p);
    return lerp(lerp(LoadClamped(b),LoadClamped(b+int2(1,0)),f.x),
        lerp(LoadClamped(b+int2(0,1)),LoadClamped(b+1),f.x),f.y);
}
float3 NativeAt(int2 p) { return NativeColor.Load(int3(clamp(p,0,int2(DestinationSize)-1),0)).rgb; }
float3 MappedAt(Texture2D<float4> tex, int2 p)
{
    p = clamp(p,0,int2(DestinationSize)-1);
    if (any(SourceExtent > DestinationSize)) return SampleArea(tex,uint2(p)).rgb;
    float2 position = (float2(p)+0.5)*float2(SourceExtent)/float2(DestinationSize)-0.5;
    return SampleSmall(tex,position).rgb;
}
#if NR_EDGE_DEPTH
float DepthAt(int2 p)
{
    int2 displayPixel = clamp(p,0,int2(DestinationSize)-1);
    int2 depthPixel = clamp(int2((float2(displayPixel)+0.5)*float2(DepthExtent)/float2(DestinationSize)),
        0,int2(DepthExtent)-1);
    return DepthInput.Load(int3(int2(DepthOrigin)+depthPixel,0)).r;
}
#endif
[RootSignature(ROOT_SIGNATURE)]
[numthreads(8,8,1)]
void Resample(uint3 id : SV_DispatchThreadID)
{
    uint2 pixel = id.xy;
    if (any(pixel >= DestinationSize)) return;
    uint2 target = pixel + DestinationOrigin;
    if (FilterMode == 5) { Destination[target] = 0; return; }
    if (FilterMode == 2) { Destination[target] = LoadClamped(int2(pixel)); return; }
    float2 position = (float2(pixel)+0.5)*float2(SourceExtent)/float2(DestinationSize)-0.5;
    if (FilterMode == 3) { Destination[target] = Bilinear(position); return; }
    if (FilterMode == 0)
    {
        float2 lo = float2(pixel)*float2(SourceExtent)/float2(DestinationSize);
        float2 hi = float2(pixel+1)*float2(SourceExtent)/float2(DestinationSize);
        float4 sum = 0;
        for (int y=int(floor(lo.y)); y<int(ceil(hi.y)); ++y)
            for (int x=int(floor(lo.x)); x<int(ceil(hi.x)); ++x)
            {
                float2 w = max(min(hi,float2(x+1,y+1))-max(lo,float2(x,y)),0);
                sum += LoadClamped(int2(x,y))*w.x*w.y;
            }
        Destination[target] = sum/max((hi.x-lo.x)*(hi.y-lo.y),1e-6);
        return;
    }
    const float3 luma = float3(0.2126,0.7152,0.0722);
    float4 native = NativeColor.Load(int3(pixel,0));
#if NR_EDGE_DEPTH
    const bool visualizeEdgeMask = EdgeProtection < 0 && DepthAvailable != 0;
    float depthEdge = 0;
    float edgeStrength = saturate(abs(EdgeProtection));
    float edgeThickness = clamp(EdgeThickness,0,6);
    float edgeShift = clamp(EdgeShift,-6,6);
    float edgeSoftness = saturate(EdgeSoftness);
    if (EdgeProtection != 0 && DepthAvailable != 0 && edgeThickness > 0)
    {
        // Sample at most four rings spanning the active shifted mask. Scanning all
        // twelve radii at native ultrawide resolution can starve presentation.
        static const int2 directions[8] = {
            int2(-1,-1),int2(0,-1),int2(1,-1),int2(-1,0),
            int2(1,0),int2(-1,1),int2(0,1),int2(1,1) };
        float centerDepth = DepthAt(int2(pixel));
        float maxDepthJump = 0;
        int firstRadius = max(1,(int)floor(max(abs(edgeShift)-edgeThickness,0)));
        int lastRadius = min(12,(int)ceil(abs(edgeShift)+edgeThickness));
        int ringCount = min(lastRadius-firstRadius+1,4);
        [unroll] for (int sample=0; sample<4; ++sample)
        {
            if (sample >= ringCount) break;
            int radius = ringCount == 1 ? firstRadius :
                firstRadius+((lastRadius-firstRadius)*sample+(ringCount-1)/2)/(ringCount-1);
            [unroll] for (int direction=0; direction<8; ++direction)
            {
                int2 neighborPixel = int2(pixel)+directions[direction]*radius;
                float neighborDepth = DepthAt(neighborPixel);
                float depthJump = abs(neighborDepth-centerDepth) /
                    max(max(abs(neighborDepth),abs(centerDepth)),1e-3);
                bool centerIsNearer = DepthInverted != 0 ? centerDepth > neighborDepth : centerDepth < neighborDepth;
                float signedDistance = centerIsNearer ? float(radius) : -float(radius);
                float distance = abs(signedDistance-edgeShift);
                float feather = 1+edgeSoftness*edgeThickness;
                float coverage = distance <= edgeThickness+1-feather ? 1 :
                    saturate((edgeThickness+1-distance)/feather);
                maxDepthJump = max(maxDepthJump,depthJump*coverage);
            }
        }
        depthEdge = smoothstep(0.03,0.15,maxDepthJump);
    }
#else
    const bool visualizeEdgeMask = false;
#endif
    // Zero transfer bypasses the neural edit, not the independent sharpening.
    if (((TransferStrength == 0 && Sharpness == 0) || DetailStrength == 0) && !visualizeEdgeMask)
        { Destination[target] = native; return; }
    const bool supersampled = any(SourceExtent > DestinationSize);
    float3 input = (supersampled ? SampleArea(SmallInput,pixel) : SampleSmall(SmallInput,position)).rgb;
    float3 output = (supersampled ? SampleArea(Source,pixel) : SampleSmall(Source,position)).rgb;
    float3 edit = output - (FilterMode == 1 ? input : native.rgb);
    float yEdit = dot(edit,luma);
#if NR_EDGE_DEPTH
    // Detect strong edges introduced by this pass but absent from its exact
    // input. These include displaced temporal silhouettes outside current depth.
    float residualEdge = 0;
    if (EdgeProtection != 0)
    {
        static const int2 cross[4] = {int2(-1,0),int2(1,0),int2(0,-1),int2(0,1)};
        float outputY = dot(output,luma);
        float nativeY = dot(native.rgb,luma);
        [unroll] for (int direction=0; direction<4; ++direction)
        {
            float neighborOutputY = dot(MappedAt(Source,int2(pixel)+cross[direction]),luma);
            float neighborNativeY = dot(NativeAt(int2(pixel)+cross[direction]),luma);
            float outputGradient = abs(outputY-neighborOutputY);
            float nativeGradient = abs(nativeY-neighborNativeY);
            float signal = max(max(abs(nativeY),abs(neighborNativeY)),0.1);
            float introduced = max(outputGradient-nativeGradient*1.25,0)/(signal+0.05);
            residualEdge = max(residualEdge,smoothstep(0.04,0.18,introduced));
        }
    }
    float protectionMask = max(depthEdge,residualEdge);
    protectionMask = lerp(protectionMask,sqrt(saturate(protectionMask)),edgeSoftness);
    // Reduce only later-pass neural transfer where the combined mask rejects it.
    // The signed value carries visualization state; its magnitude remains active.
    float effectiveTransfer = TransferStrength*(1-protectionMask*edgeStrength);
    float3 result = native.rgb + (yEdit + (edit-yEdit)*ColorStrength)*effectiveTransfer;
#else
    float3 result = native.rgb + (yEdit + (edit-yEdit)*ColorStrength)*TransferStrength;
#endif
    // Retain the residual highlight guard without clamping signed scRGB.
    float origY = dot(max(native.rgb,0),luma);
    float inY = dot(max(input,0),luma), outY = dot(max(output,0),luma);
    float resultY = dot(result,luma);
    float limit = max(origY*2.5,origY*(outY+1.0/512.0)/(inY+1.0/512.0)*1.5+0.1);
    if (FilterMode == 1 && resultY > limit && inY > 1e-5) result *= limit/resultY;
    if (Sharpness > 0)
    {
        // Reuse the residual path's spatial anchor in both modes. Raw NR
        // neighbours bypass transfer/colour and amplify rejected neural noise.
        float3 e = NativeAt(int2(pixel)+int2(1,0));
        float3 w = NativeAt(int2(pixel)-int2(1,0));
        float3 n = NativeAt(int2(pixel)-int2(0,1));
        float3 s = NativeAt(int2(pixel)+int2(0,1));
        float3 center = native.rgb;
        float minY = min(dot(center,luma),min(min(dot(e,luma),dot(w,luma)),min(dot(n,luma),dot(s,luma))));
        float maxY = max(dot(center,luma),max(max(dot(e,luma),dot(w,luma)),max(dot(n,luma),dot(s,luma))));
        float range = maxY-minY;
        float gain = Sharpness*(0.2+0.8*saturate(1-range/(abs(maxY)+1e-4)));
        if (range > 1e-5) result += (center-(e+w+n+s)*0.25)*gain;
    }
    // Independent output adjustment; the neutral branch leaves the existing
    // resolve arithmetic untouched. No extra NR evaluation or history advance.
    if (DetailStrength < 1) result = lerp(native.rgb, result, DetailStrength);
    else if (DetailStrength > 1) {
        float nativeY = dot(native.rgb, luma), neuralY = dot(result, luma);
        float3 stable = result;
        // Avoid unstable ratios near black and preserve signed HDR values.
        // A common positive RGB multiplier preserves the completed result's hue.
        if (nativeY > 1e-4 && neuralY > 1e-4) {
            float ratio = clamp(neuralY / nativeY, 0.25, 4.0);
            stable *= pow(ratio, DetailStrength - 1);
        }
        float3 coupled = native.rgb + (result - native.rgb) * DetailStrength;
        result = stable + (coupled - stable) * ColourCoupling;
    }
#if NR_EDGE_DEPTH
    if (visualizeEdgeMask)
    {
        float visibility = protectionMask*edgeStrength;
        Destination[target] = float4(lerp(result,float3(0,1,0),visibility),native.a);
        return;
    }
#endif
    Destination[target] = float4(result,native.a);
}
