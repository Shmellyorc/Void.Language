"""Small portable stdio client used by the executable demo and process tests.

Reader threads only belong to this test/client harness, never the compiler server.
"""
import json
import queue
import subprocess
import threading
import time


def frame(message):
    body = json.dumps(message, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    return b"Content-Length: " + str(len(body)).encode("ascii") + b"\r\n\r\n" + body


class Client:
    def __init__(self, executable, entry):
        self.process = subprocess.Popen([str(executable), "--entry", str(entry)],
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.messages = queue.Queue()
        self.logs = bytearray()
        self.serial = 0
        self.reader = threading.Thread(target=self._read, daemon=True)
        self.logger = threading.Thread(target=self._logs, daemon=True)
        self.reader.start()
        self.logger.start()

    def _read(self):
        try:
            stream = self.process.stdout
            while True:
                line = stream.readline(8193)
                if not line:
                    self.messages.put(EOFError("Server stdout closed"))
                    return
                assert line.startswith(b"Content-Length: ") and line.endswith(b"\r\n"), line
                length = int(line[16:-2])
                assert 0 <= length <= 8 * 1024 * 1024, length
                assert stream.read(2) == b"\r\n", "Unframed stdout/header corruption"
                body = stream.read(length)
                assert len(body) == length, "Truncated server output"
                message = json.loads(body.decode("utf-8"))
                assert message["jsonrpc"] == "2.0", message
                self.messages.put(message)
        except BaseException as error:
            self.messages.put(error)

    def _logs(self):
        while True:
            chunk = self.process.stderr.read(8192)
            if not chunk:
                return
            self.logs.extend(chunk)

    def write(self, data, fragmented=False):
        if fragmented:
            sizes = (1, 2, 7, 3, 11)
            start = 0
            while start < len(data):
                size = sizes[start % len(sizes)]
                self.process.stdin.write(data[start:start + size])
                self.process.stdin.flush()
                start += size
                time.sleep(0.001)
        else:
            self.process.stdin.write(data)
            self.process.stdin.flush()

    def notify(self, method, params=None):
        message = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            message["params"] = params
        self.write(frame(message))

    def receive(self):
        item = self.messages.get(timeout=30)
        if isinstance(item, BaseException):
            raise item
        return item

    def request(self, method, params=None, request_id=None):
        if request_id is None:
            self.serial += 1
            request_id = self.serial
        message = {"jsonrpc": "2.0", "id": request_id, "method": method}
        if params is not None:
            message["params"] = params
        self.write(frame(message))
        return self.response(request_id)

    def response(self, request_id):
        notifications = []
        while True:
            message = self.receive()
            if "id" in message:
                assert message["id"] == request_id and type(message["id"]) is type(request_id), message
                assert ("result" in message) != ("error" in message), message
                return message, notifications
            assert message.get("method") == "textDocument/publishDiagnostics", message
            notifications.append(message["params"])

    def sync(self):
        response, notifications = self.request("void/barrier")
        assert response["error"]["code"] == -32601, response
        return notifications

    def initialize(self):
        response, notifications = self.request("initialize", {"processId": None, "rootUri": None,
            "capabilities": {"general": {"positionEncodings": ["utf-16"]}, "textDocument": {
                "publishDiagnostics": {"relatedInformation": True, "versionSupport": True}}}}, "initialize")
        assert not notifications
        caps = response["result"]["capabilities"]
        assert {key: caps[key] for key in ["positionEncoding", "textDocumentSync", "definitionProvider", "referencesProvider"]} == {"positionEncoding": "utf-16", "textDocumentSync": {"openClose": True, "change": 1, "save": {"includeText": False}},
                        "definitionProvider": True, "referencesProvider": True}, caps
        self.notify("initialized", {})
        return self.sync()

    def finish(self, expected=0):
        if self.process.stdin and not self.process.stdin.closed:
            self.process.stdin.close()
        code = self.process.wait(timeout=30)
        self.reader.join(timeout=5)
        self.logger.join(timeout=5)
        assert code == expected, (code, self.logs.decode("utf-8", errors="replace"))
        assert not self.reader.is_alive() and not self.logger.is_alive(), "Leaked client reader thread"
        log = self.logs.decode("utf-8", errors="replace")
        assert not any(x in log for x in ["AddressSanitizer", "LeakSanitizer", "runtime error:", "PLEASE submit a bug report"]), log
        # Every response/notification must be consumed; only clean EOF remains.
        while not self.messages.empty():
            item = self.messages.get_nowait()
            assert isinstance(item, EOFError), item
        return log

    def shutdown(self):
        response, notifications = self.request("shutdown", request_id="shutdown")
        assert response["result"] is None and not notifications, (response, notifications)
        self.notify("exit")
        self.finish()

    def __enter__(self):
        return self

    def __exit__(self, kind, value, traceback):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=10)
        for stream in [self.process.stdin, self.process.stdout, self.process.stderr]:
            if stream and not stream.closed:
                stream.close()
        self.reader.join(timeout=5)
        self.logger.join(timeout=5)
