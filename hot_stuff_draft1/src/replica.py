import time
from concurrent import futures
from threading import Lock, Thread
from typing import List

import grpc
from common import (GlobalConfig, ReplicaConfig,
                    get_replica_config, get_replica_sessions)
from proto.HotStuff_pb2 import EchoResponse, EchoRequest
from proto.HotStuff_pb2_grpc import (HotStuffReplicaServicer,
                                     add_HotStuffReplicaServicer_to_server)
from tree import Tree


# TODO: make better thread safe
# TODO: implement QC
class ReplicaServer(HotStuffReplicaServicer):
    def __init__(self):
        self.lock = Lock()
        with self.lock:
            self.replica_sessions = []
            self.tree = Tree()
            root = self.tree.get_root()
            self.vheight = root.height
            self.b_lock = root
            self.b_exec = root
            self.b_leaf = root
            self.qc_high = root.justify

    def establish_sessions_with_delay(self, global_config: GlobalConfig, delay: int):
        time.sleep(delay)
        with self.lock:
            self.replica_sessions = get_replica_sessions(global_config)
        print("Established sessions with replicas")

    # When received echo from client, also send to all replicas
    # Just for verification of communication between replicas
    def Echo(self, request, context):
        if request.msg.startswith("REPLICA:"):
            print("Received from replica: '''"+request.msg+"'''")
        elif request.msg.startswith("CLIENT:"):
            print("Received from client: '''"+request.msg+"'''")
            with self.lock:
                for replica in self.replica_sessions:
                    response = replica.stub.Echo(
                        EchoRequest(sender_id=request.sender_id,
                                    msg='REPLICA: '+request.msg)
                    )
                    print(response.msg)
        return EchoResponse(msg="Echoed: '''" + request.msg + "'''")


def establish_sessions_with_delay(replica_server: ReplicaServer, global_config: GlobalConfig, delay: int = 5):
    Thread(target=replica_server.establish_sessions_with_delay,
           args=(global_config, delay)).start()


def serve(replica_server: ReplicaServer, config: ReplicaConfig, ):
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    add_HotStuffReplicaServicer_to_server(replica_server, server)
    server.add_insecure_port('[::]:'+str(config.port))
    print("Listining on port: "+str(config.port)+", replica id: " +
          str(config.id)+", public key: "+config.public_key)
    server.start()
    server.wait_for_termination()


if __name__ == '__main__':
    # Read configs
    config, global_config = get_replica_config()

    # Set up server
    replica_server = ReplicaServer()

    # Establish sessions with other replicas
    # This is done after a delay to ensure all servers are up
    establish_sessions_with_delay(replica_server, global_config)

    serve(replica_server, config)
