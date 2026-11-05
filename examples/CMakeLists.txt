# BitMotor examples: each one is a folder with a single, commented Main.cpp.
# They also run in the CI with `--frames 60`.
BitMotor_Example(00_Window)

if(BITMOTOR_TESTS)
    add_test(NAME Example_00_Window COMMAND BitMotor_00_Window --frames 60)
endif()
