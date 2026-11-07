#version 450
// A circle by its distance: a soft edge one pixel wide and a brighter core, added to the trails.
layout(location = 0) in vec2 InLocal;
layout(location = 1) in vec4 InTint;
layout(location = 0) out vec4 Out;

void main() {
    float distance = length(InLocal);
    float edge = fwidth(distance);
    float shape = 1.0 - smoothstep(1.0 - edge, 1.0, distance);
    float core = exp(-distance * distance * 4.0);
    Out = vec4(InTint.rgb * (0.6 + 1.4 * core), InTint.a * shape);
}
