# raft1
Raft projects for CS 190 for Ava Jih-Schiff and Gordon Martinez-Piedra

As a preconditions you must have homebrew installed which you can do via these
instructions:
https://docs.brew.sh/Installation

Note this setup.sh assumes that you have not got protobufs, abseil or libconfig
set up and performs all these installations for you.

Run ./setup.sh from the root directory to do all the necessary installation

Once you have done this delete the first four lines of
install/lib/pkgconfig/libconfig++.pc and copy the first four lines of
install/lib/pkgconfig/protobuf.cc to the top of
install/lib/pkgconfig/libconfig++.pc

This will ensure that you can dynamically link libconfig++

You should now be ready to make the server and client by calling 'make'
To run a server or client run these commands respectively:
- ./build/server
- ./build/client



