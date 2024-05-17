from proto.HotStuff_pb2 import EchoRequest
from common import get_client_config, get_replicas
import time

if __name__ == '__main__':
    config, global_config = get_client_config()

    replicas = get_replicas(global_config)

    while True:
        for replica in replicas:
            time.sleep(2)
            response = replica.stub.Echo(EchoRequest(sender_id=int(
                config.id), msg='Hello from client '+str(config.id)))
            print(response.msg)
