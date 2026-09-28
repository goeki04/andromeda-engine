#version 460 core

struct Particle{
    vec4 position; //xyz = position, w = remaining lifetime
    vec4 velocity; //xyz = velocity, w = max lifetime
    vec4 params; // x = size, yzw = freie Parameter (z.B. Rotation, TypeID)
};

layout (std430, binding = 1) buffer particleBuffer{
    Particle particles[];
};

layout (std140, binding = 2) uniform CameraData{
    mat4 proj;
    mat4 view;
};
layout (location = 1) out float vLifeFade;

void main(){
    uint index = gl_InstanceID;

    vec3 worldPos = particles[index].position.xyz;
    float remainingLife = particles[index].position.w;
    float maxLife = particles[index].velocity.w;
    if(remainingLife <= 0.0){
        gl_Position = vec4(0.0,0.0,0.0,0.0);
        gl_PointSize = 0.0;
        return;
    }
 
    float fadeOut = clamp(remainingLife / (maxLife * 0.25), 0.0, 1.0);
    float age = maxLife - remainingLife;   // = 0
    float fadeIn = clamp(age / (maxLife * 0.15), 0.0, 1.0);   // = 0
    vLifeFade = min(fadeIn, fadeOut);      // = 0 → alpha 0

    vec4 viewPos = view * vec4(worldPos, 1.0);
    float dist = length(viewPos.xyz);

    gl_Position = proj * viewPos;
    gl_PointSize = clamp(200.0 / dist, 2.0, 40.0);
}