# How to generate pb

cd ~
wget https://ghproxy.com/https://github.com/gogo/protobuf/archive/refs/tags/v1.3.2.tar.gz
tar xzf v1.3.2.tar.gz
mkdir -p ${GOPATH}/src/github.com/gogo
mv protobuf-1.3.2 ${GOPATH}/src/github.com/gogo/protobuf

cd ~
wget https://ghproxy.com/https://github.com/protocolbuffers/protobuf/releases/download/v3.14.0/protoc-3.14.0-linux-x86_64.zip
unzip protoc-3.14.0-linux-x86_64.zip
mv bin/protoc /usr/local/bin/
mv include/google/ /usr/local/include/

go install github.com/gogo/protobuf/protoc-gen-gogofaster@latest

bash pkg/protocol/proto/genernate.sh
