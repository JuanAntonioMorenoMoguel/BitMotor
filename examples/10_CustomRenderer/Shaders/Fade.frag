#version 450
// Last frame's trails, a little darker: what the circles leave behind.
layout(set = 2, binding = 0) uniform sampler2D Previous;
layout(set = 3, binding = 1) uniform Fade {
    vec4 Amount; // x: how much of last frame stays (0..1)
};
layout(location = 0) in vec2 InUv;
layout(location = 0) out vec4 Out;

void main() { Out = vec4(texture(Previous, InUv).rgb * Amount.x, 1.0); }
