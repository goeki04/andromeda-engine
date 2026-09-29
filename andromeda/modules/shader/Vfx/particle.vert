#version 460 core

struct Particle{
    vec4 position; //xyz = position, w = remaining lifetime
    vec4 velocity; //xyz = velocity, w = max lifetime
    vec4 params; // x = size, yzw
};

layout (std430, binding = 1) buffer particleBuffer{
    Particle particles[];
};

layout (std140, binding = 2) uniform CameraData{
    mat4 proj;
    mat4 view;
};

layout (location = 1) out float vLifeFade;
layout (location = 2) out vec3 vColor;

void main(){
    uint index = gl_InstanceID;

    vec3 worldPos = particles[index].position.xyz;
    float remainingLife = particles[index].position.w;
    float maxLife = particles[index].velocity.w;
    float size = particles[index].params.x;

    if (remainingLife <= 0.0) {
        gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
        gl_PointSize = 0.0;
        return;
    }

    vColor = particles[index].params.yzw;

    float fadeOut = clamp(remainingLife / (maxLife * 0.45), 0.0, 1.0);
    float age = maxLife - remainingLife;
    float fadeIn = clamp(age / (maxLife * 0.12), 0.0, 1.0);
    vLifeFade = min(fadeIn, fadeOut);

    vec4 viewPos = view * vec4(worldPos, 1.0);
    float dist = max(length(viewPos.xyz), 0.001);

    gl_Position = proj * viewPos;

    gl_PointSize = clamp(size * 120.0 / sqrt(dist), 1.0, 128.0);
}