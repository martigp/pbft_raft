import logging
import logging.config
import time
from concurrent import futures
from threading import Thread, Event

import grpc
from common import GlobalConfig, ReplicaConfig, get_replica_config
from proto.HotStuff_pb2_grpc import add_HotStuffReplicaServicer_to_server
from replica_server import ReplicaServer

logging.config.fileConfig('logging.ini', disable_existing_loggers=True)
log = logging.getLogger(__name__)

TIMER_EVENT_TIMEOUT = 10

def establish_sessions_with_delay(replica_server: ReplicaServer, global_config: GlobalConfig, delay: int):
    """Establish sessions with other replicas after a delay.

    The delay is to ensure all servers are up before establishing sessions.
    """
    time.sleep(delay)
    replica_server.establish_sessions(global_config)


def serve(replica_server: ReplicaServer, config: ReplicaConfig, timer_event : Event):
    """Start the gRPC server for the replica listening on the specified port."""
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    add_HotStuffReplicaServicer_to_server(replica_server, server)
    server.add_insecure_port('[::]:'+str(config.port))
    log.info("Listining on port: %s, replica id: %s, public key: %s",
             config.port, config.id, config.public_key)
    server.start()
    
    time.sleep(10)
    while True:
        if not timer_event.wait(TIMER_EVENT_TIMEOUT):
        # Check for a timer timeout, if this happens then we should call
            if server._state.termination_event.is_set():
                break
            
            replica_server.on_new_view_sync()
        timer_event.clear()


if __name__ == '__main__':
    # Read configs
    config, global_config = get_replica_config()

    pks = []
    for replicaconfig in global_config.replica_configs:
        pks.append(replicaconfig.public_key)

    # Timer Thread:
    timer_event = Event()
    # Set up server
    replica_server = ReplicaServer(config, pks, global_config.client_configs, timer_event)

    # Establish sessions with other replicas
    # This is done after a delay to ensure all servers are up
    delay = 10
    log.info("Establishing sessions with other replicas after a delay of %d seconds", delay)
    Thread(target=establish_sessions_with_delay, args=(
        replica_server, global_config, delay)).start()

    # Start gRPC server
    serve(replica_server, config, timer_event)
