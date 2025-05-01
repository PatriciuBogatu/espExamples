# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# if(DEFINED ENV{RMAKER_PATH})
#   set(RMAKER_PATH $ENV{RMAKER_PATH})
# else()
#   set(RMAKER_PATH ${CMAKE_CURRENT_LIST_DIR}/../esp-rainmaker)
# endif(DEFINED ENV{RMAKER_PATH})

set(EXTRA_COMPONENT_DIRS ./components)
#${RMAKER_PATH}/components/esp-insights/components ${RMAKER_PATH}/components ${RMAKER_PATH}/examples/common

set(PROJECT_VER "1.1.2")
#mqtt user and pwd

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(audio)