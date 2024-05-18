import time

from common import get_client_config, get_replica_sessions
from proto.HotStuff_pb2 import EchoRequest

if __name__ == '__main__':
    config, global_config = get_client_config()

    replica_sessions = get_replica_sessions(global_config)

    while True:
        for replica in replica_sessions:
            time.sleep(2)
            response = replica.stub.Echo(EchoRequest(sender_id=int(
                config.id), msg='CLIENT: Hello from client '+str(config.id)))
            print(response.msg)
