cmake_minimum_required(VERSION 3.16)
project(face_selection_test LANGUAGES C CXX)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
get_filename_component(CODEX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(REPO_ROOT "${CODEX_ROOT}/.." ABSOLUTE)
set(SOURCE "${REPO_ROOT}/src")
set(PINETIME_BUILD_DIR "${REPO_ROOT}/build" CACHE PATH "Firmware build containing generated fonts")
set(FONTS_ROOT "${PINETIME_BUILD_DIR}/src")

# Use the production CMake list and template, including their configured order.
add_subdirectory("${SOURCE}/displayapp/apps" "${CMAKE_CURRENT_BINARY_DIR}/generated/displayapp/apps")
file(GLOB_RECURSE LVGL_SOURCES "${SOURCE}/libs/lvgl/src/*.c")
file(GLOB FONT_SOURCES "${FONTS_ROOT}/displayapp/fonts/*.c")
add_library(ui_lvgl STATIC ${LVGL_SOURCES} ${FONT_SOURCES})
target_include_directories(ui_lvgl PUBLIC "${CODEX_ROOT}/host" "${SOURCE}/libs")
target_compile_definitions(ui_lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
target_compile_options(ui_lvgl PRIVATE -Wno-deprecated-non-prototype -Wno-incompatible-pointer-types-discards-qualifiers)
add_executable(face-selection-test selection_test.cpp
 "${SOURCE}/components/settings/Settings.cpp"
 "${SOURCE}/displayapp/screens/settings/SettingWatchFace.cpp"
 "${SOURCE}/displayapp/widgets/PageIndicator.cpp"
 "${SOURCE}/displayapp/InfiniTimeTheme.cpp")
target_include_directories(face-selection-test BEFORE PRIVATE
 "${CMAKE_CURRENT_SOURCE_DIR}/stubs" "${CMAKE_CURRENT_BINARY_DIR}/generated" "${FONTS_ROOT}" "${SOURCE}")
target_link_libraries(face-selection-test PRIVATE ui_lvgl)
