import time

from common import get_client_config, get_replica_sessions
from proto.HotStuff_pb2 import EchoRequest, BeatRequest

if __name__ == '__main__':
    # Read configs
    config, global_config = get_client_config()

    # Establish sessions with replicas
    replica_sessions = get_replica_sessions(global_config)

    # Send messages to replicas
    # For now we just just an echo message to replicas every 2 seconds
    # This is just for testing and not relevant to the protocol
    while False:
        for replica in replica_sessions:
            time.sleep(2)
            response = replica.stub.Echo(EchoRequest(sender_id=int(
                config.id), msg='CLIENT: Hello from client '+str(config.id)))
            print(response.msg)

    # Send commands to replicas
    # This is the entry point for the protocol
    while True:
        cmd = input('Enter command: ')
        for replica in replica_sessions:
            replica.stub.Beat(BeatRequest(
                sender_id=config.id, cmd='CLIENT: '+cmd))
            

