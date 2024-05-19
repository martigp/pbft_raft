import time
from concurrent import futures
from threading import Lock, Thread
from typing import Dict, Set

import grpc
from common import (GlobalConfig, ReplicaConfig, ReplicaSession,
                    get_replica_config, get_replica_sessions)
from proto.HotStuff_pb2 import EchoResponse, EchoRequest, BeatResponse
from proto.HotStuff_pb2_grpc import (HotStuffReplicaServicer,
                                     add_HotStuffReplicaServicer_to_server)
from tree import Tree, QC, Node


class ReplicaServer(HotStuffReplicaServicer):
    """Replica server implementation.

    This class implements the gRPC endpoints for the replica. 
    Main class implementing the protocol.
    """

    lock: Lock
    """Coarse grain lock. 
    
    For now we lock during the entire execution of a function.
    """
    tree: Tree
    """Tree structure to store the nodes. Handles creation and modification of nodes."""
    votes: Dict[str, Set[str]]
    """Mapping from node_id to votes for that node."""
    vheight: int
    """Height of the highest node that the replica voted for."""
    locked_node: Node
    """The node in commit phase.
    
    Highest node for which we have justify-grandchildren.
    """
    executed_node: Node
    """The highest node that has been executed."""
    leaf_node: Node
    """The highest node in the tree."""
    qc_high: QC
    """The highest QC seen so far."""

    def __init__(self, id: str):
        self.id = id

        self.lock = Lock()
        with self.lock:
            self.replica_sessions = []
            self.votes = {}
            self.tree = Tree()
            root = self.tree.get_root_node()
            self.vheight = root.height
            self.locked_node = root
            self.executed_node = root
            self.leaf_node = root
            self.qc_high = root.justify

    def establish_sessions_with_delay(self, global_config: GlobalConfig, delay: int):
        """Establish sessions with other replicas after a delay.

        The delay is to ensure all servers are up before establishing sessions.
        Helper function not relevant to the protocol.
        """
        time.sleep(delay)
        with self.lock:
            self.replica_sessions = get_replica_sessions(global_config)
        print("Established sessions with replicas")

    def get_leader_id(self) -> str:
        """Get the leader id."""
        return str(0)

    def is_leader(self) -> bool:
        """Check if the replica is the leader."""
        return self.id == self.get_leader_id()

    def get_session(self, id: str) -> ReplicaSession:
        """Get the session of the replica with the specified id.

        Helper function not relevant to the protocol.
        """
        with self.lock:
            for replica in self.replica_sessions:
                if replica.id == id:
                    return replica
        raise ValueError(f'Replica {id} not found')

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

    #############################
    # Internal procedures related to HotStuff start here
    #############################
    def execute(self, cmd: str):
        """Execute a command.

        For now, it just prints the command.
        Idally, it should append to some log file.
        """
        print("Executing command: "+cmd)

    def update_qc_high(self, qc: QC):
        """Update the highest QC seen so far. Also set it as b_leaf.

        Does nothing if the height of the QC is less than the height of the current highest QC.
        """
        with self.lock:
            qc_node = self.tree.get_node(qc.node_id)
            qc_high_node = self.tree.get_node(self.qc_high.node_id)
            if qc_node.height > qc_high_node.height:
                self.qc_high = qc
                self.leaf_node = qc_node

    def commit(self, node: Node):
        """Commit and execute a node if it's higher than b_exec.

        Also do the same for all the ancestors of the node.
        """
        with self.lock:
            if self.executed_node.height < node.height:
                self.commit(self.tree.get_node(node.parent_id))
                self.execute(node.cmd)

    def update(self, node: Node):
        """Update the replica state.
        
        This is executed when we receive a proposal for a node.
        I think this should also add the node to the tree if its not present already,
        but not sure yet.
        """
        with self.lock:
            node_jp = self.tree.get_node(node.justify.node_id) # b'' in paper
            node_jgp = self.tree.get_node(node_jp.justify.node_id) # b' in paper
            node_jggp = self.tree.get_node(node_jgp.justify.node_id) # b in paper

            self.update_qc_high(node.justify)
            if node_jgp.height > self.locked_node.height:
                # node_jgp enters commit phase
                self.locked_node = node_jgp
            
            if node_jp.parent_id == node_jgp and node_jgp.parent_id == node_jggp:
                # node_jggp can be executed now
                self.commit(node_jggp)
                self.executed_node = node_jggp


            
    #############################
    # Hot stuff protocol endpoints start here
    #############################
    def Beat(self, request, context):
        if self.is_leader():
            with self.lock:
                # new_node = self.tree.create_node(
                #     request.cmd, self.leaf_node, self.qc_high)

                # TODO: Broadcast to other replicas
                print("Broadcasting to other replicas: "+request.cmd)

                # self.leaf_node = new_node

        return BeatResponse()


def establish_sessions_async(replica_server: ReplicaServer, global_config: GlobalConfig, delay: int):
    """Establish sessions with other replicas after a delay.

    The delay is to ensure all servers are up before establishing sessions.
    """
    Thread(target=replica_server.establish_sessions_with_delay,
           args=(global_config, delay)).start()


def serve(replica_server: ReplicaServer, config: ReplicaConfig, ):
    """Start the gRPC server for the replica listening on the specified port."""
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
    replica_server = ReplicaServer(config.id)

    # Establish sessions with other replicas
    # This is done after a delay to ensure all servers are up
    establish_sessions_async(replica_server, global_config, delay=5)

    # Start gRPC server
    serve(replica_server, config)
