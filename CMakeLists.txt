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

# raylib vendors third-party code (miniaudio, jar_mod, ...) that emits
# warnings we can't fix; silence the dependency builds so the only
# warnings in our output come from our own sources - which are compiled
# with warnings turned up, below.
foreach(dep raylib glfw)
    if(TARGET ${dep})
        target_compile_options(${dep} PRIVATE
            $<$<C_COMPILER_ID:MSVC>:/w>
            $<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>)
    endif()
endforeach()

add_executable(sotw_demake
    src/main.c
    src/character.c
    src/gwmath.c
    src/ui_icons.c
    src/assets.c
    src/ground.c
    src/ui_focus.c
    src/ui_nav.c
    src/audio.c
    src/ui_create.c
    src/ui_select.c
    src/ui_searing.c
    src/entity.c
    src/attributes.c
    src/skill.c
    src/effect.c
    src/combat.c
    src/ai_hero.c
    src/input.c
    src/items.c
    src/progression.c
    src/projectile.c
    src/world.c
    src/quests.c
    src/save.c
    src/ui_menu.c
    src/ui_cursor.c
    src/ui_map.c
    src/ui_skillbar.c
    src/ui_target.c
    src/ui_party.c
    src/ui_compass.c
    src/ui_hit.c
    src/ui_panels.c
    src/ui_font.c
    src/ui_theme.c
    src/ui_tooltip.c
    src/titles.c
    src/builds.c
    src/ui_world.c
    src/ui_hints.c
    src/skillbook.c
    src/sprite.c
    src/fx.c
    src/render.c
)

target_link_libraries(sotw_demake PRIVATE raylib)
target_include_directories(sotw_demake PRIVATE src)
target_compile_options(sotw_demake PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra>)
# strncpy & co. trip MSVC's C4996 "unsafe CRT" warnings under /W4; they
# are used with explicit bounds everywhere, so silence just that class.
target_compile_definitions(sotw_demake PRIVATE
    $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)

if(APPLE)
    target_link_libraries(sotw_demake PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

# Ship the bundled font (and any future assets) next to the executable,
# so the game finds them via GetApplicationDirectory() regardless of the
# working directory it's launched from.
add_custom_command(TARGET sotw_demake POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:sotw_demake>/assets)

# --- Tests -----------------------------------------------------------
# Pure-logic tests only: things that can be checked without a window, an
# audio device or a controller. ui_focus's index arithmetic qualifies and
# is worth pinning, because a wrong index silently highlights the wrong
# reply rather than crashing.
#
#   cmake --build build --target run_tests
add_executable(test_ui_focus EXCLUDE_FROM_ALL
    tests/test_ui_focus.c
    src/ui_focus.c)
target_include_directories(test_ui_focus PRIVATE src)
target_link_libraries(test_ui_focus PRIVATE m)
# raylib.h only - the test stubs every function it uses, so nothing links
# against the library itself.
target_include_directories(test_ui_focus PRIVATE
    ${raylib_SOURCE_DIR}/src)

add_custom_target(run_tests
    COMMAND test_ui_focus
    DEPENDS test_ui_focus
    COMMENT "Running unit tests")
