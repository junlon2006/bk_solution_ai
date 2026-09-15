set(incs include)
if(CONFIG_MYBOT_VIDEO)
    # MyBot video only needs the camera and codec helpers.  Avoid pulling the
    # display/GPU/LVGL demo sources into this headless video path.
    set(srcs
        src/app_camera.c
    )
    set(priv_req bk_common bk_pm media_service bk_peripheral avdk_driver)
else()
    # Preserve the original multimedia component for every non-MyBot-video
    # product, including beken_robot and its display demos.
    set(srcs
        src/app_camera.c
        src/app_display.c
        src/app_gpu.c
        src/bk_camera_lvgl_blend.c
    )
    set(priv_req bk_common bk_pm media_service bk_peripheral avdk_driver lvgl)
endif()

if(CONFIG_MULTIMEDIA_H264_ENCODE)
    list(APPEND srcs
        src/app_codec.c
        src/multimedia_img_manager.c
    )
endif()

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES "${priv_req}")

add_definitions(-Wno-unused-variable
                -Wno-unused-but-set-variable
                -Wno-int-to-pointer-cast
                -Wno-pointer-to-int-cast)
