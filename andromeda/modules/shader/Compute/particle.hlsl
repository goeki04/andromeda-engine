
cbuffer EmitterSettings : register(b2)
{
    float3 emitterPosition;
    float deltaTime;
    
    float3 boxExtents;
    uint maxParticles;
    float3 startVelocity;
    float minLifetime;
    
    float4 color;
    
    float size;
    uint activeParticleCount;
    uint baseIndex;
    float2 padding;
};

struct Particle
{
    float4 position; // xyz = Position, w = remaining lifetime in seconds
    float4 velocity; // xyz = Velocity, w = max lifetime in seconds
    float4 params; // x = size, yzw = free parameters (rotation, type id, ...)
};

RWStructuredBuffer<Particle> particles : register(u1);

uint pcg_hash(uint inputValue)
{
    uint state = inputValue * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float hash1(uint seed)
{
    return float(pcg_hash(seed)) * (1.0f / 4294967295.0f);
}

float3 hash3(uint seed)
{
    uint x = pcg_hash(seed);
    uint y = pcg_hash(x);
    uint z = pcg_hash(y);
    return float3(x, y, z) * (1.0f / 4294967295.0f);
}

// Single entry point (matches the fixed "-E main" DXC invocation in
// CompileShaders.cmake). Combines the former RunInit/RunUpdate kernels:
// a particle buffer starts zero-initialized, so every slot is already
// "dead" (remainingLife <= 0) on the first dispatch and falls into the
// same respawn branch that later revivals also use - no separate init
// pass required. The randomized start-life formula that RunInit used is
// preserved inside that respawn branch via hash1(index).
[numthreads(256, 1, 1)]
void BoxParticle(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint local = dispatchThreadId.x;
    if (local >= maxParticles)
        return;
    uint index = baseIndex + local;
    Particle p = particles[index];
    float remainingLife = p.position.w - deltaTime;

    if (remainingLife <= 0.0f)
    {
        // params.y counts respawns so the seed changes every cycle: with a plain
        // hash3(index) the particle would reappear at the exact same spot forever.
        const bool firstSpawn = p.velocity.w <= 0.0f;
        const uint cycle = uint(p.params.y) + 1u;
        const uint seed = index * 9781u + cycle;

        float3 random = hash3(seed) * 2.0f - 1.0f; // -1 .. 1 per axis
        float life = max(minLifetime, 0.1f) * (0.5f + hash1(seed));

        p.position.xyz = emitterPosition + random * boxExtents;
        p.velocity = float4(startVelocity, life); // w = max lifetime, the fade reads it
        p.params.x = size;
        p.params.y = float(cycle);

        // A zero-filled buffer spawns every slot in the same frame, so the first
        // generation gets a random head start; later respawns start one frame old
        // so the vertex shader's fade-in is never exactly zero.
        // max() guards a frame spike longer than the lifetime: a negative value here
        // would leave the slot dead and make it respawn again on the very next frame.
        remainingLife = firstSpawn ? life * hash1(seed + 0x9E3779B9u)
                                   : max(life - deltaTime, 0.0001f);
    }
    else
    {
        p.position.xyz += p.velocity.xyz * deltaTime;
    }

    p.position.w = remainingLife;
    particles[index] = p;
}

// Uniform sampling on the sphere surface (Y-up pole axis): theta = azimuth
// around Y, y = cos(polar angle) remapped to [-1,1] so the whole sphere is
// covered. saturate() guards the sqrt against tiny negative values from
// floating-point rounding when y is close to +-1.
[numthreads(256, 1, 1)]
void SphereParticle(uint3 dispatchThreadId : SV_DispatchThreadID)
{
}

// Point inside a cone along +Y: height picks a random slice along the
// axis, radius at that slice is bounded by tan(coneAngle)*height, and
// sqrt(random.z) keeps the in-disc sampling area-uniform (not just
// edge-biased) for that slice.
[numthreads(256, 1, 1)]
void ConeParticle(uint3 dispatchThreadId : SV_DispatchThreadID)
{
  
}

// Point inside a cylinder along +Y: height is uniform along the axis,
// radius is area-uniform across the circular cross-section (sqrt(random.z)
// avoids clustering samples near the center, same trick as ConeParticle).
[numthreads(256, 1, 1)]
void CylinderParticle(uint3 dispatchThreadId : SV_DispatchThreadID)
{
   
}

[numthreads(256, 1, 1)]
void HemisphereParticle(uint3 dispatchThreadId : SV_DispatchThreadID)
{
}
