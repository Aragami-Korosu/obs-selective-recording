# SPDX-License-Identifier: GPL-2.0-or-later
"""Minimal obs-websocket v5 client used by the integration tests."""

import itertools
import json
import time

import websocket


class RequestError(RuntimeError):
    pass


class Client:
    def __init__(self, url="ws://127.0.0.1:4466", timeout=60):
        deadline = time.time() + timeout
        last_error = None
        while time.time() < deadline:
            try:
                self.ws = websocket.create_connection(url, timeout=30)
                break
            except OSError as e:
                last_error = e
                time.sleep(1)
        else:
            raise RuntimeError(f"could not connect to {url}: {last_error}")

        hello = json.loads(self.ws.recv())
        assert hello["op"] == 0, hello
        self.ws.send(json.dumps({"op": 1, "d": {"rpcVersion": 1, "eventSubscriptions": 0}}))
        identified = json.loads(self.ws.recv())
        assert identified["op"] == 2, identified
        self.ids = itertools.count(1)

    def call(self, request_type, data=None, allow_fail=False):
        request_id = str(next(self.ids))
        self.ws.send(json.dumps({"op": 6, "d": {
            "requestType": request_type, "requestId": request_id, "requestData": data or {}}}))
        while True:
            message = json.loads(self.ws.recv())
            if message["op"] == 7 and message["d"]["requestId"] == request_id:
                break
        status = message["d"]["requestStatus"]
        if not status["result"] and not allow_fail:
            raise RequestError(f"{request_type} failed: {status}")
        return message["d"].get("responseData", {})

    def vendor(self, request_type, data=None, vendor="selective-recording"):
        response = self.call("CallVendorRequest", {
            "vendorName": vendor, "requestType": request_type, "requestData": data or {}})
        return response.get("responseData", {})

    def close(self):
        self.ws.close()
