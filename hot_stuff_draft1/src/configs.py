from typing import List
import json

class ClientConfig:
    def __init__(self, id: str, public_key: str):
        self.id = id
        self.public_key = public_key

class ReplicaConfig:
    def __init__(self, id: str, host: str, port: int, public_key: str):
        self.id = id
        self.host = host
        self.port = port
        self.public_key = public_key

class Configs:
    def __init__(self, clients: List[ClientConfig], replicas: List[ReplicaConfig]):
        self.clients = clients
        self.replicas = replicas

def get_global_config(file_path: str = '../configs.json') -> Configs:
    with open(file_path) as f:
        config = json.load(f)
    clients = [ClientConfig(**client) for client in config['clients']]
    replicas = [ReplicaConfig(**replica) for replica in config['replicas']]
    return Configs(clients, replicas)

def get_replica_config(replica_id: str, config: Configs) -> ReplicaConfig:
    for replica in config.replicas:
        if str(replica.id) == str(replica_id):
            return replica
    raise ValueError(f'Replica {replica_id} not found in config')

def get_client_config(client_id: str, config: Configs) -> ClientConfig:
    for client in config.clients:
        if str(client.id) == str(client_id):
            return client
    raise ValueError(f'Client {client_id} not found in config')
