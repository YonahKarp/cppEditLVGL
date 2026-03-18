find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_library(platform_backend STATIC platform_sdl.cpp)

target_include_directories(platform_backend PUBLIC
    ${CMAKE_SOURCE_DIR}/platforms
    ${SDL2_INCLUDE_DIRS}
)
target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})

target_link_directories(platform_backend PUBLIC ${SDL2_LIBRARY_DIRS})
target_link_libraries(platform_backend PUBLIC lvgl ${SDL2_LIBRARIES})
target_compile_options(platform_backend PRIVATE ${SDL2_CFLAGS_OTHER})
target_compile_options(lvgl PRIVATE ${SDL2_CFLAGS_OTHER})

target_compile_definitions(lvgl PUBLIC
    LV_USE_SDL=1
    LV_USE_LINUX_FBDEV=0
    LV_USE_EVDEV=0
)
