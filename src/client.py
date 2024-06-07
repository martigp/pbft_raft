import logging
import logging.config
import time
import argparse
from concurrent import futures
from threading import Thread, Lock
from typing import List
import grpc
import random

from common import get_client_config, get_replica_sessions, ClientConfig, ReplicaSession
from proto.HotStuff_pb2 import ClientCommandRequest
from proto import Client_pb2, Client_pb2_grpc
from crypto import partialSign, parseSK

logging.config.fileConfig('logging.ini', disable_existing_loggers=True)

parser = argparse.ArgumentParser()
parser.add_argument('--runner', type=str, default='async', help='sync, async or random_kv')


class ClientServicer(Client_pb2_grpc.HotStuffClientServicer):
    """
    Implements the HotStuffClientServicer interface defined in the proto file.
    """

    def __init__(self, config:ClientConfig, F:int, replica_sessions:List[ReplicaSession],  *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.config = config
        self.log = logging.getLogger(self.config.id)
        self.F = F
        self.responses = {} # Dict from request id to replica ids to reponses
        self.agreed_responses = {} # Dict from request id to agreed response
        self.replica_sessions = replica_sessions
        self.curr_req_id = 0
        self.lock = Lock()

    def reply(self, request, _):
        """ Handles execution replies from replicas. """
        self.log.debug("Received reply (%s): %s",
                      request.sender_id, request.message)
        req_id, response = request.message.split(":")
        with self.lock:
            if req_id not in self.responses:
                self.responses[req_id] = {}
            self.responses[req_id][request.sender_id] = response
            responses = self.responses[req_id].values()
            # Check if there are F equal responses
            responses = sorted(responses)
            prev_response = None
            agreements = 0
            for response in responses:
                if response == prev_response:
                    agreements += 1
                else:
                    agreements = 1
                prev_response = response
                if agreements > self.F:
                    if req_id not in self.agreed_responses:
                        self.agreed_responses[req_id] = response
                        self.log.info(f"Received response for request id {req_id}: {response}")
                    break

        return Client_pb2.EmptyResponse()
    
    def async_execute(self, cmd:str)->str:
        self.log.debug(f"Sending {cmd}")
        with self.lock:
            curr_req_id = self.curr_req_id
            self.curr_req_id+=1
        for replica in self.replica_sessions:
            data = ClientCommandRequest.Data(sender_id=self.config.id, cmd=cmd, req_id = curr_req_id)
            data_bytes = data.SerializeToString()
            sig = partialSign(parseSK(self.config.secret_key), data_bytes)
            replica.stub.ClientCommand(ClientCommandRequest(data=data,sig=bytes(sig)))
        self.log.debug(f"Sent command {cmd}")
        return f"{curr_req_id}"

    
    def execute(self, cmd:str)->str:
        req_id = self.async_execute(cmd)
        for _ in range(50):
            with self.lock:
                self.log.debug(f"Agreed Responses: {self.agreed_responses}")
                if req_id in self.agreed_responses:
                    return self.agreed_responses[req_id]
            time.sleep(0.5)
        return "Failed"
                



def serve(client_servicer: ClientServicer, config: ClientConfig):
    """Starts the server to listen for incoming messages from replicas."""
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    Client_pb2_grpc.add_HotStuffClientServicer_to_server(
        client_servicer, server)
    server.add_insecure_port("[::]:" + str(config.port))
    # log.info("Listening on port: %s, client id: %s", config.port, config.id)
    server.start()
    server.wait_for_termination()


def main(args):
    """Main function for the client."""
    # Read configs
    config, global_config = get_client_config()

    # Start server
    replica_sessions = get_replica_sessions(global_config)
    client_servicer = ClientServicer(config, global_config.F, replica_sessions)
    Thread(target=serve, args=(client_servicer, config)).start()

    # Establish sessions with replicas

    # Send commands to replicas
    # This is the entry point for the protocol
    if args.runner == 'random_kv':
        while True:
            time.sleep(4)
            client_servicer.log.info(f"Sending commands to replicas")
            response = client_servicer.async_execute(f"SET {random.randint(0, 100)} {random.randint(0, 100)}")
            client_servicer.log.info(f"Response: {response}")
    elif args.runner == 'sync':
        while True:
            response = client_servicer.execute(input('Enter command: '))
            client_servicer.log.info(f"Response: {response}")
    elif args.runner == 'async':
        while True:
            response = client_servicer.async_execute(input('Enter command: '))
            client_servicer.log.info(f"Request id: {response}")
    else:
        raise ValueError(f'Invalid runner: {args.runner}')

        # Multithread receiving responses?


if __name__ == '__main__':
    main(parser.parse_args())
