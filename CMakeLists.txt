cmake_minimum_required(VERSION 3.10)

project(SmartAccessTerminal C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================
# Global Include Paths
# ============================================================
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/third_party
)

# ============================================================
# Common Sources
# 这些是正式业务程序和部分测试程序共用的模块
# ============================================================
set(COMMON_SOURCES
    src/common/log.c

    src/config/device_config.c

    src/storage/snapshot_storage.c

    src/camera/camera_v4l2.c

    src/network/tcp_client.c
    src/network/packet.c

    src/protocol/access_protocol.c

    src/bsp/door.c
    src/bsp/alarm.c

    src/app/app_state.c
    src/app/app_worker.c

    src/third_party/cJSON.c
)

# ============================================================
# Main Program: command-line worker version
# ============================================================
add_executable(smart_access_terminal
    src/main.c
    src/app/app_main.c

    ${COMMON_SOURCES}
)

target_link_libraries(smart_access_terminal
    pthread
    m
)

# ============================================================
# Test: TCP send image
# ============================================================
add_executable(test_tcp_send_image
    tests/test_tcp_send_image.c

    ${COMMON_SOURCES}
)

target_link_libraries(test_tcp_send_image
    pthread
    m
)

# ============================================================
# Test: V4L2 single RGB565 capture
# ============================================================
add_executable(test_v4l2_rgb565_capture
    tests/test_v4l2_rgb565_capture.c
)

