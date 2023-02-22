package jfr

import (
	"bytes"
	"compress/gzip"
	"context"
	"io"
	"io/ioutil"
	"os"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"

	"github.com/alibaba/ilogtail/helper/profile"
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
	return ioutil.ReadAll(r)
}

func readRawFile(fname string) ([]byte, error) {
	f, err := os.Open(fname)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	return ioutil.ReadAll(f)
}
