# Docker builds of displaz

## Motivation

Check the displaz builds and tests for a variety of Linux flavours.

## User Guide

1. `docker build --pull docker/ubuntu_20_04`
2. `docker image prune -af`

## Options

* REPO build from specified from git repository
* MIRROR use Ubuntu package mirror
  * Australia: `http://au.archive.ubuntu.com/ubuntu/`

For example:

`docker build --pull docker/ubuntu_24_04 --build-arg MIRROR=http://au.archive.ubuntu.com/ubuntu/ `

## Notes

* Ubuntu 12.04 Precise does not support C++11 required for displaz
* Ubuntu 16.04 Xenial has Qt 5.5.1 - not working
* Ubuntu 18.04 Bionic has Qt 5.9.5 - not working