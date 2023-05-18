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
	"io/ioutil"
	"net/http"
	"reflect"
	"strconv"
	"sync"
	"unsafe"

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
		buf := make([]byte, 0, 32*1024)
		return &buf
	},
}

func GetPooledBuf() *[]byte {
	buf := bufPool.Get().(*[]byte)
	return buf
}

func PutPooledBuf(buf *[]byte) {
	*buf = (*buf)[:0]
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
	reqBuf := bytes.NewBuffer(*GetPooledBuf())
	readBuf := GetPooledBuf()
	sh := (*reflect.SliceHeader)(unsafe.Pointer(readBuf))
	sh.Len = sh.Cap
	defer PutPooledBuf(readBuf)
	_, err := io.CopyBuffer(reqBuf, req.Body, *readBuf)
	if err != nil {
		reqBuf.Reset()
		buf := reqBuf.Bytes()
		PutPooledBuf(&buf)
		return nil, http.StatusRequestEntityTooLarge, err
	}

	reqBytes := reqBuf.Bytes()
	if req.Header.Get("Content-Encoding") == "snappy" {
		data := GetPooledBuf()
		reqBytes, err = snappy.Decode(*data, reqBytes)
		reqBuf.Reset()
		buf := reqBuf.Bytes()
		PutPooledBuf(&buf)
		if err != nil {
			PutPooledBuf(data)
			return nil, http.StatusBadRequest, err
		}
		return reqBytes, http.StatusOK, nil
	}

	if req.Header.Get("x-log-compresstype") == "lz4" {
		rawBodySize, err := strconv.Atoi(req.Header.Get("x-log-bodyrawsize"))
		if err != nil || rawBodySize <= 0 {
			return nil, http.StatusBadRequest, errors.New("invalid x-log-compresstype header " + req.Header.Get("x-log-bodyrawsize"))
		}
		data := make([]byte, rawBodySize)
		if readSize, err := lz4.UncompressBlock(reqBytes, data); readSize != rawBodySize || (err != nil && err != io.EOF) {
			return nil, http.StatusBadRequest, fmt.Errorf("uncompress lz4 error, expect : %d, real : %d, error : %v ", readSize, rawBodySize, err)
		}
		return data, http.StatusOK, nil
	}

	return reqBytes, http.StatusOK, nil
}

func CollectRawBody(res http.ResponseWriter, req *http.Request, maxBodySize int64) ([]byte, int, error) {
	body := req.Body
	body = http.MaxBytesReader(res, body, maxBodySize)
	bytes, err := ioutil.ReadAll(body)
	if err != nil {
		return nil, http.StatusRequestEntityTooLarge, err
	}
	return bytes, http.StatusOK, nil
}
