# Component CMakeLists.txt for Pool Controller project

# HMI_CONTROLLER is passed as a CMake variable via board_build.cmake_extra_args
if(HMI_CONTROLLER)
    # HMI Controller build - use HMI files
    FILE(GLOB_RECURSE COMPONENT_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/hmi/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/hmi/*.c
    )
    message(STATUS "Building for HMI Controller")

    set(COMPONENT_ADD_INCLUDEDIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/hmi
    )

else()
    # Main Controller build (default)
    FILE(GLOB_RECURSE COMPONENT_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/main_controller/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/main_controller/*.c
    )
    message(STATUS "Building for Main Controller")

    set(COMPONENT_ADD_INCLUDEDIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/main_controller
    )
endif()

# Register component with required ESP-IDF dependencies
idf_component_register(SRCS ${COMPONENT_SRCS}
                      INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
                      REQUIRES json esp_http_client esp_http_server nvs_flash esp_wifi esp_event esp_netif driver esp_timer esp_lcd)

