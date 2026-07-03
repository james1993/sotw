cmake_minimum_required(VERSION 3.15)
project(sotw_demake C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

add_executable(sotw_demake
    src/main.c
    src/entity.c
    src/attributes.c
    src/skill.c
    src/effect.c
    src/combat.c
    src/ai_hero.c
    src/input.c
    src/ui_skillbar.c
    src/ui_target.c
    src/render.c
)

target_link_libraries(sotw_demake PRIVATE raylib)
target_include_directories(sotw_demake PRIVATE src)

if(APPLE)
    target_link_libraries(sotw_demake PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
