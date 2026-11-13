#version 450
// A model's pixels: texture x colour, lit by the ambient, the sun and up to 8 point lights, optionally in
// bands with ordered dithering like the 2D lighting. Into the HDR scene. SDL_GPU: samplers in set 2,
// fragment uniforms in set 3.
// Physically based materials (Surface.w on): metal/roughness with GGX highlights, a normal map whose tangents
// come from the screen derivatives of the position and the UVs, baked occlusion and an emission map. Off, it
// is the plain diffuse light of before (Lambert, wrapped for the point lights), to the bit.
layout(set = 2, binding = 0) uniform sampler2D Image;
layout(set = 2, binding = 1) uniform sampler2D NormalMap;
layout(set = 2, binding = 2) uniform sampler2D MetalRoughMap;
layout(set = 2, binding = 3) uniform sampler2D OcclusionMap;
layout(set = 2, binding = 4) uniform sampler2D EmissionMap;
layout(set = 3, binding = 0) uniform Light {
    vec4 Sun;      // direction the light travels (xyz), -
    vec4 SunColor; // rgb x intensity, -
    vec4 Ambient;  // rgb, stepped
    vec4 Options;  // emission, unlit, linear colour, light count
    vec4 LightPosition[8]; // xyz, radius
    vec4 LightColor[8];    // rgb x intensity, -
    vec4 Eye;      // camera position xyz, alpha cutoff
    vec4 Surface;  // metallic, roughness, physical on, -
    vec4 Maps;     // normal scale (0: no map), metal-rough map on, occlusion strength (0: no map), emission map on
};

layout(location = 0) in vec3 InWorld;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InUv;
layout(location = 3) in vec4 InColor;
layout(location = 0) out vec4 Out;

const float Pi = 3.14159265;

vec3 SrgbToLinear(vec3 c) { return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c)); }

float Bayer4(vec2 p) {
    ivec2 i = ivec2(mod(p, 4.0));
    int m[16] = int[](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
    return float(m[i.y * 4 + i.x]) / 16.0;
}

// The normal map's normal in the world, with a tangent frame built from the derivatives (no vertex tangents).
vec3 MappedNormal(vec3 n) {
    vec3 dp1 = dFdx(InWorld), dp2 = dFdy(InWorld);
    vec2 duv1 = dFdx(InUv), duv2 = dFdy(InUv);
    vec3 dp2perp = cross(dp2, n), dp1perp = cross(n, dp1);
    vec3 t = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 b = dp2perp * duv1.y + dp1perp * duv2.y;
    float scale = max(dot(t, t), dot(b, b));
    if (scale < 1e-20) return n; // no UVs: nothing to bend
    // UV (0, 0) is the top left here, so V grows downwards: the map's green (up) is -b.
    float inverse = inversesqrt(scale);
    vec3 m = texture(NormalMap, InUv).xyz * 2.0 - 1.0;
    m.xy *= Maps.x;
    return normalize(t * inverse * m.x - b * inverse * m.y + n * m.z);
}

// GGX / Trowbridge-Reitz distribution, Smith-Schlick visibility and Schlick's Fresnel.
vec3 Highlight(vec3 n, vec3 v, vec3 l, vec3 f0, float roughness) {
    vec3 h = v + l;
    h = dot(h, h) > 1e-12 ? normalize(h) : n;
    // Clamped to [0, 1]: a dot product a hair above 1 would make pow() a NaN, which bloom spreads in squares.
    float nl = clamp(dot(n, l), 0.0, 1.0), nv = clamp(dot(n, v), 1e-4, 1.0), nh = clamp(dot(n, h), 0.0, 1.0);
    float vh = clamp(dot(v, h), 0.0, 1.0);
    float a = roughness * roughness, a2 = a * a;
    float q = nh * nh * (a2 - 1.0) + 1.0;
    float d = a2 / (Pi * q * q);
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float g = 1.0 / ((nl * (1.0 - k) + k) * (nv * (1.0 - k) + k));
    vec3 f = f0 + (1.0 - f0) * pow(1.0 - vh, 5.0);
    // x Pi: the lights here are in the "diffuse = colour x light" convention (Lambert without the 1/Pi).
    return f * (d * g * 0.25 * Pi);
}

// How much of the ambient light a surface reflects (Karis' fit of the split-sum lookup).
vec3 AmbientHighlight(vec3 f0, float roughness, float nv) {
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * nv)) * r.x + r.y;
    vec2 ab = vec2(-1.04, 1.04) * a004 + r.zw;
    return f0 * ab.x + ab.y;
}

