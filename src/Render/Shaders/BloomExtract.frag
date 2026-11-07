#version 450
// The bright part of the scene, at half resolution.
layout(set = 2, binding = 0) uniform sampler2D Scene;
layout(set = 3, binding = 0) uniform Params {
    vec4 Threshold; // threshold, -, -, -
};
layout(location = 0) in vec2 InUv;
layout(location = 0) out vec4 Out;

void main() {
    vec3 c = texture(Scene, InUv).rgb;
    float level = max(max(c.r, c.g), c.b);
    Out = vec4(c * smoothstep(Threshold.x, Threshold.x + 0.25, level), 1.0);
}
