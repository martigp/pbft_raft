# Steps to run

```
python3.10 -m venv env
source env/bin/activate
pip install -r requirements.txt
```

Everytime you make any change to HotStuff.proto, you must run
```
python -m grpc_tools.protoc -I. --python_out=. --pyi_out=. --grpc_python_out=. src/proto/HotStuff.proto
```

To run replica
```
sh run_replica.sh 0 #<id>
```

To run client:
```
sh run_client.sh 0 #<id>
```