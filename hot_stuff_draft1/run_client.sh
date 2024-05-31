#!/bin/bash

docker build --target client . && docker run --network=host -it -e CLIENT_ID=$1 $(docker build -q --target client .)