void main() {
    vec4 texel = texture(Image, InUv);
    if (Options.z > 0.5) texel.rgb = SrgbToLinear(texel.rgb);
    vec4 albedo = texel * InColor;
    if (Eye.w > 0.0) {
        if (albedo.a < Eye.w) discard;
        albedo.a = 1.0;
    }
    if (albedo.a < 0.004) discard;
    vec3 glow = albedo.rgb * Options.x * 1.5;
    if (Maps.w > 0.5) {
        vec3 e = texture(EmissionMap, InUv).rgb;
        glow = (Options.z > 0.5 ? SrgbToLinear(e) : e) * Options.x * 1.5;
    }
    if (Options.y > 0.5) {
        Out = vec4(albedo.rgb + glow, albedo.a);
        return;
    }
    vec3 n = normalize(InNormal);
    if (!gl_FrontFacing) n = -n;
    int count = int(Options.w);
    vec3 color;
    if (Surface.z < 0.5) {
        vec3 light = Ambient.rgb + SunColor.rgb * max(dot(n, -Sun.xyz), 0.0);
        for (int i = 0; i < count; ++i) {
            vec3 d = LightPosition[i].xyz - InWorld;
            float distance = length(d);
            float fade = clamp(1.0 - distance / max(LightPosition[i].w, 0.001), 0.0, 1.0);
            if (fade <= 0.0) continue;
            fade *= fade;
            light += LightColor[i].rgb * fade * (0.25 + 0.75 * max(dot(n, d / max(distance, 1e-4)), 0.0));
        }
        if (Ambient.w > 0.5) {
            float level = max(max(light.r, light.g), light.b);
            float perceived = Options.z > 0.5 ? pow(level, 1.0 / 2.2) : level;
            float banded = floor(perceived * 7.0 + Bayer4(floor(gl_FragCoord.xy)) * 0.9) / 7.0;
            if (Options.z > 0.5) banded = pow(banded, 2.2);
            light *= banded / max(level, 1e-4);
        }
        color = albedo.rgb * light;
    } else {
        if (Maps.x != 0.0) n = MappedNormal(n);
        float metallic = Surface.x, roughness = Surface.y;
        if (Maps.y > 0.5) {
            vec4 mr = texture(MetalRoughMap, InUv);
            roughness *= mr.g;
            metallic *= mr.b;
        }
        roughness = clamp(roughness, 0.045, 1.0);
        float occlusion = Maps.z > 0.0 ? mix(1.0, texture(OcclusionMap, InUv).r, Maps.z) : 1.0;
        vec3 v = normalize(Eye.xyz - InWorld);
        float nv = clamp(dot(n, v), 1e-4, 1.0);
        vec3 f0 = mix(vec3(0.04), albedo.rgb, metallic);
        vec3 diffuse = albedo.rgb * (1.0 - metallic);

        vec3 l = -Sun.xyz;
        float nl = max(dot(n, l), 0.0);
        color = (diffuse + Highlight(n, v, l, f0, roughness)) * SunColor.rgb * nl;
        for (int i = 0; i < count; ++i) {
            vec3 d = LightPosition[i].xyz - InWorld;
            float distance = length(d);
            float fade = clamp(1.0 - distance / max(LightPosition[i].w, 0.001), 0.0, 1.0);
            if (fade <= 0.0) continue;
            fade *= fade;
            vec3 pl = d / max(distance, 1e-4);
            color += (diffuse + Highlight(n, v, pl, f0, roughness)) * LightColor[i].rgb * fade * max(dot(n, pl), 0.0);
        }
        color += (diffuse + AmbientHighlight(f0, roughness, nv)) * Ambient.rgb * occlusion;
    }
    Out = vec4(color + glow, albedo.a);
}
