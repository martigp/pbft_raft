import time

from common import get_client_config, get_replica_sessions
from proto.HotStuff_pb2 import EchoRequest

if __name__ == '__main__':
    # Read configs
    config, global_config = get_client_config()

    # Establish sessions with replicas
    replica_sessions = get_replica_sessions(global_config)

    # Send messages to replicas
    # For now we just just an echo message to replicas every 2 seconds
    while True:
        for replica in replica_sessions:
            time.sleep(2)
            response = replica.stub.Echo(EchoRequest(sender_id=int(
                config.id), msg='CLIENT: Hello from client '+str(config.id)))
            print(response.msg)
