set(incs include)
set(srcs
    src/app_camera.c
)

# MyBot uses only the camera/codec helpers.  Keep display, GPU and LVGL blend
# helpers conditional so a headless video build does not acquire the LVGL
# component just to satisfy this shared solution component.
if(CONFIG_BK_DISPLAY)
    list(APPEND srcs src/app_display.c)
endif()
if(CONFIG_VG_LITE_GPU)
    list(APPEND srcs src/app_gpu.c)
endif()
if(CONFIG_LVGL)
    list(APPEND srcs src/bk_camera_lvgl_blend.c)
endif()

if(CONFIG_MULTIMEDIA_H264_ENCODE)
    list(APPEND srcs
        src/app_codec.c
        src/multimedia_img_manager.c
    )
endif()

set(priv_req bk_common bk_pm media_service bk_peripheral avdk_driver)
if(CONFIG_LVGL)
    list(APPEND priv_req lvgl)
endif()

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES "${priv_req}")

add_definitions(-Wno-unused-variable
                -Wno-unused-but-set-variable
                -Wno-int-to-pointer-cast
                -Wno-pointer-to-int-cast)
