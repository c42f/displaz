# Minimal displaz Qt5 build, test and install
# using Docker Debian 10 (buster)

# Status: Confirmed working April 20th 2025

FROM debian:buster
MAINTAINER Nigel Stewart (nigels@nigels.com)

#
# Update and install build tools, dependencies
#

ENV DEBIAN_FRONTEND noninteractive

RUN apt update
RUN apt dist-upgrade -y

RUN apt install -y build-essential git cmake doxygen graphviz cppcheck
RUN apt install -y g++ cmake qt5-default python-docutils wget

# CMake 3.12 or higher is required

RUN wget -nv https://github.com/Kitware/CMake/releases/download/v3.25.2/cmake-3.25.2-linux-x86_64.sh
RUN chmod a+x ./cmake-3.25.2-linux-x86_64.sh

ENV CMAKE_PREFIX /opt/cmake-3.25.2
RUN mkdir -p ${CMAKE_PREFIX}
RUN ./cmake-3.25.2-linux-x86_64.sh --skip-license --prefix=${CMAKE_PREFIX}

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
RUN ${CMAKE_PREFIX}/bin/cmake ../thirdparty/external
RUN make -j

RUN mkdir -p ${SCRATCH}/repo/build
WORKDIR ${SCRATCH}/repo/build
RUN ${CMAKE_PREFIX}/bin/cmake ..
RUN make -j

#
# Test
#

RUN ${CMAKE_PREFIX}/bin/ctest

#
# Install
#

RUN make install
RUN find /usr/local
