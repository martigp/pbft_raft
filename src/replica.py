import logging
import logging.config
import time
from concurrent import futures
from threading import Thread

import grpc
from common import GlobalConfig, ReplicaConfig, get_replica_config
from proto.HotStuff_pb2_grpc import add_HotStuffReplicaServicer_to_server
from replica_server import ReplicaServer

logging.config.fileConfig('logging.ini', disable_existing_loggers=True)
log = logging.getLogger(__name__)

def establish_sessions_with_delay(replica_server: ReplicaServer, global_config: GlobalConfig, delay: int):
    """Establish sessions with other replicas after a delay.

    The delay is to ensure all servers are up before establishing sessions.
    """
    time.sleep(delay)
    replica_server.establish_sessions(global_config)


def serve(replica_server: ReplicaServer, config: ReplicaConfig):
    """Start the gRPC server for the replica listening on the specified port."""
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    add_HotStuffReplicaServicer_to_server(replica_server, server)
    server.add_insecure_port('[::]:'+str(config.port))
    log.info("Listining on port: %s, replica id: %s, public key: %s",
             config.port, config.id, config.public_key)
    server.start()
    server.wait_for_termination()


if __name__ == '__main__':
    # Read configs
    config, global_config = get_replica_config()

    pks = []
    for replicaconfig in global_config.replica_configs:
        pks.append(replicaconfig.public_key)

    # Set up server
    replica_server = ReplicaServer(config, pks, global_config.client_configs)

    # Establish sessions with other replicas
    # This is done after a delay to ensure all servers are up
    delay = 10
    log.info("Establishing sessions with other replicas after a delay of %d seconds", delay)
    Thread(target=establish_sessions_with_delay, args=(
        replica_server, global_config, delay)).start()

    # Start gRPC server
    serve(replica_server, config)
