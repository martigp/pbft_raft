import logging
from threading import Lock
from typing import Dict, Set

from common import GlobalConfig, ReplicaSession, get_replica_sessions
from proto.HotStuff_pb2 import (EchoRequest, EchoResponse, EmptyResponse,
                                ProposeRequest, VoteRequest)
from proto.HotStuff_pb2_grpc import HotStuffReplicaServicer
from tree import QC, Node, Tree, node_from_bytes

log = logging.getLogger(__name__)

F = 1


class ReplicaServer(HotStuffReplicaServicer):
    """Replica server implementation.

    This class implements the gRPC endpoints for the replica. 
    Main class implementing the protocol.
    """

    N: int
    """Number of replicas."""
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

    def establish_sessions(self, global_config: GlobalConfig):
        """Establish sessions with other replicas after a delay.

        The delay is to ensure all servers are up before establishing sessions.
        Helper function not relevant to the protocol.
        """
        with self.lock:
            self.replica_sessions = get_replica_sessions(global_config)
            self.N = len(self.replica_sessions)
        log.info("Established sessions with all replicas")

    def get_leader_id(self) -> str:
        """Get the leader id."""
        return 'replica0'

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
                    return replica
        raise ValueError(f'Replica {id} not found')

    def add_vote(self, node_id: str, sender_id: str) -> int:
        if node_id not in self.votes:
            self.votes[node_id] = set()
        self.votes[node_id].add(sender_id)
        return len(self.votes[node_id])

    def Echo(self, request, context):
        if request.sender_id in [replica.config.id for replica in self.replica_sessions]:
            log.info(
                f"Received from replica {request.sender_id}: '''{request.msg}'''")
        else:
            log.info(
                f"Received from client {request.sender_id}: '''{request.msg}'''")
            with self.lock:
                for replica in self.replica_sessions:
                    response = replica.stub.Echo(EchoRequest(
                        sender_id=self.id, msg=request.msg))
                    log.info(
                        f"Received response from replica {replica.config.id}: '''{response.msg}'''")
        return EchoResponse(msg=f"Echoed: '''{request.msg}'''")

    #############################
    # Internal procedures related to HotStuff start here
    #############################
    def execute(self, cmd: str):
        """Execute a command.

        For now, it just prints the command.
        Idally, it should append to some log file.
        """
        log.info(f"Executing command: {cmd}")

    def update_qc_high(self, received_qc: QC):
        """Update the highest QC seen so far. Also set it as b_leaf.

        Does nothing if the height of the QC is less than the height of the current highest QC.
        """
        received_qc_node = self.tree.get_node(received_qc.node_id)
        my_qc_high_node = self.tree.get_node(self.qc_high.node_id)
        if received_qc_node.height > my_qc_high_node.height:
            log.debug(
                f"Updating qc_high from {my_qc_high_node} to {received_qc_node} and seeing it as leaf")
            self.qc_high = received_qc
            self.leaf_node = received_qc_node
        else:
            log.debug(
                f"Skipping update of qc_high from {my_qc_high_node} to {received_qc_node}")

    def commit(self, node: Node):
        """Commit and execute a node if it's higher than b_exec.

        Also do the same for all the ancestors of the node.
        """
        if self.executed_node.height < node.height:
            log.debug(f"Commiting {node}")
            self.commit(self.tree.get_node(node.parent_id))
            self.execute(node.cmd)
        else:
            log.debug(f"Skipping commit of {node}")

    def update(self, node: Node):
        """Update the replica state.

        This is executed when we receive a proposal for a node.
        I think this should also add the node to the tree if its not present already,
        but not sure yet.
        """
        log.debug(f"Updating {node}")
        # TODO: Check if lock is required here
        node_jp_dp = self.tree.get_node(node.justify.node_id)  # b'' in paper
        node_jgp_p = self.tree.get_node(
            node_jp_dp.justify.node_id)  # b' in paper
        node_jggp_b = self.tree.get_node(
            node_jgp_p.justify.node_id)  # b in paper

        log.debug(
            f"Justify ancestors: {node} -> {node_jp_dp} -> {node_jgp_p} -> {node_jggp_b}")

        self.update_qc_high(node.justify)
        if node_jgp_p.height > self.locked_node.height:
            log.debug(f"Locking {node_jgp_p} over {self.locked_node}")
            # node_jgp enters commit phase
            self.locked_node = node_jgp_p
        else:
            log.debug(f"Skipping lock of {node_jgp_p} over {self.locked_node}")

        if node_jp_dp.parent_id == node_jgp_p.id and node_jgp_p.parent_id == node_jggp_b.id:
            # node_jggp can be executed now
            self.commit(node_jggp_b)
            log.debug(
                f"Updating executed_node from {self.executed_node} to {node_jggp_b}")
            self.executed_node = node_jggp_b

    #############################
    # Hot stuff protocol endpoints start here
    #############################

    def Beat(self, request, context):
        if self.is_leader():
            log.debug(f"Received command from client: {request.cmd}")
            with self.lock:
                new_node = self.tree.create_node(
                    request.cmd, self.leaf_node.id, self.qc_high)

                log.debug(f"Proposing {new_node} and setting it as leaf")
                self.leaf_node = new_node

            # This should be outside lock as it will call Propose on the
            # leader and will lead to a deadlock otherwise.
            for replica in self.replica_sessions:
                replica.stub.Propose(ProposeRequest(
                    sender_id=self.id, node=new_node.to_bytes()))

        return EmptyResponse()

    def Propose(self, request, context):
        to_vote = False
        with self.lock:
            new_node = node_from_bytes(request.node)
            if not self.is_leader():
                log.debug(
                    f"Received proposal {new_node} from leader {request.sender_id}")
                self.tree.add_node(new_node)

            always_true = new_node.height > self.vheight  # Always true for the happy path
            happy_path = self.tree.is_ancestor(
                self.locked_node.id, new_node.id)
            sad_path = self.tree.get_node(
                new_node.justify.node_id).height > self.locked_node.height
            log.debug(
                f"Proposal: {always_true} and ({happy_path} or {sad_path})")
            if always_true and (happy_path or sad_path):
                self.vheight = new_node.height

                # Call to the vote function must happen outside the lock
                # otherwise this will cause a deadlock in leader
                to_vote = True

            self.update(new_node)

        if to_vote:
            log.debug(f"Voting for {new_node}")
            leader_session = self.get_session(self.get_leader_id())
            leader_session.stub.Vote(VoteRequest(
                sender_id=self.id, node=new_node.to_bytes()))
        return EmptyResponse()

    def Vote(self, request, context):
        node = node_from_bytes(request.node)
        log.debug(f"Received vote for {node} from {request.sender_id}")
        with self.lock:
            if self.add_vote(node.id, request.sender_id) > self.N - F:
                log.debug(f"Got enough votes for {node}")
                self.update_qc_high(QC(node.id))
        return EmptyResponse()
