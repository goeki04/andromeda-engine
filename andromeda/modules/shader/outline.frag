#version 460 core

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(binding = 10) uniform sampler2D fboSampler;
layout(binding = 11) uniform sampler2D maskSampler;

layout(binding = 0) uniform OutlineParamsBuffer {
    vec2 texelSize;
};

// Scales linear scene radiance into the range the tonemapper expects, the way a camera's exposure
// does. Without it the scene arrives far above 1.0 - the four point lights plus the bright sky
// irradiance add up to roughly 2-3 on a lit mid-grey surface - and ACES flattens everything to
// white. Tuned by eye; it has to stay in step with the same constant in preview_post.frag, or a
// thumbnail no longer matches its object in the viewport.
const float kExposure = 0.35;

vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec4 sceneColor = texture(fboSampler, TexCoords);
    float mask = texture(maskSampler, TexCoords).r;
    

    // Exposure applies to scene radiance only - the outline colour below is a UI colour and is
    // meant to come out exactly as written.
    vec3 finalDisplayColor = sceneColor.rgb * kExposure;

    if (mask < 0.5) {
        float radius = 2.0; 
        float neighborMask = 0.0;
        
        neighborMask += texture(maskSampler, TexCoords + vec2(radius, 0.0) * texelSize).r;
        neighborMask += texture(maskSampler, TexCoords + vec2(-radius, 0.0) * texelSize).r;
        neighborMask += texture(maskSampler, TexCoords + vec2(0.0, radius) * texelSize).r;
        neighborMask += texture(maskSampler, TexCoords + vec2(0.0, -radius) * texelSize).r;

        if (neighborMask > 0.01) {
            finalDisplayColor = vec3(1.0, 0.5, 0.0);
        }
    }

    finalDisplayColor = ACESFilm(finalDisplayColor);
    finalDisplayColor = pow(finalDisplayColor, vec3(1.0 / 2.2));

    FragColor = vec4(finalDisplayColor, 1.0);
}