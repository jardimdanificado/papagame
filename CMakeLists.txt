cmake_minimum_required(VERSION 3.10)
project(Wagnostic C)

set(CMAKE_C_STANDARD 11)

# Find SDL2
find_package(SDL2 REQUIRED)

# Sources
file(GLOB WASM3_SRC "lib/wasm3/*.c")
set(SOURCES 
    runners/native/host.c
    ${WASM3_SRC}
)

add_executable(wagnostic ${SOURCES})

target_include_directories(wagnostic PRIVATE 
    lib/wasm3
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(wagnostic PRIVATE 
    ${SDL2_LIBRARIES}
)

if(WIN32)
    target_link_libraries(wagnostic PRIVATE opengl32)
else()
    target_link_libraries(wagnostic PRIVATE GL m)
endif()
