# BitMotor's internal tests: one executable registered with ctest.
#     BitMotorTests          runs everything
#     BitMotorTests Log      only the tests whose name contains "Log"
file(GLOB_RECURSE BITMOTOR_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
add_executable(BitMotorTests ${BITMOTOR_TEST_SOURCES})
target_link_libraries(BitMotorTests PRIVATE BitMotor::BitMotor)
target_include_directories(BitMotorTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(BitMotorTests PRIVATE BITMOTOR_EXPECTED_VERSION="${PROJECT_VERSION}")
_BitMotor_Warnings(BitMotorTests)
add_test(NAME BitMotorTests COMMAND BitMotorTests)
