// Copyright 2021 iLogtail Authors
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

package common

import (
	"bytes"
	"compress/gzip"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strconv"
	"sync"

	"github.com/golang/snappy"
	"github.com/pierrec/lz4"
)

const (
	ProtocolSLS          = "sls"
	ProtocolPrometheus   = "prometheus"
	ProtocolInflux       = "influx"
	ProtocolInfluxdb     = "influxdb"
	ProtocolStatsd       = "statsd"
	ProtocolOTLPLogV1    = "otlp_logv1"
	ProtocolOTLPMetricV1 = "otlp_metricv1"
	ProtocolOTLPTraceV1  = "otlp_tracev1"
	ProtocolRaw          = "raw"
	ProtocolPyroscope    = "pyroscope"
)

var bufPool = sync.Pool{
	New: func() interface{} {
		buf := bytes.NewBuffer(make([]byte, 0, 32*1024))
		return buf
	},
}

func GetPooledBuf() *bytes.Buffer {
	buf := bufPool.Get().(*bytes.Buffer)
	return buf
}

func PutPooledBuf(buf *bytes.Buffer) {
	buf.Reset()
	bufPool.Put(buf)
}

func CollectBody(res http.ResponseWriter, req *http.Request, maxBodySize int64) ([]byte, int, error) {
	body := req.Body

	// Handle gzip request bodies
	if req.Header.Get("Content-Encoding") == "gzip" {
		var err error
		body, err = gzip.NewReader(req.Body)
		if err != nil {
			return nil, http.StatusBadRequest, err
		}
		defer body.Close()
	}

	body = http.MaxBytesReader(res, body, maxBodySize)

	if req.Header.Get("Content-Encoding") == "snappy" {
		// for snappy encoding, use pooled buf to read compressed request body
		buf := GetPooledBuf()
		defer PutPooledBuf(buf)
		_, err := io.Copy(buf, body) // nolint
		if err != nil {
			return nil, http.StatusBadRequest, err
		}
		data, err := snappy.Decode(nil, buf.Bytes())
		if err != nil {
			return nil, http.StatusBadRequest, err
		}
		return data, http.StatusOK, nil
	}

	bytes, err := io.ReadAll(body)
	if err != nil {
		return nil, http.StatusRequestEntityTooLarge, err
	}

	if req.Header.Get("x-log-compresstype") == "lz4" {
		rawBodySize, err := strconv.Atoi(req.Header.Get("x-log-bodyrawsize"))
		if err != nil || rawBodySize <= 0 {
			return nil, http.StatusBadRequest, errors.New("invalid x-log-compresstype header " + req.Header.Get("x-log-bodyrawsize"))
		}
		data := make([]byte, rawBodySize)
		if readSize, err := lz4.UncompressBlock(bytes, data); readSize != rawBodySize || (err != nil && err != io.EOF) {
			return nil, http.StatusBadRequest, fmt.Errorf("uncompress lz4 error, expect : %d, real : %d, error : %v ", readSize, rawBodySize, err)
		}
		return data, http.StatusOK, nil
	}

	return bytes, http.StatusOK, nil
}

func CollectRawBody(res http.ResponseWriter, req *http.Request, maxBodySize int64) ([]byte, int, error) {
	body := req.Body
	body = http.MaxBytesReader(res, body, maxBodySize)
	bytes, err := io.ReadAll(body)
	if err != nil {
		return nil, http.StatusRequestEntityTooLarge, err
	}
	return bytes, http.StatusOK, nil
}
