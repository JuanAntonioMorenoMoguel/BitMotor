// One particle as the storage buffer holds it (std430, 32 bytes), the same as `Particle` in Main.cpp.
struct Particle {
    vec2 Position; // screen pixels
    vec2 Velocity; // pixels per second
    vec4 Tint;
};
