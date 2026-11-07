#version 450
// One direction of a 9-tap gaussian blur (run once across and once down).
layout(set = 2, binding = 0) uniform sampler2D Source;
layout(set = 3, binding = 0) uniform Params {
    vec4 Step; // one texel in the blur direction (uv), -, -
};
layout(location = 0) in vec2 InUv;
layout(location = 0) out vec4 Out;

void main() {
    const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 sum = texture(Source, InUv).rgb * weights[0];
    for (int i = 1; i < 5; ++i) {
        sum += texture(Source, InUv + Step.xy * float(i)).rgb * weights[i];
        sum += texture(Source, InUv - Step.xy * float(i)).rgb * weights[i];
    }
    Out = vec4(sum, 1.0);
}
