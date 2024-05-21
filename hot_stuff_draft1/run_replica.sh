#!/bin/bash

docker build --target replica . && docker run -it --network=host -e REPLICA_ID=$1 $(docker build -q --target replica .)