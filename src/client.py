import logging
import logging.config
from concurrent import futures
from threading import Thread
import grpc

from common import get_client_config, get_replica_sessions, ClientConfig
from proto.HotStuff_pb2 import BeatRequest
from proto import Client_pb2, Client_pb2_grpc
from crypto import partialSign, parseSK

logging.config.fileConfig('logging.ini', disable_existing_loggers=True)


class ClientServicer(Client_pb2_grpc.HotStuffClientServicer):
    """
    Implements the HotStuffClientServicer interface defined in the proto file.
    """

    def __init__(self, id, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.id = id
        self.log = logging.getLogger(self.id)

    def reply(self, request, _):
        """ Handles execution replies from replicas. """
        self.log.info("Received reply (%s): %s",
                      request.sender_id, request.message)
        return Client_pb2.EmptyResponse()


def serve(client_servicer: ClientServicer, config: ClientConfig):
    """Starts the server to listen for incoming messages from replicas."""
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    Client_pb2_grpc.add_HotStuffClientServicer_to_server(
        client_servicer, server)
    server.add_insecure_port("[::]:" + str(config.port))
    # log.info("Listening on port: %s, client id: %s", config.port, config.id)
    server.start()
    server.wait_for_termination()


def main():
    """Main function for the client."""
    # Read configs
    config, global_config = get_client_config()

    # Start server
    client_servicer = ClientServicer(config.id)
    Thread(target=serve, args=(client_servicer, config)).start()

    # Establish sessions with replicas
    replica_sessions = get_replica_sessions(global_config)

    # Send commands to replicas
    # This is the entry point for the protocol
    i = 0
    while True:
        cmd = input('Enter command: ')
        # We should be signing this as a sender req.SerializeToString()
        for replica in replica_sessions:
            data = BeatRequest.Data(sender_id=config.id, cmd=cmd, req_id = i)
            data_bytes = data.SerializeToString()
            sig = partialSign(parseSK(config.secret_key), data_bytes)
            replica.stub.Beat(BeatRequest(data=data,sig=bytes(sig)))
        i+=1

        # Multithread receiving responses?


if __name__ == '__main__':
    main()
