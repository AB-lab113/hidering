FROM ubuntu:24.04

RUN set -ex && \
    apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends --yes \
        ca-certificates \
        libboost-chrono1.83.0 \
        libboost-filesystem1.83.0 \
        libboost-program-options1.83.0 \
        libboost-serialization1.83.0 \
        libboost-thread1.83.0 \
        libhidapi-libusb0 \
        libreadline8 \
        libsodium23 \
        libssl3 \
        libunbound8 \
        libzmq5 && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY build/bin/hideringd /usr/local/bin/hideringd

RUN useradd --system --create-home --shell /usr/sbin/nologin hidering
USER hidering
WORKDIR /home/hidering

VOLUME ["/home/hidering/.hidering"]

EXPOSE 19740 19741

ENTRYPOINT ["hideringd"]
CMD ["--non-interactive", \
     "--confirm-external-bind", \
     "--no-igd", \
     "--public-node", \
     "--p2p-bind-ip=0.0.0.0", \
     "--p2p-bind-port=19740", \
     "--rpc-bind-ip=0.0.0.0", \
     "--rpc-bind-port=19741", \
     "--log-level=0"]
