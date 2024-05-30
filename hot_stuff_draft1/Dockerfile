FROM python:3.10 as base
# Gordon adds START #
RUN pip install --upgrade pip

COPY requirements.txt .
RUN pip install -r requirements.txt
COPY . .
WORKDIR /src
RUN python -m grpc_tools.protoc -I. --python_out=. --grpc_python_out=. proto/HotStuff.proto

FROM base as replica
CMD ["python", "replica.py"]

FROM base as client
CMD ["python", "client.py"]

FROM base as crypto
CMD ["python" "crypto.py"]

FROM base as logger
RUN wget https://github.com/openobserve/openobserve/releases/download/v0.10.7-rc2/openobserve-v0.10.7-rc2-linux-arm64.tar.gz
RUN tar -xvf openobserve-v0.10.7-rc2-linux-arm64.tar.gz
EXPOSE 5080
ENV ZO_ROOT_USER_EMAIL=root@example.com
ENV ZO_ROOT_USER_PASSWORD=Complexpass#123
CMD ["./openobserve"]