target_include_directories(test_v4l2_rgb565_capture PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(test_v4l2_rgb565_capture
    m
)

# ============================================================
# Test: V4L2 camera preview thread
# 第20关：实时预览采集模块测试
# ============================================================
add_executable(test_camera_preview
    tests/test_camera_preview.c
    src/camera/camera_preview.c
    src/common/log.c
)

target_include_directories(test_camera_preview PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(test_camera_preview
    pthread
    m
)

# ============================================================
# LVGL Framebuffer Build
# 用于 IMX6ULL /dev/fb0 显示
# ============================================================
option(BUILD_LVGL_FB_TEST "Build LVGL framebuffer test" OFF)

if(BUILD_LVGL_FB_TEST)

    file(GLOB_RECURSE LVGL_SOURCES_FB
        ${PROJECT_SOURCE_DIR}/src/third_party/lvgl/src/*.c
    )

    add_executable(test_lvgl_fb
        tests/test_lvgl_fb.c

        src/third_party/lv_drivers/display/fbdev.c

        ${LVGL_SOURCES_FB}
    )

    add_executable(test_lvgl_camera_preview_fb
        src/main.c
        src/app/app_main_preview_fb.c
        src/ui/ui_main.c

        src/app/app_state.c

        src/camera/camera_preview.c
        src/common/log.c
        src/config/device_config.c
        src/third_party/cJSON.c

        src/third_party/lv_drivers/display/fbdev.c

        ${LVGL_SOURCES_FB}
    )

    target_include_directories(test_lvgl_camera_preview_fb PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/third_party
        ${PROJECT_SOURCE_DIR}/src/third_party
        ${PROJECT_SOURCE_DIR}/src/third_party/lvgl
        ${PROJECT_SOURCE_DIR}/src/third_party/lv_drivers
    )

    target_compile_definitions(test_lvgl_camera_preview_fb PRIVATE
        LV_CONF_INCLUDE_SIMPLE
        LV_DRV_CONF_INCLUDE_SIMPLE
        USE_FBDEV=1
    )

    target_link_libraries(test_lvgl_camera_preview_fb
        pthread
        m
    )


    target_include_directories(test_lvgl_fb PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/third_party
        ${PROJECT_SOURCE_DIR}/src/third_party/lvgl
        ${PROJECT_SOURCE_DIR}/src/third_party/lv_drivers
    )

    target_compile_definitions(test_lvgl_fb PRIVATE
        LV_CONF_INCLUDE_SIMPLE
        LV_DRV_CONF_INCLUDE_SIMPLE
        USE_FBDEV=1
    )

    target_link_libraries(test_lvgl_fb
        pthread
        m
    )

    # --------------------------------------------------------
    # Main Program: framebuffer UI version
    # 第19关：正式门禁 UI + worker + framebuffer
    # --------------------------------------------------------
    add_executable(smart_access_terminal_fb
        src/main.c
        src/app/app_main_fb.c
        src/ui/ui_main.c

        src/third_party/lv_drivers/display/fbdev.c

        ${COMMON_SOURCES}
        ${LVGL_SOURCES_FB}
    )

    target_include_directories(smart_access_terminal_fb PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/third_party
        ${PROJECT_SOURCE_DIR}/src/third_party
        ${PROJECT_SOURCE_DIR}/src/third_party/lvgl
        ${PROJECT_SOURCE_DIR}/src/third_party/lv_drivers
    )

    target_compile_definitions(smart_access_terminal_fb PRIVATE
        LV_CONF_INCLUDE_SIMPLE
        LV_DRV_CONF_INCLUDE_SIMPLE
        USE_FBDEV=1
    )

    target_link_libraries(smart_access_terminal_fb
        pthread
        m
    )

endif()

# ============================================================
# LVGL SDL Build
# 用于 PC / Ubuntu 上测试 LVGL UI
# ============================================================
option(BUILD_LVGL_UI_TEST "Build LVGL SDL UI test" ON)

if(BUILD_LVGL_UI_TEST)

    find_package(SDL2 REQUIRED)

    file(GLOB_RECURSE LVGL_SOURCES
        ${PROJECT_SOURCE_DIR}/src/third_party/lvgl/src/*.c
    )

    file(GLOB_RECURSE LV_DRIVERS_SOURCES
        ${PROJECT_SOURCE_DIR}/src/third_party/lv_drivers/sdl/*.c
        ${PROJECT_SOURCE_DIR}/src/third_party/lv_drivers/indev/*.c
    )

    set(LVGL_COMMON_INCLUDES
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/third_party
        ${PROJECT_SOURCE_DIR}/src/third_party/lvgl
        ${PROJECT_SOURCE_DIR}/src/third_party/lv_drivers
        ${SDL2_INCLUDE_DIRS}
    )

    set(LVGL_COMMON_DEFINES
        USE_SDL=1
        LV_CONF_INCLUDE_SIMPLE
        LV_DRV_CONF_INCLUDE_SIMPLE
    )

    # --------------------------------------------------------
    # Test: basic LVGL UI
    # --------------------------------------------------------
    add_executable(test_lvgl_ui
        tests/test_lvgl_ui.c
        src/ui/ui_main.c

        ${LVGL_SOURCES}
        ${LV_DRIVERS_SOURCES}
    )

    target_include_directories(test_lvgl_ui PRIVATE
        ${LVGL_COMMON_INCLUDES}
    )

    target_compile_definitions(test_lvgl_ui PRIVATE
        ${LVGL_COMMON_DEFINES}
    )

    target_link_libraries(test_lvgl_ui
        ${SDL2_LIBRARIES}
        pthread
        m
    )

    # --------------------------------------------------------
    # Test: app_state + LVGL UI
    # --------------------------------------------------------
    add_executable(test_app_state_ui
        tests/test_app_state_ui.c
        src/ui/ui_main.c
        src/app/app_state.c

        ${LVGL_SOURCES}
        ${LV_DRIVERS_SOURCES}
    )

    target_include_directories(test_app_state_ui PRIVATE
        ${LVGL_COMMON_INCLUDES}
    )

    target_compile_definitions(test_app_state_ui PRIVATE
        ${LVGL_COMMON_DEFINES}
    )

    target_link_libraries(test_app_state_ui
        ${SDL2_LIBRARIES}
        pthread
        m
    )

    # --------------------------------------------------------
    # Test: integrated UI mock worker
    # 第17关：LVGL 主线程 + mock worker 后台线程
    # --------------------------------------------------------
    add_executable(test_app_integrated_ui
        tests/test_app_integrated_ui.c
        src/ui/ui_main.c
        src/app/app_state.c

        ${LVGL_SOURCES}
        ${LV_DRIVERS_SOURCES}
    )

    target_include_directories(test_app_integrated_ui PRIVATE
        ${LVGL_COMMON_INCLUDES}
    )

    target_compile_definitions(test_app_integrated_ui PRIVATE
        ${LVGL_COMMON_DEFINES}
    )

    target_link_libraries(test_app_integrated_ui
        ${SDL2_LIBRARIES}
        pthread
        m
    )

endif()