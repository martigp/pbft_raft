import logging
import logging.config
import time

from common import get_client_config, get_replica_sessions
from proto.HotStuff_pb2 import BeatRequest, EchoRequest

logging.config.fileConfig('logging.ini', disable_existing_loggers=True)
log = logging.getLogger(__name__)

if __name__ == '__main__':
    log.debug("Logging set to DEBUG level")
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

    # import http.server
    # import socketserver
    # import subprocess
    # import json

    # PORT = 8080

    # class CommandHandler(http.server.SimpleHTTPRequestHandler):
    #     def do_POST(self):
    #         # Read the length of the data
    #         content_length = int(self.headers['Content-Length'])
    #         # Read the data
    #         post_data = self.rfile.read(content_length)
            
    #         # Parse the command from the data
    #         try:
    #             data = json.loads(post_data)
    #             cmd = data.get('command')
    #         except json.JSONDecodeError:
    #             self.send_response(400)
    #             self.end_headers()
    #             self.wfile.write(b"Invalid JSON")
    #             return

    #         # Execute the command
    #         if cmd:
    #             for replica in replica_sessions:
    #                 replica.stub.Beat(BeatRequest(sender_id=config.id, cmd=cmd, req_id = i))
    #             i += 1
    #         else:
    #             self.send_response(400)
    #             self.end_headers()
    #             self.wfile.write(b"No command provided")

    # with socketserver.TCPServer(("", PORT), CommandHandler) as httpd:
    #     print("Serving on port", PORT)
    #     httpd.serve_forever()
