#!/bin/bash

PROTO_HOME=$(cd $(dirname "$0"); pwd)
PROTO_GEN_HOME=$PROTO_HOME/gen
if [ -d "${PROTO_GEN_HOME}" ]; then
  rm -rf "${PROTO_GEN_HOME}"
fi
mkdir "${PROTO_GEN_HOME}"
export PATH=$PATH:$GOPATH/bin

protoc -I="${PROTO_HOME}" \
  -I="${GOPATH}/src" \
  --gogofaster_out=plugins=grpc:"${PROTO_GEN_HOME}" "${PROTO_HOME}"/*.proto