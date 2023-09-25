// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package jfr

import (
	"bytes"
	"compress/gzip"
	"context"
	"io"
	"os"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"

	"github.com/alibaba/ilogtail/pkg/helper/profile"
)

func TestRawProfile_Parse(t *testing.T) {

}

func TestParse(t *testing.T) {
	jfr, err := readGzipFile("./testdata/example.jfr.gz")
	if err != nil {
		t.Fatalf("Unable to read JFR file: %s", err)
	}

	rp := RawProfile{
		FormDataContentType: "",
		RawData:             jfr,
	}

	logs, err := rp.Parse(context.Background(), &profile.Meta{
		Tags:            map[string]string{"_app_name_": "12"},
		SpyName:         "javaspy",
		StartTime:       time.Now(),
		EndTime:         time.Now(),
		SampleRate:      99,
		Units:           profile.SamplesUnits,
		AggregationType: profile.SumAggType,
	}, nil)
	if err != nil {
		t.Fatalf("Failed to parse JFR: %s", err)
		return
	}
	require.Equal(t, len(logs), 323)
}

func TestParseJFR(t *testing.T) {
	jfr, err := readRawFile("./testdata/jfr.raw")
	if err != nil {
		t.Fatalf("Unable to read JFR file: %s", err)
	}

	jfrLables, err := readRawFile("./testdata/jfr_labels.raw")
	if err != nil {
		t.Fatalf("Unable to read JFR label file: %s", err)
	}
	var labels LabelsSnapshot

	if err = proto.Unmarshal(jfrLables, &labels); err != nil {
		t.Fatalf("Unable to read JFR label file: %s", err)
	}

	var reader io.Reader = bytes.NewReader(jfr)

	r := new(RawProfile)
	meta := &profile.Meta{
		Tags:            map[string]string{"_app_name_": "12"},
		SpyName:         "javaspy",
		StartTime:       time.Now(),
		EndTime:         time.Now(),
		SampleRate:      99,
		Units:           profile.SamplesUnits,
		AggregationType: profile.SumAggType,
	}
	cb := r.extractProfileV1(meta, nil)
	r.ParseJFR(context.Background(), meta, reader, &labels, cb)
	logs := r.logs
	require.Equal(t, len(logs), 3)
}

func readGzipFile(fname string) ([]byte, error) {
	f, err := os.Open(fname)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	r, err := gzip.NewReader(f)
	if err != nil {
		return nil, err
	}
	defer r.Close()
	return io.ReadAll(r)
}

func readRawFile(fname string) ([]byte, error) {
	f, err := os.Open(fname)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	return io.ReadAll(f)
}
