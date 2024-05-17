import grpc
from proto.HotStuff_pb2 import EchoRequest
from proto.HotStuff_pb2_grpc import HotStuffReplicaStub
import os
from configs import get_global_config, get_client_config
import time

replicas = []

if __name__ == '__main__':
    id = os.getenv('CLIENT_ID')
    global_config = get_global_config()
    config = get_client_config(id, global_config)

    for replica in global_config.replicas:
        channel = grpc.insecure_channel(
            replica.host+':'+str(replica.port))
        stub = HotStuffReplicaStub(channel)
        replicas.append(stub)

    while True:
        for replica in replicas:
            time.sleep(2)
            response = replica.Echo(
                EchoRequest(sender_id=int(config.id), msg='Hello from client '+str(config.id)))
            print(response.msg)
    
