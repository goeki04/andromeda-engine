#version 460 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) in float vLifeFade;
layout(location = 2) in vec3 vColor;

layout (std140, binding = 4) uniform ParticleMaterial{
    vec4 particleColor;
};
void main(){
    float distance = length(gl_PointCoord - vec2(0.5)) * 2.0;
    if(distance > 1.0){
        discard;
    }

    float core = smoothstep(0.55, 0.0, distance);
    float halo = smoothstep(1.0, 0.35, distance) * 0.45;
    float alpha = (core + halo) * vLifeFade;
    FragColor = vec4(vColor * (0.6 + core * 0.8), alpha);
}