cmake_minimum_required(VERSION 3.20)
project(CFS_ARANYA_EP_APP C)

include(../../../ComponentSettings.cmake)
include(FetchContent)

# Trying to use corrosion to build rust aranya client capi here and avoid linking issues
FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.5 # Optionally specify a commit hash, version tag or branch here
  )
    
# Local/public includes
include_directories(
    mission_inc
    platform_inc
    src
    include         # aranya-client.h placed here
    ${hwlib_MISSION_DIR}/fsw/public_inc
)

aux_source_directory(src APP_SRC_FILES)

# Build the cFS app
add_cfe_app(aranya_ep ${APP_SRC_FILES})

# Ensuring rust pipeline matches with the one installed in the build container and with the aranya one
set(Rust_TOOLCHAIN "1.85-aarch64-unknown-linux-gnu")  # rustup toolchain name inside container
set(Rust_CARGO_TARGET "aarch64-unknown-linux-gnu") # corrosion asked to specify

# Set any global configuration variables such as `Rust_TOOLCHAIN` before this line!
FetchContent_MakeAvailable(Corrosion)

# Import targets defined in a package or workspace manifest `Cargo.toml` file
message(STATUS "Importing aranya-client-capi crate via corrosion...")
corrosion_import_crate(
  MANIFEST_PATH 
  aranya-client-capi/Cargo.toml
  PROFILE release   # linking will fail with debug executable for some reason
  FEATURES preview afc aqc experimental
)

# resolves to fsw/build/exe/cpu1/cf
set(_CFE_STAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/../../../../../../exe/cpu1/cf")

# Ensure the runtime linker looks in the module directory and (as a fallback) the app build dir.
# This keeps aranya_ep.so able to find libaranya_client_capi.so in both places.
# CMAKE_CURRENT_BINARY_DIR is build/fsw/apps/aranya_ep
# from readelf -d readelf -d ./build/exe/cpu1/cf/aranya_ep.so :
#   Library runpath: [/media/sf_cfs/build/native/default_cpu1/apps/aranya_ep]
message(STATUS "Setting BPATH and RPATH of aranya_ep.so to ${CMAKE_CURRENT_BINARY_DIR} and ${_CFE_STAGE_DIR}")
set_target_properties(aranya_ep PROPERTIES
  BUILD_RPATH "${CMAKE_CURRENT_BINARY_DIR}"
  INSTALL_RPATH "${_CFE_STAGE_DIR}" # changed this to set runtime dir to exe/cpu1/cf
)

# After building, also stage the Rust lib .so next to aranya_ep.so in build/exe/cpu1/cf.
add_custom_command(TARGET aranya_ep POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_CFE_STAGE_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:aranya_client_capi-shared>  # -shared required for some reason, without it it wont link
          "${_CFE_STAGE_DIR}"
  COMMENT "Staging libaranya_client_capi.so to ${_CFE_STAGE_DIR}"
)

message(STATUS "Linking aranya_ep against aranya-client-capi crate...")
# Link explicitly against the shared target to ensure a runtime dependency on the .so
target_link_libraries(aranya_ep aranya_client_capi)
