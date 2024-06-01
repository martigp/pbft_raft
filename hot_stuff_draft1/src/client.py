import logging
import logging.config

from common import get_client_config, get_replica_sessions
from proto.HotStuff_pb2 import BeatRequest, EchoRequest

logging.config.fileConfig('logging.ini', disable_existing_loggers=True)
log = logging.getLogger(__name__)

if __name__ == '__main__':
    # Read configs
    config, global_config = get_client_config()

    # Establish sessions with replicas
    replica_sessions = get_replica_sessions(global_config)

    # Send commands to replicas
    # This is the entry point for the protocol
    i = 0
    while True:
        cmd = input('Enter command: ')
        # We should be signing this as a sender req.SerializeToString()
        for replica in replica_sessions:
            replica.stub.Beat(BeatRequest(sender_id=config.id, cmd=cmd, req_id = i))

        i+=1

        # Multithread receiving responses?
