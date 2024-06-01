#!/bin/bash

docker build --target logger . && docker run --rm -p 5080:5080 $(docker build -q --target logger .)
