FROM ghcr.io/ebell495/docker-c-cpp:main as build

COPY . /libsais
WORKDIR /libsais

RUN CC="clang" CXX="clang++" CFLAGS="-fsanitize=fuzzer-no-link,undefined" cmake . && make && cd fuzz && make

FROM debian:bookworm
COPY --from=build /libsais/fuzz .
