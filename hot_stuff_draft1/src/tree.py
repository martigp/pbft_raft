import logging
import pickle
from enum import Enum

log = logging.getLogger(__name__)

ROOT_ID = 'root_id'
ROOT_CMD = 'root_cmd'



class QC:
    """Quorum Certificate.

    Contains the quorum certificate of a node.
    Also contains information about the node. FOr now only node id is stored. Most likely this should be enough
    This class is serlialized and sent to other replicas.

    TODO: Implement the actual quorum certificate.
    """

    def __init__(self, node_id: str, viewNumber : int = None, aggSig : bytes = None, pkids: list[int] = None):
        # The node this QC corresponds to
        self.node_id = node_id
        # The combined signatures of the QC
        self.sig = aggSig
        # The pkids corresponding to the replicas who make up
        # the combined signature
        self.pkids = pkids
        # The view number of the node this QC corresponds to
        self.viewNumber = viewNumber


    def to_bytes(self) -> bytes:
        """Serialize the node to bytes."""
        return pickle.dumps(self)


def qc_from_bytes(data: bytes) -> QC:
    """Deserialize the node from bytes."""
    return pickle.loads(data)


class Node:
    """Node in the tree.

    Nodes are immutabe. They should only be created and modified in the tree class.
    This class is serlialized and sent to other replicas.
    """

    def __init__(self, id: str, height: int, parent_id: str, cmd: str, client_id : str = "Null", qc: QC = None, view_number = 0, client_req_id : int = None):
        self.id = id
        self.height = height
        self.parent_id = parent_id
        self.children_ids = []
        self.cmd = cmd
        self.client_id = client_id
        self.justify = qc
        self.view_number = view_number
        self.client_req_id = client_req_id

    def to_bytes(self) -> bytes:
        """Serialize the node to bytes."""
        return pickle.dumps(self)
    
    def __str__(self):
        return f'{self.id}:h{self.height}:{self.cmd}'


def node_from_bytes(data: bytes) -> Node:
    """Deserialize the node from bytes."""
    return pickle.loads(data)

# Tree structure
# The tree is append only. No other operations are supported.


class Tree:
    """Append-only Tree structure.

    The tree is append only. No other operations are supported.
    Only this class is allowed to create and modify nodes.
    It is local to the replica and is not shared with other replicas.
    TODO: figure out when non-leader replicas should create nodes.
    """

    def __init__(self, replica_id: str, root_qc_sig : str):
        self.replica_id = replica_id
        root_qc = QC(ROOT_ID, 0,
                               bytes.fromhex(root_qc_sig),
                               [0,1,2,3])
        root_node = Node(ROOT_ID, 0, ROOT_ID, ROOT_CMD, "null", root_qc)
        print(f"Root node's justify {root_node.justify.node_id}")
        self.nodes = {
            ROOT_ID: root_node
        }

    def get_node(self, node_id: str) -> Node:
        """Get the node with the specified id."""
        return self.nodes[node_id]

    def get_root_node(self) -> Node:
        """Get the root node of the tree."""
        return self.nodes[ROOT_ID]

    # Returns the id of the newly created node
    def create_node(self, cmd: str, parent_id: str, client_id : str, qc: QC, view_number : int, client_req_id : int) -> Node:
        """Create and add a new node to the tree and return it.

        qc becomes the justify of the new node.
        """
        new_id = f"{self.replica_id}_n{len(self.nodes)}"
        parent = self.get_node(parent_id)
        new_node = Node(new_id, parent.height+1, parent.id, cmd, client_id, qc, view_number, client_req_id)
        self.nodes[new_id] = new_node
        parent.children_ids.append(new_id)
        return new_node

    def add_node(self, node: Node):
        """Add the node to the tree. To be used when receiving new nodes from other replicas.

        This assumes that the parent of the node is already in the tree.
        TODO: handle the case when the parent is not in the tree.
        """
        if node.id in self.nodes:
            return
        self.nodes[node.id] = node
        print(f"Parent ID: {node.parent_id}")
        self.get_node(node.parent_id).children_ids.append(node.id)

    def is_ancestor(self, ancestor_id: str, descendant_id: str) -> bool:
        """Check if ancestor_id is an ancestor of descendant_id."""
        if ancestor_id == ROOT_ID:
            return True
        while descendant_id != ROOT_ID:
            if descendant_id == ancestor_id:
                return True
            descendant_id = self.get_node(descendant_id).parent_id
        return False

    def get_ancestry(self, node_id: str) ->str:
        """Return the ancestry of the node."""
        ret = "'''"
        while node_id != ROOT_ID:
            node = self.get_node(node_id)
            ret += str(Node) + ' -> '
            node_id = node.parent_id
        return ret + f"{ROOT_ID}'''"
    
    def get_justify_ancestry(self, node_id: str) -> str:
        """Return the ancestry of the justify of the node."""
        ret = "'''"
        while node_id != ROOT_ID:
            node = self.get_node(node_id)
            ret += str(Node) + ' -> '
            node_id = node.justify.node_id
        return ret + f"{ROOT_ID}'''"