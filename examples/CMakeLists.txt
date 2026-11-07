# BitMotor examples: each one is a folder with a single, commented Main.cpp.
# They also run in the CI with `--frames 60`.
BitMotor_Example(00_Window)
BitMotor_Example(01_Loop)
BitMotor_Example(02_Input)
BitMotor_Example(03_Scenes)
BitMotor_Example(04_Sprites COPY 04_Sprites/Assets)
BitMotor_Example(05_Lighting COPY 04_Sprites/Assets)
BitMotor_Example(06_Particles COPY 04_Sprites/Assets)

if(BITMOTOR_TESTS)
    add_test(NAME Example_00_Window COMMAND BitMotor_00_Window --frames 60)
    add_test(NAME Example_01_Loop COMMAND BitMotor_01_Loop --frames 120)
    add_test(NAME Example_02_Input COMMAND BitMotor_02_Input --frames 120)
    add_test(NAME Example_03_Scenes COMMAND BitMotor_03_Scenes --frames 600)
    add_test(NAME Example_04_Sprites COMMAND BitMotor_04_Sprites --frames 240)
    add_test(NAME Example_05_Lighting COMMAND BitMotor_05_Lighting --frames 240)
    add_test(NAME Example_06_Particles COMMAND BitMotor_06_Particles --frames 240)
endif()
