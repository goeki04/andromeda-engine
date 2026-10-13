#version 460 core

layout (location = 0) out vec4 FragColor;
layout (location = 0) in vec3 localPos;

layout (binding = 0) uniform samplerCube environmentMap;

// How bright the sky is meant to sit, as a factor on the raw HDR environment value. This is a
// by-eye number, not a physical one.
const float kSkyBrightness = 0.5;

// The post-processing pass multiplies the whole scene by its exposure before tonemapping. Dividing
// it out here keeps the sky at the brightness it had before that stage existed, which is what the
// look was tuned for. The consequence is that the sky does NOT follow the exposure: if you change
// kExposure in outline.frag, change it here too, or the sky drifts away from the rest of the scene.
const float kExposure = 0.35;

void main(){
    vec3 envColor = texture(environmentMap, localPos).rgb;
    envColor *= kSkyBrightness / kExposure;

    FragColor = vec4(envColor, 1.0);
}