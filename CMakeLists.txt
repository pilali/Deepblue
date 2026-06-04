cmake_minimum_required(VERSION 3.10)
project(deepblue VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LV2 REQUIRED lv2)

# ── Options ───────────────────────────────────────────────────────────────
# Length of the dispersion allpass chain. 6 fits the MOD Dwarf (Cortex-A35);
# 12 (native) / 16 (Pi 5) give a richer dispersive tail.
set(DEEPBLUE_DISP_STAGES "12" CACHE STRING
    "Dispersion allpass stages (6 for MOD Dwarf, 12 native, 16 for Pi 5)")

# Polyphony of the Minnaert bubble stream. 6 fits the MOD Dwarf; 12/16 elsewhere.
set(DEEPBLUE_BUBBLE_VOICES "12" CACHE STRING
    "Bubble-stream voices (6 for MOD Dwarf, 12 native, 16 for Pi 5)")

set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/deepblue.lv2")

add_library(deepblue MODULE
    src/plugin.cpp
    src/deepblue_dsp.cpp
    src/glibc_compat.cpp)

target_include_directories(deepblue PRIVATE ${LV2_INCLUDE_DIRS} src)

target_compile_definitions(deepblue PRIVATE
    DEEPBLUE_DISP_STAGES=${DEEPBLUE_DISP_STAGES}
    DEEPBLUE_BUBBLE_VOICES=${DEEPBLUE_BUBBLE_VOICES})

target_compile_options(deepblue PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    -ffast-math
    -fvisibility=hidden
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-O0 -g>
)

set_target_properties(deepblue PROPERTIES
    PREFIX ""
    OUTPUT_NAME "deepblue"
    LIBRARY_OUTPUT_DIRECTORY "${BUNDLE_DIR}"
)

message(STATUS "Dispersion stages: ${DEEPBLUE_DISP_STAGES}")
message(STATUS "Bubble voices:     ${DEEPBLUE_BUBBLE_VOICES}")

# Copy every TTL in the source bundle (manifest, plugin descriptor and the
# preset files) next to the freshly built .so.
file(GLOB DEEPBLUE_TTLS "${CMAKE_SOURCE_DIR}/deepblue.lv2/*.ttl")
add_custom_command(TARGET deepblue POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${DEEPBLUE_TTLS} "${BUNDLE_DIR}/"
    COMMENT "Copying TTL files (incl. presets) to bundle"
)

install(DIRECTORY "${BUNDLE_DIR}"
    DESTINATION lib/lv2
)
