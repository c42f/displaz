# Minimal displaz Qt5 build, test and install
# using Docker Debian 12 (bookworm)

# Status: Confirmed working April 20th 2025

FROM debian:bookworm
MAINTAINER Nigel Stewart (nigels@nigels.com)

#
# Update and install build tools, dependencies
#

ENV DEBIAN_FRONTEND noninteractive

RUN apt update
RUN apt dist-upgrade -y

RUN apt install -y build-essential git cmake doxygen graphviz cppcheck
RUN apt install -y g++ cmake qtbase5-dev python3

#
# Clone repo
#

ENV SCRATCH /scratch
RUN mkdir ${SCRATCH}
WORKDIR ${SCRATCH}

ARG BRANCH
ENV BRANCH ${BRANCH:-master}

ARG REPO
ENV REPO ${REPO:-https://github.com/c42f/displaz.git}

RUN git clone --branch ${BRANCH} ${REPO} repo

#
# Build
#

RUN mkdir -p ${SCRATCH}/repo/build_external
WORKDIR ${SCRATCH}/repo/build_external
RUN cmake ../thirdparty/external
RUN make -j

RUN mkdir -p ${SCRATCH}/repo/build
WORKDIR ${SCRATCH}/repo/build
RUN cmake ..
RUN make -j

#
# Test
#

RUN ctest

#
# Install
#

RUN make install
RUN find /usr/local
