from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class EchoRequest(_message.Message):
    __slots__ = ("sender_id", "msg")
    SENDER_ID_FIELD_NUMBER: _ClassVar[int]
    MSG_FIELD_NUMBER: _ClassVar[int]
    sender_id: str
    msg: str
    def __init__(self, sender_id: _Optional[str] = ..., msg: _Optional[str] = ...) -> None: ...

class EchoResponse(_message.Message):
    __slots__ = ("msg",)
    MSG_FIELD_NUMBER: _ClassVar[int]
    msg: str
    def __init__(self, msg: _Optional[str] = ...) -> None: ...

class BeatRequest(_message.Message):
    __slots__ = ("sender_id", "cmd")
    SENDER_ID_FIELD_NUMBER: _ClassVar[int]
    CMD_FIELD_NUMBER: _ClassVar[int]
    sender_id: str
    cmd: str
    def __init__(self, sender_id: _Optional[str] = ..., cmd: _Optional[str] = ...) -> None: ...

class BeatResponse(_message.Message):
    __slots__ = ()
    def __init__(self) -> None: ...
