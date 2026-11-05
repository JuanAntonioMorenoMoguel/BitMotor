# CMake helpers for BitMotor and for the games that use it.

# Warnings for BitMotor's own targets (library, tests and examples). PRIVATE: never reach a game.
function(_BitMotor_Warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:preprocessor
            $<$<BOOL:${BITMOTOR_WARNINGS_AS_ERRORS}>:/WX>)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic
            $<$<BOOL:${BITMOTOR_WARNINGS_AS_ERRORS}>:-Werror>)
    endif()
endfunction()

# Leaves a game executable ready to run with no manual steps.
#     BitMotor_Prepare(MyGame [COPY Data Assets ...])
# - Links BitMotor if the game did not.
# - Output in <build>/bin unless the game chose another folder.
# - Windows: no console in Release (keeping a normal main()) and dependency DLLs next to the exe.
# - COPY: copies each folder next to the exe, and defines BITMOTOR_SOURCE_DIR (the game's source folder)
#   so that during development resources can be read from the source tree without copying.
function(BitMotor_Prepare target)
    cmake_parse_arguments(P "" "" "COPY" ${ARGN})
    get_target_property(type ${target} TYPE)
    if(NOT type STREQUAL "EXECUTABLE")
        message(FATAL_ERROR "BitMotor_Prepare: ${target} is not an executable")
    endif()

    get_target_property(libs ${target} LINK_LIBRARIES)
    if(NOT "BitMotor::BitMotor" IN_LIST libs AND NOT "BitMotor" IN_LIST libs)
        target_link_libraries(${target} PRIVATE BitMotor::BitMotor)
    endif()

    get_target_property(out ${target} RUNTIME_OUTPUT_DIRECTORY)
    if(NOT out AND NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    endif()

    if(WIN32)
        set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
        if(MSVC)
            # A Windows (no console) executable still starts at main().
            target_link_options(${target} PRIVATE $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "$<$<BOOL:$<TARGET_RUNTIME_DLLS:${target}>>:${CMAKE_COMMAND};-E;copy_if_different;$<TARGET_RUNTIME_DLLS:${target}>;$<TARGET_FILE_DIR:${target}>>"
            COMMAND_EXPAND_LISTS)
    endif()

    target_compile_definitions(${target} PRIVATE BITMOTOR_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    foreach(folder ${P_COPY})
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/${folder} $<TARGET_FILE_DIR:${target}>/${folder})
    endforeach()
endfunction()

# Internal: each BitMotor example is a folder with a Main.cpp.
#     BitMotor_Example(00_Window)
function(BitMotor_Example name)
    set(target BitMotor_${name})
    add_executable(${target} ${name}/Main.cpp)
    target_link_libraries(${target} PRIVATE BitMotor::BitMotor)
    _BitMotor_Warnings(${target})
    BitMotor_Prepare(${target})
endfunction()
