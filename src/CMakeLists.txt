# Component CMakeLists.txt for Pool Controller project

# Get compile definitions to check which environment we're building for
get_directory_property(COMPILE_DEFS COMPILE_DEFINITIONS)

# Check for HMI_CONTROLLER first (more specific check)
if("HMI_CONTROLLER" IN_LIST COMPILE_DEFS)
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
                      REQUIRES json esp_http_client esp_http_server nvs_flash esp_wifi esp_event esp_netif driver esp_timer)
