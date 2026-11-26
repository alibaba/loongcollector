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

## gRPC event receiver

Configure the receiver before upgrading a pipeline that uses the Go
`flusher_grpc`. Generate server bindings from
`pipeline_event_group_transfer.proto`, adding `protobuf_public/models` to the
protoc include path.

Implement the client-streaming method
`/logtail.report.PipelineEventGroupReportService/Collect`. Read each
`PipelineEventGroupReport` until the client closes the stream, then send an
empty `PipelineEventGroupReportResponse` to acknowledge receipt. Return a
non-OK gRPC status if receipt fails.

A report carries a run of consecutive events of the same kind. Read reports in
stream order. Each report carries the group's tags, metadata, and
`destination_logstore`. For example, one group of log, metric, and log events
arrives as three reports with the same group attributes. The log's event tags
are in `log_attributes` and can have the same keys as the group tags. Both
attribute lists and `observed_timestamps` correspond to events by index when
present. Event and observed timestamps use nanoseconds. Log contents listed in
`byte_field_indices` hold bytes, while the other contents hold strings. Other Go log field values use the pipeline's
text representation.
The metric attributes include a JSON representation of string, boolean,
array, and object values. Raw events are in `raw_events` with the event group
oneof left unset.

The Go flusher uses the event service for every export. Deploy the event
receiver when upgrading a pipeline that used the previous log service.
