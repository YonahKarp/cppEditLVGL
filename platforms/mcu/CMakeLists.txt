add_library(platform_backend STATIC platform_mcu.cpp)

target_include_directories(platform_backend PUBLIC
    ${CMAKE_SOURCE_DIR}/platforms
)
target_link_libraries(platform_backend PUBLIC lvgl)

target_compile_definitions(lvgl PUBLIC
    LV_USE_SDL=0
    LV_USE_LINUX_FBDEV=0
    LV_USE_EVDEV=0
)
