package loki

import (
	"sort"
	"strconv"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/snappy"
	json "github.com/json-iterator/go"

	"github.com/grafana/loki/pkg/push"
	"github.com/prometheus/prometheus/promql/parser"
)

// batch holds pending log streams waiting to be sent to Loki, and it's used
// to reduce the number of push requests to Loki aggregating multiple log streams
// and entries in a single batch request. In case of multi-tenant Promtail, log
// streams for each tenant are stored in a dedicated batch.
type batch struct {
	streams   map[string]*push.Stream
	bytes     int
	createdAt time.Time
}

func newBatch(entries ...entry) *batch {
	b := &batch{
		streams:   map[string]*push.Stream{},
		bytes:     0,
		createdAt: time.Now(),
	}

	// Add entries to the batch
	for _, entry := range entries {
		b.add(entry)
	}

	return b
}

// add an entry to the batch
func (b *batch) add(entry entry) {
	b.bytes += len(entry.Line)

	// Append the entry to an already existing stream (if any)
	labels := entry.labels.String()
	if stream, ok := b.streams[labels]; ok {
		stream.Entries = append(stream.Entries, entry.Entry)
		return
	}

	// Add the entry as a new stream
	b.streams[labels] = &push.Stream{
		Labels:  labels,
		Entries: []push.Entry{entry.Entry},
	}
}

// sizeBytes returns the current batch size in bytes
func (b *batch) sizeBytes() int {
	return b.bytes
}

// sizeBytesAfter returns the size of the batch after the input entry
// will be added to the batch itself
func (b *batch) sizeBytesAfter(entry entry) int {
	return b.bytes + len(entry.Line)
}

// age of the batch since its creation
func (b *batch) age() time.Duration {
	return time.Since(b.createdAt)
}

// encode the batch as snappy-compressed push request, and returns
// the encoded bytes and the number of encoded entries
func (b *batch) encode() ([]byte, int, error) {
	req, entriesCount := b.createPushRequest()
	buf, err := proto.Marshal(req)
	if err != nil {
		return nil, 0, err
	}
	buf = snappy.Encode(nil, buf)
	return buf, entriesCount, nil
}

// encode the batch as json push request, and returns
// the encoded bytes and the number of encoded entries
func (b *batch) encodeJSON() ([]byte, int, error) {
	req, entriesCount := b.createPushRequest()
	type jsonStream struct {
		Stream map[string]string `json:"stream"`
		Values [][2]string       `json:"values"`
	}
	jsonReq := struct {
		Streams []jsonStream `json:"streams"`
	}{
		Streams: make([]jsonStream, 0, len(req.Streams)),
	}
	for _, stream := range req.Streams {
		parsedLabels, err := parser.NewParser(parser.Options{}).ParseMetric(stream.Labels)
		if err != nil {
			return nil, 0, err
		}
		jsonValues := make([][2]string, 0, len(stream.Entries))
		for _, entry := range stream.Entries {
			jsonValues = append(jsonValues, [2]string{
				strconv.FormatInt(entry.Timestamp.UnixNano(), 10),
				entry.Line,
			})
		}
		jsonReq.Streams = append(jsonReq.Streams, jsonStream{
			Stream: parsedLabels.Map(),
			Values: jsonValues,
		})
	}
	buf, err := json.Marshal(jsonReq)
	if err != nil {
		return nil, 0, err
	}
	return buf, entriesCount, nil
}

// creates push request and returns it, together with number of entries
func (b *batch) createPushRequest() (*push.PushRequest, int) {
	req := push.PushRequest{
		Streams: make([]push.Stream, 0, len(b.streams)),
	}

	entriesCount := 0
	labelSets := make([]string, 0, len(b.streams))
	for labelSet := range b.streams {
		labelSets = append(labelSets, labelSet)
	}
	sort.Strings(labelSets)
	for _, labelSet := range labelSets {
		stream := b.streams[labelSet]
		req.Streams = append(req.Streams, *stream)
		entriesCount += len(stream.Entries)
	}
	return &req, entriesCount
}
