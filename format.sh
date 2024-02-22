#!/bin/bash
find ./proj2/* -iname *.hh -o -iname *.cc | xargs clang-format -i -style=Google
