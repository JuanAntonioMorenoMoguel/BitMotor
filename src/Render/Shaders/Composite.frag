#version 450
// The lit scene + bloom onto the screen target, with the screen effects: a soft tone curve (when lit),
// chromatic aberration, saturation, vignette, grain and the screen flash. The Hud is drawn after this.
layout(set = 2, binding = 0) uniform sampler2D Scene;
layout(set = 2, binding = 1) uniform sampler2D Bloom;
layout(set = 3, binding = 0) uniform Params {
    vec4 A; // bloom strength, tone curve on, time, linear colour
    vec4 B; // vignette, grain, aberration (pixels), saturation
    vec4 C; // flash rgb, flash amount
    vec4 D; // screen size xy, -, -
};
layout(location = 0) in vec2 InUv;
layout(location = 0) out vec4 Out;

float Hash(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 ToneCurve(vec3 c) {
    // Linear up to 0.8 and a smooth shoulder above: the palette stays, bright lights do not clip harshly.
    vec3 over = max(c - 0.8, 0.0);
    return min(c, 0.8) + 0.2 * (1.0 - exp(-over / 0.2));
}

void main() {
    vec3 color;
    if (B.z > 0.0) {
        vec2 shift = (InUv - 0.5) * B.z / D.xy;
        color = vec3(texture(Scene, InUv + shift).r, texture(Scene, InUv).g, texture(Scene, InUv - shift).b);
    } else {
        color = texture(Scene, InUv).rgb;
    }
    if (A.x > 0.0) color += texture(Bloom, InUv).rgb * A.x;
    if (A.y > 0.5) color = ToneCurve(color);
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(vec3(luma), color, B.w);
    if (B.x > 0.0) {
        vec2 q = InUv - 0.5;
        color *= 1.0 - B.x * smoothstep(0.25, 0.75, length(q * vec2(1.0, 0.85)) * 1.15);
    }
    if (B.y > 0.0) {
        // Grain is even to the eye: in linear light it goes on the sRGB-like encoding, or the darks get noisy.
        float grain = (Hash(gl_FragCoord.xy + fract(A.z * 7.13) * 311.0) - 0.5) * B.y;
        if (A.w > 0.5)
            color = pow(max(pow(max(color, 0.0), vec3(1.0 / 2.2)) + grain, 0.0), vec3(2.2));
        else
            color += grain;
    }
    color = mix(color, C.rgb, C.a);
    Out = vec4(clamp(color, 0.0, 1.0), 1.0);
}
