# Dockerfile
FROM ubuntu:22.04

# Environment
ENV DEBIAN_FRONTEND=noninteractive 

# Install dependencies
RUN apt-get update && apt-get install -y \
	build-essential \
	bison \
	flex \
	libgmp3-dev \
	libmpc-dev \
	libmpfr-dev \
	texinfo \
	wget \
	git \
	gawk \
	libisl-dev \
	curl \
	ca-certificates \
	xz-utils \
	&& rm -rf /var/lib/apt/lists/*

# Preparation
ENV TARGET=x86_64-elf
ENV PREFIX=/opt/cross
ENV PATH=$PREFIX/bin:$PATH

WORKDIR /src

# Download
RUN curl -LO https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.xz && \
	curl -LO https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz && \
	curl -LO https://ftp.gnu.org/gnu/gdb/gdb-13.2.tar.xz

# Extract
RUN	tar -xf binutils-2.41.tar.xz && \
	tar -xf gcc-13.2.0.tar.xz && \
	tar -xf gdb-13.2.tar.xz

# Binutils
RUN mkdir build-binutils && \
	cd build-binutils && \
	../binutils-2.41/configure --target=$TARGET --prefix=$PREFIX --with-sysroot --disable-nls --disable-werror && \
	make -j$(nproc) && make install

# GCC
RUN mkdir build-gcc && \
	cd build-gcc && \
	../gcc-13.2.0/configure --target=$TARGET --prefix=$PREFIX --disable-nls --enable-languages=c,c++ --without-headers --disable-hosted-libstdcxx && \
	make all-gcc -j$(nproc) && \
	make all-target-libgcc -j$(nproc) && \
	make all-target-libstdc++-v3 -j$(nproc) && \
	make install-gcc && \
	make install-target-libgcc && \
	make install-target-libstdc++-v3

# GDB
RUN cd /src && \
	mkdir build-gdb && \
	cd build-gdb && \
	../gdb-13.2/configure --target=$TARGET --prefix=$PREFIX --disable-werror && \
	make all-gdb -j$(nproc) && \
	make install-gdb
