FROM python:3.10 as base
COPY requirements.txt .
RUN pip install -r requirements.txt
COPY . .
WORKDIR /src
RUN python -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. proto/HotStuff.proto

FROM base as replica
CMD ["python", "replica.py"]

FROM base as client
CMD ["python", "client.py"]

