# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: proto/HotStuff.proto
# Protobuf Python Version: 5.26.1
"""Generated protocol buffer code."""
from google.protobuf import descriptor as _descriptor
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf import symbol_database as _symbol_database
from google.protobuf.internal import builder as _builder
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(b'\n\x14proto/HotStuff.proto\x12\x08HotStuff\"-\n\x0b\x45\x63hoRequest\x12\x11\n\tsender_id\x18\x01 \x01(\t\x12\x0b\n\x03msg\x18\x02 \x01(\t\"\x1b\n\x0c\x45\x63hoResponse\x12\x0b\n\x03msg\x18\x01 \x01(\t\"\x0f\n\rEmptyResponse\"|\n\x0b\x42\x65\x61tRequest\x12(\n\x04\x64\x61ta\x18\x01 \x01(\x0b\x32\x1a.HotStuff.BeatRequest.Data\x12\x0b\n\x03sig\x18\x02 \x01(\x0c\x1a\x36\n\x04\x44\x61ta\x12\x11\n\tsender_id\x18\x01 \x01(\t\x12\x0b\n\x03\x63md\x18\x02 \x01(\t\x12\x0e\n\x06req_id\x18\x03 \x01(\x04\"1\n\x0eProposeRequest\x12\x11\n\tsender_id\x18\x01 \x01(\t\x12\x0c\n\x04node\x18\x02 \x01(\x0c\"C\n\x0bVoteRequest\x12\x11\n\tsender_id\x18\x01 \x01(\t\x12\x0c\n\x04node\x18\x02 \x01(\x0c\x12\x13\n\x0bpartial_sig\x18\x03 \x01(\x0c\"I\n\x0eNewViewRequest\x12\x11\n\tsender_id\x18\x01 \x01(\t\x12\x0c\n\x04node\x18\x02 \x01(\x0c\x12\x0f\n\x02qc\x18\x03 \x01(\x0cH\x00\x88\x01\x01\x42\x05\n\x03_qc2\xbe\x02\n\x0fHotStuffReplica\x12\x37\n\x04\x45\x63ho\x12\x15.HotStuff.EchoRequest\x1a\x16.HotStuff.EchoResponse\"\x00\x12\x38\n\x04\x42\x65\x61t\x12\x15.HotStuff.BeatRequest\x1a\x17.HotStuff.EmptyResponse\"\x00\x12>\n\x07Propose\x12\x18.HotStuff.ProposeRequest\x1a\x17.HotStuff.EmptyResponse\"\x00\x12\x38\n\x04Vote\x12\x15.HotStuff.VoteRequest\x1a\x17.HotStuff.EmptyResponse\"\x00\x12>\n\x07NewView\x12\x18.HotStuff.NewViewRequest\x1a\x17.HotStuff.EmptyResponse\"\x00\x62\x06proto3')

_globals = globals()
_builder.BuildMessageAndEnumDescriptors(DESCRIPTOR, _globals)
_builder.BuildTopDescriptorsAndMessages(DESCRIPTOR, 'proto.HotStuff_pb2', _globals)
if not _descriptor._USE_C_DESCRIPTORS:
  DESCRIPTOR._loaded_options = None
  _globals['_ECHOREQUEST']._serialized_start=34
  _globals['_ECHOREQUEST']._serialized_end=79
  _globals['_ECHORESPONSE']._serialized_start=81
  _globals['_ECHORESPONSE']._serialized_end=108
  _globals['_EMPTYRESPONSE']._serialized_start=110
  _globals['_EMPTYRESPONSE']._serialized_end=125
  _globals['_BEATREQUEST']._serialized_start=127
  _globals['_BEATREQUEST']._serialized_end=251
  _globals['_BEATREQUEST_DATA']._serialized_start=197
  _globals['_BEATREQUEST_DATA']._serialized_end=251
  _globals['_PROPOSEREQUEST']._serialized_start=253
  _globals['_PROPOSEREQUEST']._serialized_end=302
  _globals['_VOTEREQUEST']._serialized_start=304
  _globals['_VOTEREQUEST']._serialized_end=371
  _globals['_NEWVIEWREQUEST']._serialized_start=373
  _globals['_NEWVIEWREQUEST']._serialized_end=446
  _globals['_HOTSTUFFREPLICA']._serialized_start=449
  _globals['_HOTSTUFFREPLICA']._serialized_end=767
# @@protoc_insertion_point(module_scope)