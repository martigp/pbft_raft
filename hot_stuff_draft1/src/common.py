from typing import List, Tuple
import json
import os
import grpc
from proto.HotStuff_pb2_grpc import HotStuffReplicaStub


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

class Replica:
    def __init__(self, config: ReplicaConfig):
        self.config = config
        self.stub = HotStuffReplicaStub(grpc.insecure_channel(config.host+':'+str(config.port)))


class GlobalConfig:
    def __init__(self, clients: List[ClientConfig], replicas: List[ReplicaConfig]):
        self.client_configs = clients
        self.replica_configs = replicas


def get_global_config(file_path: str = '../configs.json') -> GlobalConfig:
    with open(file_path) as f:
        config = json.load(f)
    clients = [ClientConfig(**client) for client in config['clients']]
    replicas = [ReplicaConfig(**replica) for replica in config['replicas']]
    return GlobalConfig(clients, replicas)


def get_replica_config() -> Tuple[ReplicaConfig, GlobalConfig]:
    id = os.getenv('REPLICA_ID')
    config = get_global_config()
    for replica in config.replica_configs:
        if str(replica.id) == str(id):
            return replica, config
    raise ValueError(f'Replica {id} not found in config')


def get_client_config() -> Tuple[ClientConfig, GlobalConfig]:
    id = os.getenv('CLIENT_ID')
    config = get_global_config()
    for client in config.client_configs:
        if str(client.id) == str(id):
            return client, config
    raise ValueError(f'Client {id} not found in config')


def get_replicas(global_config: GlobalConfig) -> List[HotStuffReplicaStub]:
    return [Replica(replica_config) for replica_config in global_config.replica_configs]
