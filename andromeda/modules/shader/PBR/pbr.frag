#version 460 core

const float PI = 3.14159265359;
const uint SAMPLE_COUNT = 4096u;
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

layout (location = 0) out vec4 FragColor;
layout (location = 0) in vec2 TexCoords;
layout (location = 1) in vec3 WorldPos;
layout (location = 2) in vec3 Normal;
layout (binding = 0) uniform samplerCube irradianceMap;
layout (binding = 1) uniform samplerCube prefilterMap;
layout (binding = 2) uniform sampler2D brdfLUT;
layout (std140, binding = 2) uniform pbrMaterial {
    // @Editor: Color
    vec3  albedo;
    // @Editor: Slider(0.0, 1.0)
    float metallic;
    // @Editor: Slider(0.0, 1.0)
    float roughness;
    // @Editor: Slider(0.0, 1.0)
    float ao;
    uint  textureFlags;
};

/* Which of the maps below actually carry data. A material without maps keeps the plain
   UBO values, which is how every material behaved before textures existed. */
const uint HAS_ALBEDO_MAP    = 1u;
const uint HAS_ROUGHNESS_MAP = 2u;
const uint HAS_NORMAL_MAP    = 4u;
const uint HAS_EMISSIVE_MAP  = 8u;

layout (binding = 3) uniform sampler2D albedoMap;
layout (binding = 4) uniform sampler2D roughnessMap;
layout (binding = 5) uniform sampler2D normalMap;
layout (binding = 6) uniform sampler2D emissiveMap;

/**
 * Rebuilds a tangent frame from screen-space derivatives instead of a vertex attribute.
 * The Vertex struct carries no tangent, and deriving one here avoids changing the vertex
 * layout, the VAO and every shader that shares it. Accurate enough for surface detail.
 */
vec3 applyNormalMap(vec3 N, vec3 worldPos, vec2 uv)
{
    vec3 tangentNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;

    vec3 dPosdx = dFdx(worldPos);
    vec3 dPosdy = dFdy(worldPos);
    vec2 dUVdx  = dFdx(uv);
    vec2 dUVdy  = dFdy(uv);

    vec3 T = dPosdx * dUVdy.y - dPosdy * dUVdx.y;
    if (dot(T, T) < 1e-12) {
        return N;  // degenerate UVs, keep the geometric normal
    }
    T = normalize(T - N * dot(N, T));
    vec3 B = normalize(cross(N, T));

    return normalize(mat3(T, B, N) * tangentNormal);
}

layout (std140, binding = 3) uniform lights {
    vec4 lightPositions[4];
    vec4 lightColors[4];
};

layout (std140, binding = 0) uniform CameraBuffer {
    mat4 viewMatrix;
    mat4 projMatrix;
    vec3 camPos; 
};
void main()
{
    vec3 N = normalize(Normal);

    // The maps override the flat UBO values where they exist. A material without any map
    // keeps them unchanged, so untextured objects render exactly as they did before.
    vec3  baseColor = albedo;
    float rough     = roughness;
    float metal     = metallic;
    vec3  emission  = vec3(0.0);

    if ((textureFlags & HAS_ALBEDO_MAP) != 0u) {
        baseColor = texture(albedoMap, TexCoords).rgb;
    }
    if ((textureFlags & HAS_ROUGHNESS_MAP) != 0u) {
        rough = clamp(texture(roughnessMap, TexCoords).r, 0.05, 1.0);
    }
    if ((textureFlags & HAS_NORMAL_MAP) != 0u) {
        N = applyNormalMap(N, WorldPos, TexCoords);
    }
    if ((textureFlags & HAS_EMISSIVE_MAP) != 0u) {
        emission = texture(emissiveMap, TexCoords).rgb;
    }

    vec3 V = normalize(camPos - WorldPos);
    vec3 R = reflect(-V, N);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, baseColor, metal);

    vec3 Lo = vec3(0.0);
    for(int i = 0; i < 4; ++i)
    {
        vec3 L = normalize(lightPositions[i].xyz - WorldPos);
        vec3 H = normalize(V + L);
        float distance    = length(lightPositions[i].xyz - WorldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance     = lightColors[i].rgb * attenuation;

        float NDF = DistributionGGX(N, H, rough);
        float G   = GeometrySmith(N, V, L, rough);
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metal;

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * baseColor / PI + specular) * radiance * NdotL;
    }
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, rough);
    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metal;
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse      = irradiance * baseColor;

    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, R,  rough * MAX_REFLECTION_LOD).rgb;
    vec2 brdf  = texture(brdfLUT, vec2(max(dot(N, V), 0.0), rough)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    vec3 ambient = (kD * diffuse + specular) * ao;

    vec3 color = ambient + Lo + emission;

    FragColor = vec4(color, 1.0);
} 