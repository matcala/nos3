cmake_minimum_required(VERSION 2.6.4)
project(CFS_ARANYA_EP_APP C)

include(../../../ComponentSettings.cmake)

include_directories(mission_inc)
include_directories(platform_inc)
include_directories(src)
include_directories(include)    # for aranya-client.h
include_directories(lib)        # for aranya client shared lib

include_directories(${hwlib_MISSION_DIR}/fsw/public_inc)
aux_source_directory(src APP_SRC_FILES)

# Import external Aranya client shared library and link explicitly
add_library(aranya_client SHARED IMPORTED)
set_target_properties(aranya_client PROPERTIES
	IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/libaranya_client.so"
)

# Create the app module
add_cfe_app(aranya_ep ${APP_SRC_FILES})

# Might have to review and fix
target_link_libraries(aranya_ep aranya_client)
