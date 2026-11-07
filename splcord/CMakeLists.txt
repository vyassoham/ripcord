cmake_minimum_required(VERSION 3.20)
project(splcord VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags
if(MSVC)
    add_compile_options(/O2 /W4 /utf-8)
else()
    add_compile_options(-O3 -Wall -Wextra -Wno-unused-parameter)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/opus
)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

# Main Executable: splcord
add_executable(splcord
    src/main.cpp
)

target_link_libraries(splcord PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/libopus.a
    ws2_32
    winmm
    ole32
    shell32
    gdiplus
    gdi32
    user32
    comctl32
)

# Test Executable: splcord_tests
add_executable(splcord_tests
    tests/test_dsp.cpp
)

target_link_libraries(splcord_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/libopus.a
    ws2_32
    winmm
    ole32
    shell32
)

# Copy libopus-0.dll next to built executables
add_custom_command(TARGET splcord POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/libopus-0.dll"
    "$<TARGET_FILE_DIR:splcord>/libopus-0.dll"
)

add_custom_command(TARGET splcord_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/libopus-0.dll"
    "$<TARGET_FILE_DIR:splcord_tests>/libopus-0.dll"
)
