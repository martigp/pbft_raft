import grpc
from proto.HotStuff_pb2 import EchoResponse
from proto.HotStuff_pb2_grpc import HotStuffReplicaServicer, add_HotStuffReplicaServicer_to_server
from concurrent import futures
from common import get_replica_config, get_replicas, ReplicaConfig


class HotStuffReplica(HotStuffReplicaServicer):
    def Echo(self, request, context):
        print("Received: '''"+request.msg+"'''")
        return EchoResponse(msg="Echoed: '''" + request.msg + "'''")


def serve(config: ReplicaConfig):
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    add_HotStuffReplicaServicer_to_server(HotStuffReplica(), server)
    server.add_insecure_port('[::]:'+str(config.port))

    print("Listining on port: "+str(config.port)+", replica id: " +
          str(config.id)+", public key: "+config.public_key)
    server.start()
    server.wait_for_termination()


if __name__ == '__main__':
    config, global_config = get_replica_config()

    replicas = get_replicas(global_config)

    serve(config)
