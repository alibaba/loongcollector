// Copyright 2026 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.

package mqtt

import (
	"encoding/json"
	"testing"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestMarshalLog(t *testing.T) {
	data, err := marshalLog(&protocol.Log{
		Time: 123,
		Contents: []*protocol.Log_Content{
			{Key: "message", Value: "hello"},
			{Key: "source", Value: "test"},
		},
	})
	if err != nil {
		t.Fatalf("marshal log: %v", err)
	}

	var fields map[string]string
	if err := json.Unmarshal(data, &fields); err != nil {
		t.Fatalf("decode payload: %v", err)
	}
	want := map[string]string{"message": "hello", "source": "test", "__time__": "123"}
	if len(fields) != len(want) {
		t.Fatalf("got fields %#v, want %#v", fields, want)
	}
	for key, value := range want {
		if fields[key] != value {
			t.Errorf("field %q = %q, want %q", key, fields[key], value)
		}
	}
}

func TestNewMQTTFlusherDefaults(t *testing.T) {
	f := NewMQTTFlusher()
	if f.ConnectTimeout <= 0 {
		t.Fatalf("ConnectTimeout = %s, want positive default", f.ConnectTimeout)
	}
}
