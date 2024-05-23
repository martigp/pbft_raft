FROM alpine:3.12.0

RUN apk add \
    gcc=9.3.0-r2 \
    libc-dev=0.7.2-r3 \
    make=4.3-r0

FROM python:3.10 as base
# Gordon adds START #
RUN pip install --upgrade pip

RUN curl --proto '=https' --tlsv1.2 -sSf -y https://sh.rustup.rs | sh
ENV PATH ~/.cargo/bin:$PATH
# I know that I shouldn't have this here but just so I don't mess up
# requirements, the first requires gcc
RUN pip install --use-pep517 bls-lib
# This one requires rust
RUN pip install multi-party-schnorr
# GORDON adds END#
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

