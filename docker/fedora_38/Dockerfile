# Minimal displaz Qt5 build, test and install
# using Fedora 38

# Fedora 38 EOL: 2024-05-21

FROM fedora:38
MAINTAINER Nigel Stewart (nigels@nigels.com)

#
# Update and install build tools, dependencies
#

ENV DEBIAN_FRONTEND noninteractive

RUN dnf -y upgrade

RUN dnf -y install git gcc-c++ make patch cmake qt5-qtbase-devel mesa-libGLU-devel python-docutils

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
