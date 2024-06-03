import json
import logging
import base64
import requests


class CustomHttpHandler(logging.Handler):
    def __init__(self, url, user, password):
        logging.Handler.__init__(self)
        self.url = url
        creds = base64.b64encode(
            bytes(user + ":" + password, "utf-8")
            ).decode("utf-8")
        self.headers = {
            'Content-type': 'application/json',
            'Authorization': 'Basic ' + creds}

    def emit(self, record):
        log_entry = self.format(record)
        requests.post(
            self.url, headers=self.headers, data=log_entry, timeout=20)
