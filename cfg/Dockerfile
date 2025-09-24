# Extra docker image to include rust toolchain to build Aranya C API.
# NOS3 build is isolated in a NASA docker box, which does not allow rust-related cmake steps required for Aranya

# Dockerfile.rust
FROM ivvitc/nos3-64:20250514

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive
# Rust locations
ENV RUSTUP_HOME=/opt/rustup
ENV CARGO_HOME=/opt/cargo
ENV PATH=${CARGO_HOME}/bin:${PATH}
ENV RUSTUP_ARANYA_CAPI_TOOLCHAIN=1.85-aarch64-unknown-linux-gnu

RUN apt-get update -y && apt-get install -y curl ca-certificates build-essential pkg-config \
 && curl https://sh.rustup.rs -sSf | sh -s -- -y --default-toolchain ${RUSTUP_ARANYA_CAPI_TOOLCHAIN} \
 && rustup component add rustfmt clippy \
 && rustup target add aarch64-unknown-linux-gnu \
 && rm -rf /var/lib/apt/lists/*

# Consider adding
# git config --global --add safe.directory /media/sf_cfs/build/native/default_cpu1/_deps/corrosion-src
# to solve possible cmake corrosion import issues
# Don't know if container has git installed though