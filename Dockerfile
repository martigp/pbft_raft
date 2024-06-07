FROM python:3.10 as base
# Gordon adds START #
WORKDIR /
RUN pip install --upgrade pip
RUN wget https://github.com/openobserve/openobserve/releases/download/v0.10.7-rc2/openobserve-v0.10.7-rc2-linux-arm64.tar.gz
RUN tar -xvf openobserve-v0.10.7-rc2-linux-arm64.tar.gz

COPY requirements.txt .
RUN pip install -r requirements.txt
COPY . .
WORKDIR /src
RUN python -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. proto/HotStuff.proto
RUN python -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. proto/Client.proto

FROM base as replica
CMD ["python", "replica.py"]

FROM base as client
CMD ["python", "client.py"]

FROM base as client_sync
CMD ["python", "client.py", "--runner", "sync"]

FROM base as client_random_kv
CMD ["python", "client.py", "--runner", "random_kv"]

FROM base as crypto
CMD ["python" "crypto.py"]

FROM base as logger
WORKDIR /
EXPOSE 5080
ENV ZO_ROOT_USER_EMAIL=root@example.com
ENV ZO_ROOT_USER_PASSWORD=Complexpass#123
HEALTHCHECK CMD curl -f http://localhost:5080/ || exit 1
CMD ["sh", "-c", "./openobserve > /dev/null 2>&1"]
