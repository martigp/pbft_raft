import json
import logging
import os
from typing import List, Tuple
import base64
import requests
from executor import Executor, KVStore
import grpc
from proto.HotStuff_pb2_grpc import HotStuffReplicaStub

logging.config.fileConfig('logging.ini', disable_existing_loggers=False)
log = logging.getLogger(__name__)

class ClientConfig:
    """Contains the configuration of a client."""

    def __init__(self, id: str, host: str, port: int, public_key: str, secret_key : str):
        self.id = str(id)
        self.host = host
        self.port = str(port)
        self.public_key = public_key
        self.secret_key = secret_key


class ReplicaConfig:
    """Contains the configuration of a replica."""

    def __init__(self, id: str, host: str, port: int, public_key: str, secret_key: str, root_qc_sig : str):
        self.id = str(id)
        self.host = str(host)
        self.port = str(port)
        self.public_key = public_key
        self.secret_key = secret_key
        self.root_qc_sig = root_qc_sig


class GlobalConfig:
    """Contains the configuration of all clients and replicas."""

    def __init__(self, clients: List[ClientConfig], replicas: List[ReplicaConfig], F: int = 1):
        self.client_configs = clients
        self.replica_configs = replicas
        self.F = F

class ErrorIgnoringStub:
    def __init__(self, config: ReplicaConfig):
        self.config = config
        self.stub = HotStuffReplicaStub(
                grpc.insecure_channel(self.config.host+':'+str(self.config.port)))

    @staticmethod
    def ignore_error(func, name, id):
        def wrapper(*args, **kwargs):
            try:
                return func(*args, **kwargs)
            except Exception as e:
                log.error(f"Failed to call {name} on {id}")
                return None
        return wrapper

    def __getattr__(self, name):
        attr = getattr(self.stub, name)
        if callable(attr):
            return ErrorIgnoringStub.ignore_error(
                attr, name, self.config.id)
        return attr


class ReplicaSession:
    """Class used to communicate with replicas."""

    def __init__(self, config: ReplicaConfig):
        self.config = config
        self.stub = ErrorIgnoringStub(config)


def get_global_config(file_path: str = '../configs.json') -> GlobalConfig:
    """Reads the global(both clients and replicas) configuration from a file."""
    # TODO: allow custom file name
    log.info(f'Reading config from {file_path}')
    with open(file_path) as f:
        config = json.load(f)
    clients = [ClientConfig(**client) for client in config['clients']]
    replicas = [ReplicaConfig(**replica) for replica in config['replicas']]
    return GlobalConfig(clients, replicas, config['F'])


def get_replica_config() -> Tuple[ReplicaConfig, GlobalConfig]:
    """Gets the conifugration of self as well as the global config containing all clients and replicas."""
    id = str(os.getenv('REPLICA_ID'))
    config = get_global_config()
    for replica in config.replica_configs:
        if replica.id == id:
            return replica, config
    raise ValueError(f'Replica {id} not found in config')


def get_client_config() -> Tuple[ClientConfig, GlobalConfig]:
    """Gets the conifugration of self as well as the global config containing all clients and replicas."""
    id = str(os.getenv('CLIENT_ID'))
    config = get_global_config()
    for client in config.client_configs:
        if client.id == id:
            return client, config
    raise ValueError(f'Client {id} not found in config')


def get_replica_sessions(global_config: GlobalConfig) -> List[ReplicaSession]:
    """Establishes sessions with all replicas."""
    return [ReplicaSession(replica_config) for replica_config in global_config.replica_configs]

def get_executor()->Executor:
    executor = os.getenv('EXECUTOR', 'KVStore')
    if executor == 'KVStore':
        return KVStore()
    else:
        raise ValueError(f'Executor {executor} not found')
