// BitMotor's Gpu::Globals, uniform block 0 of every shader (std140, 256 bytes). Include it in a vertex
// shader with GLOBALS_SET 1 and in a fragment shader with GLOBALS_SET 3.
layout(set = GLOBALS_SET, binding = 0) uniform Globals {
    vec4 Clock;  // Time::Total, Time::Delta, real seconds, frame number
    vec4 Screen; // width, height, 1 / width, 1 / height
    vec4 Mouse;  // x, y in screen pixels
    vec4 Values[13]; // Gpu::SetGlobal
};
