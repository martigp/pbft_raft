import logging
import logging.config
from threading import Lock
from typing import Dict, Set

from itertools import chain, combinations

from common import GlobalConfig, ReplicaConfig, ReplicaSession, get_replica_sessions
from proto.HotStuff_pb2 import (EchoRequest, EchoResponse, EmptyResponse,
                                ProposeRequest, VoteRequest)
from proto.HotStuff_pb2_grpc import HotStuffReplicaServicer
from tree import QC, Node, Tree, node_from_bytes
from crypto import partialSign, parsePK, parseSK, verifySigs

logging.config.fileConfig('logging.ini', disable_existing_loggers=True)

F = 1


class ReplicaServer(HotStuffReplicaServicer):
    """Replica server implementation.

    This class implements the gRPC endpoints for the replica. 
    Main class implementing the protocol.
    """

    num_replica: int  # Number of replicas
    lock: Lock  # Coarse grain lock. For now we lock during the entire execution of a function
    tree: Tree  # Tree structure to store the nodes. Handles creation and modification of nodes
    votes: Dict[str, Set[str]]  # Mapping from node_id to votes for that node
    vheight: int  # Height of the highest node that the replica voted for
    locked_node: Node  # Node in commit phase. Highest node for which we have justify-grandchildren
    executed_node: Node  # Highest node that has been executed
    leaf_node: Node  # Highest node in the tree
    qc_high: QC  # Highest QC seen so far
    log: logging.Logger  # Logger


    def __init__(self, config : ReplicaConfig, pks : list[str]):
        self.id = config.id
        self.public_key = parsePK(config.public_key)
        self.secret_key = parseSK(config.secret_key)
        self.replica_pks = [parsePK(pk_str) for pk_str in pks]
        self.log = logging.getLogger(self.id)

        self.lock = Lock()
        with self.lock:
            self.replica_sessions = []
            self.votes = {}
            self.tree = Tree(self.id, config.root_qc_sig)
            self.view_number = 1
            root = self.tree.get_root_node()
            self.vheight = root.height
            self.locked_node = root
            self.executed_node = root
            self.leaf_node = root
            self.qc_high = root.justify
            print(self.qc_high.node_id)

    def establish_sessions(self, global_config: GlobalConfig):
        """Establish sessions with other replicas after a delay.

        The delay is to ensure all servers are up before establishing sessions.
        Helper function not relevant to the protocol.
        """
        with self.lock:
            self.replica_sessions = get_replica_sessions(global_config)
            self.num_replica = len(self.replica_sessions)
        self.log.info("Established sessions with all replicas")

    def get_leader_id(self) -> str:
        """Get the leader id."""
        # return f"r{self.viewNumber - 1 % self.N}"
        return "r0"

    def is_leader(self) -> bool:
        """Check if the replica is the leader."""
        return self.id == self.get_leader_id()

    def get_session(self, _id: str) -> ReplicaSession:
        """Get the session of the replica with the specified id.

        Helper function not relevant to the protocol.
        """
        with self.lock:
            for replica in self.replica_sessions:
                print(replica.config.id)
                if replica.config.id == _id:
                    return replica
        raise ValueError(f'Replica {_id} not found')

    def add_vote(self, node_id: str, vote_reqeust: VoteRequest) -> int:
        if node_id not in self.votes:
            self.votes[node_id] = set()
        # Add the tuple of sender_id and the signature for that node
        self.votes[node_id].add((vote_reqeust.sender_id, vote_reqeust.partial_sig))
        print(f"Node {node_id} received {len(self.votes[node_id])} votes")
        return len(self.votes[node_id])

    def check_votes(self, node_id: str):
        """
        Checks whether there are sufficient valid signatures for node identified
        by node_id. If yes, returns the aggregate signature and corresponding
        server_ids of who contributed to the aggregate signature.
        """
        votes = self.votes[node_id]
        message = self.tree.get_node(node_id).to_bytes()

        # Slow way to do checking without a proper partial Sig library
        votes_powerset = list(
            chain.from_iterable(combinations(votes, r)
            for r in range(self.num_replica - F, len(votes)+1)))
        for vote_set in votes_powerset:
            sigs = []
            pkids = []
            for sender_id, sig in vote_set:
                sigs.append(sig)
                pkids.append(int(sender_id[-1]))

            pks = [self.replica_pks[pkid] for pkid in pkids]
            verified, agg_sig = verifySigs(message, sigs, pks)
            if verified:
                return verified, agg_sig, pkids
        return False, None, None

    def Echo(self, request, context):
        if request.sender_id in [replica.config.id for replica in self.replica_sessions]:
            self.log.info("Received from replica %s: '''%s'''", request.sender_id, request.msg)
        else:
            self.log.info("Received from client %s: '''%s'''", request.sender_id, request.msg)
            with self.lock:
                for replica in self.replica_sessions:
                    response = replica.stub.Echo(EchoRequest(
                        sender_id=self.id, msg=request.msg))
                    self.log.info("Received response from replica %s: '''%s'''",
                             replica.config.id, response.msg)
        return EchoResponse(msg=f"Echoed: '''{request.msg}'''")

    #############################
    # Internal procedures related to HotStuff start here
    #############################
    def execute(self, cmd: str):
        """Execute a command.

        For now, it just prints the command.
        Idally, it should append to some log file.
        """
        self.log.info("Executing command: %s", cmd)

    def update_qc_high(self, received_qc: QC):
        """Update the highest QC seen so far. Also set it as b_leaf.

        Does nothing if the height of the QC is less than the height of the current highest QC.
        """
        received_qc_node = self.tree.get_node(received_qc.node_id)
        my_qc_high_node = self.tree.get_node(self.qc_high.node_id)
        if received_qc_node.height > my_qc_high_node.height:
            self.log.debug("Updating qc_high from %s to %s and seeing it as leaf",
                      my_qc_high_node, received_qc_node)
            self.qc_high = received_qc
            self.leaf_node = received_qc_node
        else:
            self.log.debug("Skipping update of qc_high from %s to %s",
                      my_qc_high_node, received_qc_node)

    def commit(self, node: Node):
        """Commit and execute a node if it's higher than b_exec.

        Also do the same for all the ancestors of the node.
        """
        if self.executed_node.height < node.height:
            self.log.debug("Commiting %s", node)
            self.commit(self.tree.get_node(node.parent_id))
            self.execute(node.cmd)
        else:
            self.log.debug("Skipping commit of %s", node)

    def update(self, node: Node):
        """Update the replica state.

        This is executed when we receive a proposal for a node.
        I think this should also add the node to the tree if its not present already,
        but not sure yet.
        """
        self.log.debug("Updating %s", node)
        # TODO: Check if lock is required here
        node_jp_dp = self.tree.get_node(node.justify.node_id)  # b'' in paper
        node_jgp_p = self.tree.get_node(
            node_jp_dp.justify.node_id)  # b' in paper
        node_jggp_b = self.tree.get_node(
            node_jgp_p.justify.node_id)  # b in paper

        self.log.debug("Justify ancestors: %s -> %s -> %s -> %s",
                  node, node_jp_dp, node_jgp_p, node_jggp_b)

        self.update_qc_high(node.justify)
        if node_jgp_p.height > self.locked_node.height:
            self.log.debug("Locking %s over %s", node_jgp_p, self.locked_node)
            # node_jgp enters commit phase
            self.locked_node = node_jgp_p
        else:
            self.log.debug("Skipping lock of %s over %s", node_jgp_p, self.locked_node)

        if node_jp_dp.parent_id == node_jgp_p.id and node_jgp_p.parent_id == node_jggp_b.id:
            # node_jggp can be executed now
            self.commit(node_jggp_b)
            self.log.debug("Updating executed_node from %s to %s", self.executed_node, node_jggp_b)
            self.executed_node = node_jggp_b


    #############################
    # Hot stuff protocol endpoints start here
    #############################

    def Beat(self, request, context):
        if self.is_leader():
            self.log.debug("Received command from client: %s", request.cmd)
            with self.lock:
                new_node = self.tree.create_node(
                    request.cmd, self.leaf_node.id, request.sender_id,
                    self.qc_high, self.view_number)
                self.log.debug("New node's jusitfy node id %s", new_node.justify.node_id)
                self.log.debug("Proposing %s and setting it as leaf", new_node)
                self.leaf_node = new_node

            # This should be outside lock as it will call Propose on the
            # leader and will lead to a deadlock otherwise.
            for replica in self.replica_sessions:
                replica.stub.Propose(ProposeRequest(
                    sender_id=self.id, node=new_node.to_bytes()))
        return EmptyResponse()

    def Propose(self, request, context):
        to_vote = False
        print("In proposal receipt")
        with self.lock:
            new_node = node_from_bytes(request.node)
            if not self.is_leader():
                self.log.debug("Received proposal %s from leader %s",
                          new_node, request.sender_id)
                self.tree.add_node(new_node)
            always_true = new_node.height > self.vheight  # Always true for the happy path
            happy_path = self.tree.is_ancestor(
                self.locked_node.id, new_node.id)
            sad_path = self.tree.get_node(
                new_node.justify.node_id).height > self.locked_node.height
            self.log.debug("Proposal: %s and (%s or %s)", always_true, happy_path, sad_path)
            if always_true and (happy_path or sad_path):
                self.vheight = new_node.height

                # Call to the vote function must happen outside the lock
                # otherwise this will cause a deadlock in leader
                to_vote = True

            self.update(new_node)

        if to_vote:
            self.log.debug("Voting for %s", new_node)
            leader_session = self.get_session(self.get_leader_id())
            node_bytes = new_node.to_bytes()
            sig = bytes(partialSign(self.secret_key, node_bytes))
            vote = VoteRequest(sender_id=self.id, node=node_bytes, partial_sig=sig)
            leader_session.stub.Vote(vote)

        # This might be incorrect, do we only update View when we receive a valid
        # proposal or do we update either way.
        with self.lock:
            self.view_number += 1

        return EmptyResponse()

    def Vote(self, request, context):
        node = node_from_bytes(request.node)
        self.log.debug("Received vote for %s from %s with signature %s",
                  node, request.sender_id, request.partial_sig[:5])
        with self.lock:
            if self.add_vote(node.id, request) >= self.num_replica - F:
                # Put in logic for checking
                verified, agg_sig, pkids =  self.check_votes(node.id)
                if verified:
                    self.log.debug("Got enough votes for %s", node)
                    new_qc = QC(node.id, node.view_number, bytes(agg_sig), pkids)
                    self.update_qc_high(new_qc)
                    self.view_number += 1
        return EmptyResponse()
