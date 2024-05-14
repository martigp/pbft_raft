# raft implementation
Raft projects taken from Stanford CS 190 project for Ava Jih-Schiff and Gordon Martinez-Piedra

# Set Up
As a preconditions you must have homebrew installed which you can do via these
instructions:
https://docs.brew.sh/Installation

Note this setup.sh assumes that you do not have protobufs, abseil or libconfig
set up and performs all these installations for you. It will also perform updates
on these libraries if run again.

Run `./setup.sh` from the root directory to do all the necessary installation
NOTE - that when you run brew install abseil, everything related to abseil
will be in the /opt/homebrew/Cellar/abseil/xxxx directory. You may have to change
xxxx (currently 20240116.2) to whatever value you have there.

Once you have done this delete the first four lines of
`install/lib/pkgconfig/libconfig++.pc` and copy the first four lines of
`install/lib/pkgconfig/protobuf.cc` to the top of
`install/lib/pkgconfig/libconfig++.pc`

This will ensure that you can dynamically link libconfig++
Note that we use the libconfig for easy use of configuration files.

# Running Servers and Clients
Once you have completed the set-up instructions, run `make` from the `raft` 
directory. You will need to run the command before you run any of the executables
to be able to load the dlls and use the protobuf compiler


`export DYLD_LIBRARY_PATH=<path_to_repo>/install/runtime_libs`
`export PATH=$PATH:<path_to_repo>/install/bin`

## Run a server
First, ensure that the files `config_ID_*.cfg` within `raft` are up-to-date
with the correct port and address numbers as required for your Raft Cluster.

Run each server from the `raft` directory in separate terminals by calling the 
command:

`./build/server config_ID_*.cfg`


Access help and usage information with:

`./build/server -h`


Indicate that a server is new and booting for the first time(and thus does not require
pre-existing persistent state):

`./build/server -n config_ID_*.cfg`

## Run a client
First, ensure that the file `config_client.cfg` within `raft` is up-to-date
with the correct port and address numbers as required for your Raft Cluster.

Run each client from the `proj2` directory in separate terminals by calling the 
command:

`./build/client`



