ROOT_ID = 'root'


class QC:
    """Quorum Certificate.
    
    Contains the quorum certificate of a node.
    Also contains information about the node. FOr now only node id is stored. Most likely this should be enough

    TODO: Implement the actual quorum certificate.
    """
    def __init__(self, node_id: str):
        self.node_id = node_id


class Node:
    """Node in the tree.
    
    Nodes are immutabe. They should only be created and modified in the tree class.
    """
    def __init__(self, id: str, height: int, parent_id: str, cmd: str, qc: QC = None):
        self.id = id
        self.height = height
        self.parent_id = parent_id
        self.children_ids = []
        self.cmd = cmd
        self.justify = qc

# Tree structure
# The tree is append only. No other operations are supported.
class Tree:
    """Append-only Tree structure.

    The tree is append only. No other operations are supported.
    Only this class is allowed to create and modify nodes.
    It is local to the replica and is not shared with other replicas.
    TODO: figure out when non-leader replicas should create nodes.
    """
    def __init__(self):
        root_node = Node(ROOT_ID, 0, '', '')
        root_node.justify = QC(ROOT_ID)
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
    def create_node(self, cmd: str, parent_id: str, qc: QC) -> Node:
        """Create and add a new node to the tree and return it.
        
        qc becomes the justify of the new node.
        """
        new_id = 'node'+str(len(self.nodes))
        parent = self.get_node(parent_id)
        new_node = Node(new_id, parent.height+1, parent_id, cmd, qc)
        self.nodes[new_id] = new_node
        parent.children_ids.append(new_id)
        return new_node
