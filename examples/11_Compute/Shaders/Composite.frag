#version 450
// The HDR scene onto the screen with a soft tone curve.
layout(set = 2, binding = 0) uniform sampler2D Scene;
layout(location = 0) in vec2 InUv;
layout(location = 0) out vec4 Out;

void main() {
    vec3 hdr = texture(Scene, InUv).rgb;
    Out = vec4(hdr / (1.0 + hdr), 1.0);
}
