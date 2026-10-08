#version 460 core

// Display conversion for the asset preview thumbnails.
//
// The PBR shader writes linear HDR, and the editor viewport only becomes display-ready in the
// post-processing pass (outline.frag). The thumbnails do not run through that pass - they would be
// noticeably darker and more contrasty than the same model in the scene - so this shader applies
// the same two steps on its own: ACES tonemapping, then gamma encoding.
//
// It deliberately does not reuse outline.frag: that one also samples the selection mask to draw
// the outline, which would need a guaranteed-zero texture bound here and would tie the thumbnails
// to the editor's outline feature. The ACES curve below must stay identical to the one in
// outline.frag, otherwise a thumbnail no longer matches its object in the viewport.
//
// It also composites the tile background. The preview pass clears to alpha 0 and the PBR shader
// writes alpha 1, so the alpha channel says where the model is; the multisample resolve turns that
// into fractional coverage along the silhouette, which gives a soft edge against the gradient for
// free. The gradient is mixed in AFTER tonemapping, so the grey values below are the ones that end
// up on screen rather than something the ACES curve has moved.

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 TexCoords;

layout(binding = 10) uniform sampler2D fboSampler;

// Neutral greys, in display space. Lighter in the middle so the model reads against it, darker
// towards the corners so the tiles stay separable on the panel background.
const vec3 kBgCenter = vec3(0.30);
const vec3 kBgCorner = vec3(0.14);

vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec4 scene = texture(fboSampler, TexCoords);

    vec3 lit = ACESFilm(scene.rgb);
    lit = pow(lit, vec3(1.0 / 2.2));

    // 0 at the centre of the tile, 1 in its corners.
    float dist = length(TexCoords - vec2(0.5)) / 0.70710678;
    vec3 background = mix(kBgCenter, kBgCorner, smoothstep(0.0, 1.0, dist));

    FragColor = vec4(mix(background, lit, scene.a), 1.0);
}
