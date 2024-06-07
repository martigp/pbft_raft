"""Application executor module."""

class Executor:

    def execute(self, cmd:str) -> str:
        """Execute a command."""
        raise NotImplementedError
    
class KVStore(Executor):
    """Key-value store class."""

    def __init__(self):
        """Initialize the key-value store."""
        self.store = {}

    def set(self, key:str, value:str):
        """Set a key-value pair."""
        self.store[key] = value

    def get(self, key:str):
        """Get the value for a key."""
        return self.store.get(key, "Key not found")

    def delete(self, key:str)->str:
        """Delete a key-value pair."""
        return self.store.pop(key, "Key not found")

    def execute(self, cmd:str) -> str:
        """Execute a command."""
        cmd = cmd.split()
        if cmd[0] == "set":
            if len(cmd) != 3:
                return "Set command requires 2 arguments"
            self.set(cmd[1], cmd[2])
            return "OK"
        elif cmd[0] == "get":
            if len(cmd) != 2:
                return "Get command requires 1 arguments"
            return self.get(cmd[1])
        elif cmd[0] == "delete":
            if len(cmd) != 2:
                return "Delete command requires 1 arguments"
            return self.delete(cmd[1])
        else:
            return "Invalid command"
        