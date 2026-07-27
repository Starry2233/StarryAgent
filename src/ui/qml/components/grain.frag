#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float time;
    float intensity;
    float dark;
};

// Simple value noise for paper grain texture
float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float noise(vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);
    float a = rand(i);
    float b = rand(i + vec2(1.0, 0.0));
    float c = rand(i + vec2(0.0, 1.0));
    float d = rand(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

void main() {
    // Multi-octave noise for organic paper texture
    vec2 uv = qt_TexCoord0 * 1.5;
    float n = 0.0;
    n += noise(uv * 80.0 + time * 0.02) * 0.5;
    n += noise(uv * 160.0 - time * 0.015) * 0.25;
    n += noise(uv * 320.0 + time * 0.01) * 0.125;

    // Output only dark speckles for subtle grain
    // Use black with grain-modulated alpha for paper texture
    float grainAlpha = n * intensity * 3.5;

    // Pure black specks with moderate alpha for visibility
    fragColor = vec4(0.0, 0.0, 0.0, grainAlpha * qt_Opacity);
}
