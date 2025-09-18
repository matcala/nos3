cmake_minimum_required(VERSION 3.13)
project(aranya_ep C)

# cFS macro will create a target named aranya_ep
add_cfe_app(aranya_ep
  fsw/src/aranya_ep_app.c
)

target_include_directories(aranya_ep PRIVATE
  fsw/inc
)

# ---- Aranya C API discovery ----
# Adjust default search paths to where you place the Aranya bits
# Prefer the directory containing this CMakeLists.txt first
set(_ARANYA_LOCAL_DIR "${CMAKE_CURRENT_LIST_DIR}")

find_path(ARANYA_INCLUDE_DIR
   NAMES aranya-client.h
   HINTS "${_ARANYA_LOCAL_DIR}"            # same dir as this CMakeLists.txt
            "${_ARANYA_LOCAL_DIR}/include"    # optional common subdir
   PATHS /usr/local/include /opt/aranya/include
)

find_library(ARANYA_CLIENT_LIB
   NAMES aranya_client_capi aranya_client
   HINTS "${_ARANYA_LOCAL_DIR}"            # same dir as this CMakeLists.txt
            "${_ARANYA_LOCAL_DIR}/lib"        # optional common subdir
   PATHS /usr/local/lib /opt/aranya/lib
)

if (ARANYA_INCLUDE_DIR AND ARANYA_CLIENT_LIB)
  message(STATUS "Found Aranya C API: ${ARANYA_INCLUDE_DIR}, ${ARANYA_CLIENT_LIB}")
  target_include_directories(aranya_ep PRIVATE ${ARANYA_INCLUDE_DIR})
  target_link_libraries(aranya_ep PRIVATE ${ARANYA_CLIENT_LIB})
else()
  message(FATAL_ERROR "Aranya C API not found. Set ARANYA_INCLUDE_DIR/ARANYA_CLIENT_LIB or install to /usr/local")
endif()
