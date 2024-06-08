import logging
import logging.config
from threading import Lock, Event
from typing import Dict, Set

from itertools import chain, combinations

from common import GlobalConfig, ReplicaConfig, ReplicaSession, ClientConfig, get_replica_sessions
from proto.HotStuff_pb2 import (ClientCommandRequest, EchoRequest, EchoResponse, EmptyResponse,
                                ProposeRequest, VoteRequest, NewViewRequest)
from proto.HotStuff_pb2_grpc import HotStuffReplicaServicer
from tree import QC, Node, Tree, node_from_bytes, qc_from_bytes
from crypto import partialSign, parsePK, parseSK, verifySigs
from executor import Executor
from pacemaker import Pacemaker

from client_history import ClientInformation
import grpc
from proto.Client_pb2 import Response
from proto.Client_pb2_grpc import HotStuffClientStub
from proto.CatchUp_pb2 import NodeRequest
from proto.CatchUp_pb2_grpc import NodeSenderStub

logging.config.fileConfig('logging.ini', disable_existing_loggers=True)


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
    global_config: GlobalConfig  # Global configuration
    new_view_event : Event # Timer event used to signal to reset the timer
    executor: Executor  # Application executor
    F: int # Number of Byzantine faults

    clientMap : Dict[str, ClientInformation] # All client infomration

    def __init__(self, config : ReplicaConfig, pks : list[str],
                 client_configs : list[ClientConfig], pacemaker : Pacemaker, executor : Executor, F : int):
        self.id = config.id
        self.F = F
        self.public_key = parsePK(config.public_key)
        self.secret_key = parseSK(config.secret_key)
        self.replica_pks = [parsePK(pk_str) for pk_str in pks]
        self.log = logging.getLogger(self.id)
        self.num_replica = len(pks)

        self.clientMap = dict()
        
        # Populating the client Map
        for client_config in client_configs:
            self.clientMap[client_config.id] = ClientInformation(client_config.public_key)



        self.lock = Lock()
        with self.lock:
            self.replica_sessions = []
            self.votes = {}
            self.tree = Tree(self.id, config.root_qc_sig)
            root = self.tree.get_root_node()
            self.vheight = root.height
            self.view_number = root.height + 1
            self.locked_node = root
            self.executed_node = root
            self.leaf_node = root
            self.qc_high = root.justify
            self.pacemaker = pacemaker
        self.executor = executor

    def establish_sessions(self, global_config: GlobalConfig):
        """Establish sessions with other replicas after a delay.

        The delay is to ensure all servers are up before establishing sessions.
        Helper function not relevant to the protocol.
        """
        with self.lock:
            self.replica_sessions = get_replica_sessions(global_config)
            self.num_replica = len(self.replica_sessions)                
        self.log.info("Established sessions with all replicas")
        self.global_config = global_config

    def get_leader_id(self) -> str:
        """Get the leader id."""
        return f"r{(self.view_number - 1) % self.num_replica}"
        # return "r0"

    def is_leader(self) -> bool:
        """Check if the replica is the leader."""
        return self.id == self.get_leader_id()

    def get_session(self, _id: str) -> ReplicaSession:
        """Get the session of the replica with the specified id.

        Helper function not relevant to the protocol.
        """
        with self.lock:
            for replica in self.replica_sessions:
                if replica.config.id == _id:
                    return replica
        raise ValueError(f'Replica {id} not found')

    def add_vote(self, node_id: str, vote_reqeust: VoteRequest) -> int:
        if node_id not in self.votes:
            self.votes[node_id] = set()
        # Add the tuple of sender_id and the signature for that node
        self.votes[node_id].add((vote_reqeust.sender_id, vote_reqeust.partial_sig))
        self.log.info(f"Node {node_id} received {len(self.votes[node_id])} votes")
        return len(self.votes[node_id])
    
    def check_votes(self, node_id: str):
        """
        Checks whether there are sufficient valid signatures for node identified
        by node_id. If yes, returns the aggregate signature and corresponding
        server_ids of who contributed to the aggregate signature.
        """
        votes = self.votes[node_id]
        try:
            message = self.tree.get_node(node_id).to_bytes()
        except KeyError:
            return False, None, None

        # Slow way to do checking without a proper partial Sig library
        votesPowerset = list(chain.from_iterable(combinations(votes, r) for r in range(self.num_replica - self.F, len(votes)+1)))
        for voteSet in votesPowerset:
            sigs = []
            pkids = []
            for sender_id, sig in voteSet:
                sigs.append(sig)
                pkids.append(int(sender_id[-1]))
        
            pks = [self.replica_pks[pkid] for pkid in pkids]
            verified, aggSig = verifySigs(message, sigs, pks)
            if verified:
                return verified, aggSig, pkids
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
    def execute(self, node: Node):
        """Execute a command.

        For now, it just prints the command.
        Idally, it should append to some log file.
        """
        self.log.info("Executing command: %s", node.cmd)
        response = self.executor.execute(node.cmd)
        client_id = node.client_id
        client = None
        for client_config in self.global_config.client_configs:
            if client_config.id == client_id:
                client = client_config
        self.log.info(f"Sending response to client {client_id}: {response}")
        channel = grpc.insecure_channel(f'{client.host}:{client.port}')
        stub = HotStuffClientStub(channel)
        stub.reply(Response(message=f"{node.client_req_id}: {response}", sender_id=self.id))


    def maybe_get_node(self, node_id: str, sender_id: str):
        try:
            return self.tree.get_node(node_id)
        except KeyError:
            sender = None
            for replica_config in self.global_config.replica_configs:
                if replica_config.id == sender_id:
                    sender = replica_config
            self.log.info(f"Fetching node {node_id} from {sender_id}")
            channel = grpc.insecure_channel(f'{sender.host}:1{sender.port}')
            stub = NodeSenderStub(channel)
            nodes = []
            node_response = stub.send_node(NodeRequest(node_id=node_id))
            nodes.append(node_from_bytes(node_response.node))
            while nodes[-1].parent_id not in self.tree.nodes:
                node_response = stub.send_node(NodeRequest(node_id=nodes[-1].parent_id))
                nodes.append(node_from_bytes(node_response.node))
            for node in reversed(nodes):
                self.tree.add_node(node)
                self.view_number = max(self.view_number, node.view_number)
            return self.tree.get_node(node_id)

    def update_qc_high(self, received_qc: QC, sender_id: str):
        """Update the highest QC seen so far. Also set it as b_leaf.

        Does nothing if the height of the QC is less than the height of the current highest QC.
        """
        received_qc_node = self.maybe_get_node(received_qc.node_id, sender_id)
        my_qc_high_node = self.tree.get_node(self.qc_high.node_id)
        if received_qc_node.height > my_qc_high_node.height:
            self.log.info(
                f"Updating qc_high from {my_qc_high_node} to {received_qc_node} and setting it as leaf")
            self.qc_high = received_qc
            self.log.info(f"Changing inside update qc leaf node from {self.leaf_node} to {received_qc_node}")
            self.maybe_get_node(received_qc_node.id, sender_id)
            self.maybe_get_node(self.leaf_node.id, sender_id)
            if not self.tree.is_ancestor(received_qc_node.id, self.leaf_node.id):
                self.leaf_node = received_qc_node
        else:
            self.log.debug(
                f"Skipping update of qc_high from {my_qc_high_node} to {received_qc_node}")

    def commit(self, node: Node):
        """Commit and execute a node if it's higher than b_exec.

        Also do the same for all the ancestors of the node.
        """
        if self.executed_node.height < node.height:
            self.log.info(f"Commiting {node}")
            self.commit(self.tree.get_node(node.parent_id))
            self.execute(node)
        else:
            self.log.debug(f"Skipping commit of {node}")

    def update(self, node: Node, sender_id: str):
        """Update the replica state.

        This is executed when we receive a proposal for a node.
        I think this should also add the node to the tree if its not present already,
        but not sure yet.
        """
        self.log.debug(f"Updating {node}")
        # TODO: Check if lock is required here
        node_jp_dp = self.maybe_get_node(node.justify.node_id, sender_id)  # b'' in paper
        node_jgp_p = self.maybe_get_node(
            node_jp_dp.justify.node_id, sender_id)  # b' in paper
        node_jggp_b = self.maybe_get_node(
            node_jgp_p.justify.node_id, sender_id)  # b in paper

        self.log.info(
            f"Justify ancestors: {node} -> {node_jp_dp} -> {node_jgp_p} -> {node_jggp_b}")

        self.update_qc_high(node.justify, sender_id)
        if node_jgp_p.height > self.locked_node.height:
            self.log.info(f"Locking {node_jgp_p} over {self.locked_node}")
            # node_jgp enters commit phase
            self.locked_node = node_jgp_p
        else:
            self.log.debug(f"Skipping lock of {node_jgp_p} over {self.locked_node}")

        if node_jp_dp.parent_id == node_jgp_p.id and node_jgp_p.parent_id == node_jggp_b.id:
            # node_jggp can be executed now
            self.commit(node_jggp_b)
            self.log.debug(
                f"Updating executed_node from {self.executed_node} to {node_jggp_b}")
            self.executed_node = node_jggp_b
    
    def on_new_view_sync(self):
        with self.lock:
            self.view_number += 1
        self.log.debug(f"Sending NEW_VIEW to {self.get_leader_id()}")
        new_view_req = NewViewRequest(sender_id=self.id, node=None, qc=self.qc_high.to_bytes())
        leader_session = self.get_session(self.get_leader_id())
        leader_session.stub.NewView(new_view_req)

    def on_beat(self, request : ClientCommandRequest):
        self.log.error(f"Received command from client {request.data.sender_id}, {self.is_leader()}")
        if request is None:
            return
        
        clientIdStr = request.data.sender_id
        send_proposal = False
        with self.lock:
            if self.is_leader() and self.clientMap[clientIdStr].updateReq(request.data.req_id):
                self.log.info(f"Processing command from client: {request.data.cmd}")
                self.log.info(' '.join([str(k) for k in [request.data.cmd, self.leaf_node.id, request.data.sender_id,
                        self.qc_high, self.view_number, request.data.req_id]]))
                try:
                    new_node = self.tree.create_node(
                        request.data.cmd, self.leaf_node.id, request.data.sender_id,
                            self.qc_high, self.view_number, request.data.req_id)
                except KeyError:
                    self.log.error(f"Parent node {self.leaf_node.id} not in the tree")
                    return
                
                self.log.debug(f"New node's jusitfy node id {new_node.justify.node_id}")

                self.log.info(f"Proposing {new_node} and setting it as leaf")
                self.log.info(f"Changing leaf node from {self.leaf_node} to {new_node}")
                self.leaf_node = new_node
                send_proposal = True
                
        if send_proposal:
            # This should be outside lock as it will call Propose on the
            # leader and will lead to a deadlock otherwise.
            for replica in self.replica_sessions:
                self.log.debug(f"ABOUT TO SEND cmd {request.data.cmd} to replica {replica.config.id}")
                replica.stub.Propose(ProposeRequest(
                    sender_id=self.id, node=new_node.to_bytes()))
                self.log.debug(f"SENT cmd {request.data.cmd} to replica {replica.config.id}")
        

    #############################
    # Hot stuff protocol endpoints start here
    #############################

    def ClientCommand(self, request : ClientCommandRequest, context):
        data_bytes = request.data.SerializeToString()
        clientIdStr = request.data.sender_id
        clientPkStr = self.clientMap[clientIdStr].clientPk
        validSig, _ = verifySigs(data_bytes, [request.sig], [parsePK(clientPkStr)])
        if validSig:
            self.on_beat(request)
            # self.pacemaker.on_client_request(request)
        self.log.debug("CLIENT SHOULD RETURN")
        return EmptyResponse()

    def Propose(self, request, context):
        to_vote = False
        new_node = node_from_bytes(request.node)
        self.log.info(f"Received proposal {new_node}")
        self.pacemaker.new_view_event.set()
        with self.lock:
            self.clientMap[new_node.client_id].updateReq(new_node.client_req_id)
            self.pacemaker.dedup_req(new_node.client_id, new_node.client_req_id)
            self.log.debug(f"Received proposal {new_node} from leader {request.sender_id}")
            self.tree.add_node(new_node)
            always_true = new_node.height > self.vheight  # Always true for the happy path
            happy_path = self.tree.is_ancestor(
                self.locked_node.id, new_node.id)
            sad_path = self.maybe_get_node(
                new_node.justify.node_id, request.sender_id).height > self.locked_node.height
            self.view_number = max(self.view_number, new_node.view_number + 1)
            self.log.info(
                f"Proposal: {always_true} ({new_node.height} > {self.vheight}) and ({happy_path} or {sad_path})")
            if always_true and (happy_path or sad_path):
                self.vheight = new_node.height
                self.log.info(f"Changing inside propose leaf node from {self.leaf_node} to {new_node}")
                self.leaf_node = new_node
                # Call to the vote function must happen outside the lock
                # otherwise this will cause a deadlock in leader
                to_vote = True

            self.update(new_node, request.sender_id)
            self.view_number += 1

        # TODO: Note sure if this is necessary but reflects a change in central control
        if self.is_leader():
            self.pacemaker.central_control_event.set()

        if to_vote:
            self.log.info(f"Voting for {new_node}")
            leader_session = self.get_session(self.get_leader_id())
            node_bytes = new_node.to_bytes()
            sig = bytes(partialSign(self.secret_key, node_bytes))
            vote = VoteRequest(sender_id=self.id, node=node_bytes, partial_sig=sig)
            leader_session.stub.Vote(vote)
            self.log.debug(f"SENT vote for {new_node.id} to leader {self.get_leader_id()}")
        
        # # This might be incorrect, do we only update View when we receive a valid
        # # proposal or do we update either way.
        # with self.lock:
        #     if not self.is_leader():
        #         self.view_number += 1
        
        return EmptyResponse()

    def Vote(self, request, context):
        node = node_from_bytes(request.node)
        self.log.info(f"Received vote for {node} from {request.sender_id} with signature {request.partial_sig[:5]}")
        with self.lock:
            self.tree.add_node(node)
            numVotes = self.add_vote(node.id, request)
            if numVotes >= self.num_replica - self.F:
                # Put in logic for checking
                verified, aggSig, pkids =  self.check_votes(node.id)
                if verified:
                    self.log.debug(f"Got enough votes for {node}")
                    newQC = QC(node.id, node.view_number, bytes(aggSig), pkids)
                    self.update_qc_high(newQC, self.id)
                else:
                    self.log.debug(f"Received {numVotes} but votes weren't valid")
            else:
                self.log.debug(f"Received {numVotes} : < {self.num_replica - self.F}")

        return EmptyResponse()
    
    def NewView(self, request: NewViewRequest, context):
        self.log.info(f"Received NEW_VIEW from {request.sender_id}")
        with self.lock:
            self.update_qc_high(qc_from_bytes(request.qc), request.sender_id)
        return EmptyResponse()
