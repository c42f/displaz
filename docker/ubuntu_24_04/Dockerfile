# Minimal displaz Qt5 build, test and install
# using Docker Ubuntu 24.04 (Noble)

FROM ubuntu:noble
MAINTAINER Nigel Stewart (nigels@nigels.com)

#
# Optional custom package mirror
#
# --build-arg MIRROR=http://au.archive.ubuntu.com/ubuntu/
#

ARG MIRROR
ENV MIRROR ${MIRROR:-http://archive.ubuntu.com/ubuntu}

#RUN sed -i -r "/#.*/d"                                           /etc/apt/sources.list
#RUN sed -i -r "/^$/d"                                            /etc/apt/sources.list
#RUN cat /etc/apt/sources.list
#RUN sed -i -r "s#deb .* noble(-.*)? #deb ${MIRROR} noble\\1 #"   /etc/apt/sources.list
#RUN cat /etc/apt/sources.list

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
