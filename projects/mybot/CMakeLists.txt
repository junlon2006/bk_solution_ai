cmake_minimum_required(VERSION 3.5)

include($ENV{ARMINO_TOOLS_PATH}/build_tools/cmake/project.cmake)

# components/mybot is a container whose sub-components (mybot_sdk, mybot_aosl)
# are discovered automatically.
set(EXTRA_COMPONENTS_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/mybot
)

project(app)
