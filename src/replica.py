import logging
import logging.config
import time
from concurrent import futures
from threading import Thread, Event

import grpc
from common import GlobalConfig, ReplicaConfig, get_replica_config, get_executor
from proto.HotStuff_pb2_grpc import add_HotStuffReplicaServicer_to_server
from replica_server import ReplicaServer

from pacemaker import Pacemaker, PacemakerEventStatus

logging.config.fileConfig('logging.ini', disable_existing_loggers=True)
log = logging.getLogger(__name__)

HEARTBEAT_EVENT_TIMEOUT = 5
NEW_SYNC_TIMEOUT = 5

def wait_on_heartbeat_event(pacemaker : Pacemaker):
    while True:
        pacemaker.heartbeat_event.wait(timeout=HEARTBEAT_EVENT_TIMEOUT)
        if pacemaker.heartbeat_event.status() == PacemakerEventStatus.TIMED_OUT:
            log.debug(f"HEARTBEAT TIMEOUT")
            pacemaker.central_control_event.set()
            pacemaker.central_control_event.wait(PacemakerEventStatus.NOT_SET)
        else:
            pacemaker.heartbeat_event.set(PacemakerEventStatus.NOT_SET)

def wait_on_new_sync_event(pacemaker : Pacemaker):
    while True:
        pacemaker.new_view_event.wait(timeout=NEW_SYNC_TIMEOUT)
        if pacemaker.new_view_event.status() == PacemakerEventStatus.TIMED_OUT:
            log.debug(f"NEW VIEW TIMEOUT")
            pacemaker.central_control_event.set()
            pacemaker.central_control_event.wait(PacemakerEventStatus.NOT_SET)
        else:
            pacemaker.new_view_event.set(PacemakerEventStatus.NOT_SET)

def serve(replica_server: ReplicaServer, config: ReplicaConfig, pacemaker : Pacemaker):
    """Start the gRPC server for the replica listening on the specified port."""
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    add_HotStuffReplicaServicer_to_server(replica_server, server)
    server.add_insecure_port('[::]:'+str(config.port))
    log.info("Listining on port: %s, replica id: %s, public key: %s",
             config.port, config.id, config.public_key)
    server.start()

    time.sleep(10)

    heartbeat_thread = Thread(target=wait_on_heartbeat_event, args=(pacemaker,),daemon=True)
    
    new_sync_thread = Thread(target=wait_on_new_sync_event,
                              args=(pacemaker,),daemon=True)
    
    # heartbeat_thread.start()
    new_sync_thread.start()


    while True:
        pacemaker.central_control_event.wait()
        with pacemaker.central_control_event._cond:
            print("CENTRAL CONTROL THREAD RECEIVED EVENT")
            # Check for a timer timeout, if this happens then we should call
            if server._state.termination_event.is_set():
                # heartbeat_thread.stop()
                new_sync_thread.stop()
                break

            heartbeat_status  = pacemaker.heartbeat_event.status()
            if heartbeat_status == PacemakerEventStatus.TIMED_OUT:
                next_req  = pacemaker.get_next_req()
                replica_server.on_beat(next_req)
                pacemaker.heartbeat_event.set(PacemakerEventStatus.NOT_SET)
            
            new_view_status  = pacemaker.new_view_event.status()
            if new_view_status == PacemakerEventStatus.TIMED_OUT:
                log.debug("CENTRAL CONTROL Calling new view")
                replica_server.on_new_view_sync()
                pacemaker.new_view_event.set(PacemakerEventStatus.NOT_SET)
            
        pacemaker.central_control_event.set(PacemakerEventStatus.NOT_SET)

if __name__ == '__main__':
    # Read configs
    config, global_config = get_replica_config()
    executor = get_executor()

    pks = []
    for replicaconfig in global_config.replica_configs:
        pks.append(replicaconfig.public_key)

    # New View Event:
    pacemaker = Pacemaker()

    # New 

    #
    # Set up server
    replica_server = ReplicaServer(config, pks, global_config.client_configs, pacemaker, executor, global_config.F)

    log.info("Establishing sessions with other replicas")
    replica_server.establish_sessions(global_config)

    # Start gRPC server
    serve(replica_server, config, pacemaker)
