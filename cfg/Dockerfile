# Upgraded docker image to include rust toolchain to build Aranya C API.
# NOS3 build is isolated in a NASA docker box, which does not allow rust-related cmake steps required for Aranya
# Built over NOS3 config image

#  TODO: check gid and groups of docker
#  since it was built in the vbox share it runs with:
#  uid=0(root) gid=986(vboxsf) groups=986(vboxsf)

# TODO: aranya-daemon listen address based on ENV var


# Dockerfile.rust
FROM ivvitc/nos3-64:20250514

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive
# Rust locations
ENV RUSTUP_HOME=/opt/rustup
ENV CARGO_HOME=/opt/cargo
ENV PATH=${CARGO_HOME}/bin:${PATH}
ENV RUSTUP_ARANYA_CAPI_TOOLCHAIN=1.85-aarch64-unknown-linux-gnu
# Change once we deploy multiple daemons for cFS to avoid conflicts
ENV ARANYA_DAEMON_SYNC_LISTEN_PORT=9999 

# Install rustup toolchain to build aranya C lib
RUN apt-get update -y && apt-get install -y curl ca-certificates build-essential pkg-config \
 && curl https://sh.rustup.rs -sSf | sh -s -- -y --default-toolchain ${RUSTUP_ARANYA_CAPI_TOOLCHAIN} \
 && rustup component add rustfmt clippy \
 && rustup target add aarch64-unknown-linux-gnu \
 && rm -rf /var/lib/apt/lists/*

# TODO: import daemon state into VM and map
# TODO: check if sync works outside of container
# Install aranya-daemon bin
RUN cargo install --git https://github.com/matcala/aranya.git \
                           --branch cosmos-gate aranya-daemon \
                           --version 3.0.0 \
                           --target aarch64-unknown-linux-gnu  \
                           --features aqc,experimental,afc,preview \
   && mkdir -m 0755 -p /run/aranya/run /run/aranya/config /run/aranya/log /run/aranya/cache /run/aranya/state /etc/aranya

   # Copy sample daemon configa 
# WARNING Change once we deploy multiple daemons for cFS to avoid conflicts 
COPY ./../components/aranya_ep/fsw/cfs/cfg/daemon-config.toml /etc/aranya/daemon-config.toml


# Consider adding
# git config --global --add safe.directory /media/sf_cfs/build/native/default_cpu1/_deps/corrosion-src
# to solve possible cmake corrosion import issues
# Don't know if container has git installed though