# Hot stuff implementation for Stanford CS244B

This is an implementation of the event based Hot Stuff consensus protocol for Stanford CS244B course project.

Replicas are gRPC servers that are requested to by clients. Replicas maintain gRPC sessions among each other for co-ordination.

Status: We have a working implementation of happy path right now. Finding for `TODO` in the codebase will tell you what parts are not yet implemented.


## Set up

You need python3.10 and Docker.

To set up the development environment run the following:
```
python3.10 -m venv env
source env/bin/activate
pip install -r requirements.txt
```

Everytime you make any change to HotStuff.proto, you must run the following from within the virtual environment to update class definitions.
```
python -m grpc_tools.protoc -I. --python_out=. --pyi_out=. --grpc_python_out=. src/proto/HotStuff.proto
```

## Running

The replicas and clients are run as docker containers that communicate with each other. All variables have names and functions and classes have documentation that will help understand the protocol.

TODO: use docker compose instead of CLI that uses host network for containers. This will need better logging functionality.

The configuration of replicas and clients is expected to be in a file called configs.json. This is used by replicas and clients to establish connections with each other and also reply to the clients.

We use logging level DEBUG for verbose logs and INFO for final outputs. To disable DEBUG logs, set loggin level to INFO in main of `client.py` and `replica.py`.

### Replicas and Logging

A replica is a gRPC server that implements the HotStuff protocol. The following command will run the replicas, start the server and establish connections with other replicas after a small delay (to ensure other replicas are up). It will also start a logging service where all messages get logged to a common logging server.
```
docker compose up --remove-orphans --build -d
```
To understand the protocol, refer to `proto/HotStuff.ptoto` for the server interface and `replica_server.py` for its implementation.

The log dashboard can be accessed at `localhost:5080`. The login credentials are `root@example.com` and `Complexpass#123`.

Once done executing, run the following for cleaning up:
```
docker compose down
```

### Client

The following script starts a client that establishes sessions with other replicas waits for user input. The user input is sent to all replicas as a request which is exepcted to be executed by the replicas.

To run client:
```
sh run_client.sh <client_id> #c0
```

## Testing

Run replicas
```
docker compose up --build
```

Run client
```
timeout 5m docker compose -f compose_clients.yaml up --build &> test_logs/replicas_5_down_0_client_2_mean_4.txt
```