#!/bin/bash

docker run -v "${PWD}:/root" --ipc="host" --privileged -ti agodio/itba-so-multi-platform:3.0

