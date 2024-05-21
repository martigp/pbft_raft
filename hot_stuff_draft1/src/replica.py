import time
from concurrent import futures
from threading import Lock, Thread
from typing import Dict, Set

import grpc
from common import (GlobalConfig, ReplicaConfig, ReplicaSession,
                    get_replica_config, get_replica_sessions)
from proto.HotStuff_pb2 import EchoResponse, EchoRequest, EmptyResponse, ProposeRequest, VoteRequest
from proto.HotStuff_pb2_grpc import (HotStuffReplicaServicer,
                                     add_HotStuffReplicaServicer_to_server)
from tree import Tree, QC, Node, node_from_bytes

F = 1


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
            self.tree = Tree(self.id)
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
                if replica.config.id == id:
                    print("Found replica: "+id)
                    return replica
        raise ValueError(f'Replica {id} not found')
    
    def add_vote(self, node_id: str, sender_id: str):
        if node_id not in self.votes:
            self.votes[node_id] = set()
        self.votes[node_id].add(sender_id)

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
        print("Updating qc_high for node: "+qc.node_id+" and cmd: ", end="")
        # TODO: Check if lock is required here
        received_qc_node = self.tree.get_node(qc.node_id)
        print(received_qc_node.cmd)
        my_qc_high_node = self.tree.get_node(self.qc_high.node_id)
        if received_qc_node.height > my_qc_high_node.height:
            self.qc_high = qc
            self.leaf_node = received_qc_node

    def commit(self, node: Node):
        """Commit and execute a node if it's higher than b_exec.

        Also do the same for all the ancestors of the node.
        """
        print("Committing node: "+node.cmd+" "+node.id+" "+str(node.height))
        print("SELF executed_node: "+self.executed_node.cmd+" "+self.executed_node.id+" "+str(self.executed_node.height))
        # TODO: Check if lock is required here
        if self.executed_node.height < node.height:
            print("HERERERERE")
            self.commit(self.tree.get_node(node.parent_id))
            self.execute(node.cmd)

    def update(self, node: Node):
        """Update the replica state.

        This is executed when we receive a proposal for a node.
        I think this should also add the node to the tree if its not present already,
        but not sure yet.
        """
        print("Updating replica state for node: "+node.cmd)
        # TODO: Check if lock is required here
        node_jp_dp = self.tree.get_node(node.justify.node_id)  # b'' in paper
        node_jgp_p = self.tree.get_node(
            node_jp_dp.justify.node_id)  # b' in paper
        node_jggp_b = self.tree.get_node(
            node_jgp_p.justify.node_id)  # b in paper

        self.update_qc_high(node.justify)
        if node_jgp_p.height > self.locked_node.height:
            # node_jgp enters commit phase
            print("Node: "+node_jgp_p.cmd+" enters commit phase")
            self.locked_node = node_jgp_p

        print("node_jp_dp is "+node_jp_dp.cmd
              + ", node_jgp_p is "+node_jgp_p.cmd
              + ", node_jggp_b is "+node_jggp_b.cmd)
        print("node_jp_dp is "+node_jp_dp.id
              + ", node_jgp_p is "+node_jgp_p.id
              + ", node_jggp_b is "+node_jggp_b.id)
        print("node_jp_dp.parent_id is "+node_jp_dp.parent_id
              + ", node_jgp_p.parent_id is "+node_jgp_p.parent_id
              + ", node_jggp_b.parent_id is "+node_jggp_b.parent_id)
        if node_jp_dp.parent_id == node_jgp_p.id and node_jgp_p.parent_id == node_jggp_b.id:
            # node_jggp can be executed now
            
            print("Commiting node: "+node_jggp_b.cmd+" and its ancestors")
            self.commit(node_jggp_b)
            self.executed_node = node_jggp_b

    #############################
    # Hot stuff protocol endpoints start here
    #############################

    def Beat(self, request, context):
        if self.is_leader():
            print("Processing command from client: "+request.cmd)
            with self.lock:
                new_node = self.tree.create_node(
                    request.cmd, self.leaf_node.id, self.qc_high)

                print("Old leaf node: "+self.tree.to_string(self.leaf_node.id))
                print("New leaf node: "+self.tree.to_string(new_node.id))
                self.leaf_node = new_node

            # TODO: Think if this should be done inside the lock
            # It can block handling of gRPC call sent to self
            for replica in self.replica_sessions:
                print("Sending proposal to replica: "+replica.config.id)
                replica.stub.Propose(ProposeRequest(
                    sender_id=self.id, node=new_node.to_bytes()))

        return EmptyResponse()

    def Propose(self, request, context):
        print("Received proposal from leader: "+request.sender_id, flush=True)

        to_vote = False
        with self.lock:
            new_node = node_from_bytes(request.node)
            if not self.is_leader():
                self.tree.add_node(new_node)
                print("Added node to tree: "+self.tree.to_string(new_node.id))

            always_true = new_node.height > self.vheight  # Always true for the happy path
            happy_path = self.tree.is_ancestor(
                self.locked_node.id, new_node.id)
            sad_path = self.tree.get_node(
                new_node.justify.node_id).height > self.locked_node.height
            print("Conditions:", always_true, happy_path, sad_path)
            if always_true and (happy_path or sad_path):
                self.vheight = new_node.height

                # Vote for the proposal
                # This needs to be done outside the lock
                to_vote = True

            self.update(new_node)

        if to_vote:
            print("Voting for proposal: "+new_node.cmd)
            leader_session = self.get_session(self.get_leader_id())
            leader_session.stub.Vote(VoteRequest(
                sender_id=self.id, node=new_node.to_bytes()))
        return EmptyResponse()

    def Vote(self, request, context):
        print("Received vote from replica: "+request.sender_id)
        with self.lock:
            node = node_from_bytes(request.node)
            self.add_vote(node.id, request.sender_id)
            if len(self.votes[node.id]) > len(self.replica_sessions)-F:
                self.update_qc_high(QC(node.id))
        return EmptyResponse()


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
