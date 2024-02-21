#!/bin/bash
find ./project2/* -iname *.hh -o -iname *.cc | xargs clang-format -i -style=Google