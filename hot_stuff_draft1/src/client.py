import logging
import time

from common import get_client_config, get_replica_sessions
from proto.HotStuff_pb2 import BeatRequest, EchoRequest
from crypto import partialSign, parseSK

log = logging.getLogger(__name__)

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    log.debug("Logging set to DEBUG level")
    # Read configs
    config, global_config = get_client_config()

    # Establish sessions with replicas
    replica_sessions = get_replica_sessions(global_config)

    # Send an echo message to replicas every 2 seconds
    # This is just for testing and not relevant to the protocol
    send_echo = False
    while send_echo:
        for replica in replica_sessions:
            time.sleep(2)
            response = replica.stub.Echo(EchoRequest(
                sender_id=config.id, msg='Hello from client '+str(config.id)))
            log.info(
                f"Received response from replica {replica.config.id}: '''{response.msg}'''")

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
