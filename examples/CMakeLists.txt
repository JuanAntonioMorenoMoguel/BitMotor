# BitMotor examples: each one is a folder with a single, commented Main.cpp.
# They also run in the CI with `--frames 60`.
BitMotor_Example(00_Window)
BitMotor_Example(01_Loop)

if(BITMOTOR_TESTS)
    add_test(NAME Example_00_Window COMMAND BitMotor_00_Window --frames 60)
    add_test(NAME Example_01_Loop COMMAND BitMotor_01_Loop --frames 120)
endif()
