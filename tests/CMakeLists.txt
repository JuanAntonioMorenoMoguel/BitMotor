# BitMotor's internal tests: one executable registered with ctest.
#     BitMotorTests          runs everything
#     BitMotorTests Log      only the tests whose name contains "Log"
file(GLOB_RECURSE BITMOTOR_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
add_executable(BitMotorTests ${BITMOTOR_TEST_SOURCES})
target_link_libraries(BitMotorTests PRIVATE BitMotor::BitMotor)
# ../src: the tests also reach BitMotor's internal Detail functions (SetArgs, SetSavePath...).
target_include_directories(BitMotorTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(BitMotorTests PRIVATE BITMOTOR_EXPECTED_VERSION="${PROJECT_VERSION}")
_BitMotor_Warnings(BitMotorTests)
add_test(NAME BitMotorTests COMMAND BitMotorTests)
# A TrueType font for the Font::Load tests (the one example 13 uses).
target_compile_definitions(BitMotorTests PRIVATE
    BITMOTOR_TEST_FONT="${PROJECT_SOURCE_DIR}/examples/13_Extras/Assets/fonts/PressStart2P.ttf")
