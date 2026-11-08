#version 450
// A soft dot, added to the scene.
layout(location = 0) in vec2 InLocal;
layout(location = 1) in vec4 InTint;
layout(location = 0) out vec4 Out;

void main() {
    float d = dot(InLocal, InLocal);
    Out = vec4(InTint.rgb, InTint.a * exp(-d * 3.0));
}
