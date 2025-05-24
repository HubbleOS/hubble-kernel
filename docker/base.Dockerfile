# Root Dockerfile
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
	mtools \
	&& rm -rf /var/lib/apt/lists/*
