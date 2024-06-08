import logging
import logging.config
import time
from concurrent import futures
from threading import Thread, Event

import grpc
from common import GlobalConfig, ReplicaConfig, get_replica_config, get_executor
from proto.HotStuff_pb2_grpc import add_HotStuffReplicaServicer_to_server
from replica_server import ReplicaServer
from proto import CatchUp_pb2, CatchUp_pb2_grpc

from pacemaker import Pacemaker, PacemakerEventStatus

logging.config.fileConfig('logging.ini', disable_existing_loggers=True)
log = logging.getLogger(__name__)

class CatchUpServicer(CatchUp_pb2_grpc.NodeSenderServicer):
    """Implements the CatchUpServicer interface defined in the proto file. Use for catching up requests that have fallen behind."""

    def __init__(self, replica_server: ReplicaServer, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.replica_server = replica_server

    def send_node(self, request, _):
        """Handles a request to send a node."""
        if request.node_id in self.replica_server.tree.nodes:
            return CatchUp_pb2.NodeResponse(
                node=self.replica_server.tree.nodes[request.node_id].to_bytes())
        else:
            return CatchUp_pb2.NodeResponse()

    @staticmethod
    def serve(catchup_servicer, port):
        server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
        CatchUp_pb2_grpc.add_NodeSenderServicer_to_server(catchup_servicer, server)
        server.add_insecure_port('[::]:1' + port)
        server.start()
        server.wait_for_termination()


HEARTBEAT_EVENT_TIMEOUT = 5
NEW_SYNC_TIMEOUT = 20

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

    # Start catchup service
    Thread(
        target=CatchUpServicer.serve, 
        args=(CatchUpServicer(replica_server), config.port), 
        daemon=True
    ).start()

    # Start gRPC server
    serve(replica_server, config, pacemaker)
