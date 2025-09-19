cmake_minimum_required(VERSION 2.6.4)
project(CFS_ARANYA_EP_APP C)

include(../../../ComponentSettings.cmake)

# Ensure RPATH is defined before target creation so INSTALL_RPATH is inherited
set(_CF_RUNTIME_DIR "${CMAKE_INSTALL_PREFIX}/${TGTSYS_${SYSVAR}}/${INSTALL_SUBDIR}")
set(CMAKE_INSTALL_RPATH "${_CF_RUNTIME_DIR}")

# Local/public includes
include_directories(
  mission_inc
  platform_inc
  fsw/src
  include         # aranya-client.h placed here
  ${hwlib_MISSION_DIR}/fsw/public_inc
)

aux_source_directory(src APP_SRC_FILES)

# Build the cFS app
add_cfe_app(aranya_ep ${APP_SRC_FILES})

# Also force both build-tree and install-tree RPATH explicitly
set_target_properties(aranya_ep PROPERTIES
  BUILD_RPATH "${_CF_RUNTIME_DIR}"
  INSTALL_RPATH "${_CF_RUNTIME_DIR}"
)

# --- Link against the Aranya C API at build time ---
# If you have headers only, this is optional (symbols resolve at runtime).
# But it's fine to link now if the lib is present.
set(ARANYA_SO ${CMAKE_CURRENT_SOURCE_DIR}/lib/libaranya_client.so)

if(EXISTS "${ARANYA_SO}")
	target_link_libraries(aranya_ep ${ARANYA_SO})

	# --- Copy the Aranya .so into the same dir as aranya_ep.so after build ---
	# cFE stages modules into build/exe/<cpu>/cf; $<TARGET_FILE_DIR:aranya_ep> resolves there.
	add_custom_command(TARGET aranya_ep POST_BUILD
	  COMMAND ${CMAKE_COMMAND} -E copy_if_different
	          "${ARANYA_SO}"
	          "$<TARGET_FILE_DIR:aranya_ep>/libaranya_client.so"
	  COMMAND ${CMAKE_COMMAND} -E make_directory "${_CF_RUNTIME_DIR}"
	  COMMAND ${CMAKE_COMMAND} -E copy_if_different
	          "${ARANYA_SO}"
	          "${_CF_RUNTIME_DIR}/libaranya_client.so"
	  COMMENT "Staging libaranya_client.so (build dir and runtime ${_CF_RUNTIME_DIR})"
	)
else()
	message(WARNING "Aranya client library not found at ${ARANYA_SO}; building aranya_ep without direct linkage.")
endif()
