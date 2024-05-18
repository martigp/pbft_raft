ROOT_ID = 'root'

# TODO: implement this
class QC:
    def __init__(self, node_id: str):
        self.node_id = node_id


# Node in the tree
# Nodes are immutable.
# They should only be created and modified in the tree class.
class Node:
    def __init__(self, id: str, height: int, parent: str, cmd: str, qc: QC = None):
        self.id = id
        self.height = height
        self.parent = parent
        self.children = []
        self.cmd = cmd
        self.justify = qc

# Tree structure
# The tree is append only. No other operations are supported.
class Tree:
    def __init__(self):
        root_node = Node(ROOT_ID, 0, '', '')
        root_node.justify = QC(ROOT_ID)
        self.nodes = {
            ROOT_ID: root_node
        }

    def get_node(self, node_id: str) -> Node:
        return self.nodes[node_id]

    def get_root(self) -> Node:
        return self.nodes[ROOT_ID]

    # Returns the id of the newly created node
    def add_node(self, cmd: str, parent_id: str, qc: QC) -> str:
        new_id = 'node'+str(len(self.nodes))
        parent = self.get_node(parent_id)
        new_node = Node(new_id, parent.height+1, parent_id, cmd, qc)
        self.nodes[new_id] = new_node
        parent.children.append(new_id)
        return new_id
