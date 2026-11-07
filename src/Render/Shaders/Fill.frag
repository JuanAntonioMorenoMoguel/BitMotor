#version 450
// A flat colour (straight alpha), blended by the pipeline. SDL_GPU: fragment uniforms live in set 3.
layout(set = 3, binding = 0) uniform Fill {
    vec4 Tint;
};
layout(location = 0) out vec4 Out;

void main() { Out = Tint; }
