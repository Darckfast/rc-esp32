# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)
FILE(GLOB_RECURSE app_libs ${CMAKE_SOURCE_DIR}/lib/*.*)

set(requires "bluepad32" "btstack" "esp_wifi")

idf_component_register(SRCS ${app_sources} ${app_libs} INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include REQUIRES "${requires}")
