cmake_minimum_required(VERSION 3.14)
project(justtype_lvgl C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build tests" ON)
set(PLATFORM "sdl" CACHE STRING "Target platform: sdl, linux_fb, mcu")
set_property(CACHE PLATFORM PROPERTY STRINGS sdl linux_fb mcu)

# Use local LVGL from libs folder
set(LV_CONF_PATH ${CMAKE_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
add_subdirectory(libs/lvgl)

# Configure LVGL with our lv_conf.h
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
target_include_directories(lvgl PUBLIC ${CMAKE_SOURCE_DIR})

# Exclude ARM assembly files from LVGL build
get_target_property(LVGL_SOURCES lvgl SOURCES)
set(FILTERED_SOURCES "")
foreach(SRC ${LVGL_SOURCES})
    if(NOT SRC MATCHES ".*helium.*\\.S$" AND NOT SRC MATCHES ".*neon.*\\.S$" AND NOT SRC MATCHES ".*arm2d.*")
        list(APPEND FILTERED_SOURCES ${SRC})
    endif()
endforeach()
set_target_properties(lvgl PROPERTIES SOURCES "${FILTERED_SOURCES}")

if(PLATFORM STREQUAL "sdl")
    add_subdirectory(platforms/sdl)
elseif(PLATFORM STREQUAL "linux_fb")
    add_subdirectory(platforms/linux_fb)
elseif(PLATFORM STREQUAL "mcu")
    add_subdirectory(platforms/mcu)
else()
    message(FATAL_ERROR "Unsupported PLATFORM='${PLATFORM}'. Valid values: sdl, linux_fb, mcu")
endif()

add_executable(justtype_lvgl
    src/main.cpp
    src/app.cpp
    src/editor.cpp
    src/editor_word_count.cpp
    src/sidebar.cpp
    src/sidebar_dialog.cpp
    src/sidebar_folder.cpp
    src/sidebar_file.cpp
    src/sidebar_file_utils.cpp
    src/sidebar_file_dialog.cpp
    src/search.cpp
    src/file_manager.cpp
    src/text_navigation.cpp
    src/icons/battery_icon.cpp
    src/icons/magnifying_glass_icon.cpp
    src/icons/add_document_icon.cpp
    src/icons/add_folder_icon.cpp
)

target_include_directories(justtype_lvgl PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/platforms
)

target_link_libraries(justtype_lvgl PRIVATE
    lvgl
    platform_backend
    pthread
)

# Testing
if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(test_text_navigation
        tests/test_text_navigation.cpp
        src/text_navigation.cpp
    )
    target_include_directories(test_text_navigation PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_text_navigation GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(test_text_navigation)
endif()
