cmake_minimum_required(VERSION 3.10)

project(SmartAccessTerminal C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
)

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
)

add_executable(smart_access_terminal
    src/main.c
    src/app/app_main.c

    ${COMMON_SOURCES}
)

add_executable(test_tcp_send_image
    tests/test_tcp_send_image.c

    ${COMMON_SOURCES}
)



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
        m
        pthread
    )


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
        m
        pthread
    )
endif()