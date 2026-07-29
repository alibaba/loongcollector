// Copyright 2026 iLogtail Authors
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

package opentelemetry

import (
	"bytes"
	"compress/gzip"
	"errors"
	"io"
	"net/http"
	"net/http/httptest"
	"sync/atomic"
	"testing"
	"time"

	"github.com/smartystreets/goconvey/convey"
	"go.opentelemetry.io/collector/pdata/plog/plogotlp"
	spb "google.golang.org/genproto/googleapis/rpc/status"
	"google.golang.org/protobuf/proto"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

// capturedRequest is what the test OTLP/HTTP server recorded for one request.
type capturedRequest struct {
	path            string
	contentType     string
	contentEncoding string
	userAgent       string
	header          http.Header
	body            []byte
}

// otlpHTTPTestServer is a minimal OTLP/HTTP receiver used by the tests below. It records every
// request and replies with the responses queued in statuses (the last one repeats forever).
type otlpHTTPTestServer struct {
	server   *httptest.Server
	requests chan capturedRequest
	calls    int32

	statuses    []int
	retryAfter  string
	respBody    []byte
	handlerHook func(w http.ResponseWriter, r *http.Request) bool
}

func newOTLPHTTPTestServer(statuses ...int) *otlpHTTPTestServer {
	if len(statuses) == 0 {
		statuses = []int{http.StatusOK}
	}
	s := &otlpHTTPTestServer{
		requests: make(chan capturedRequest, 64),
		statuses: statuses,
	}
	s.server = httptest.NewServer(http.HandlerFunc(s.handle))
	return s
}

func (s *otlpHTTPTestServer) handle(w http.ResponseWriter, r *http.Request) {
	body, _ := io.ReadAll(r.Body)
	s.requests <- capturedRequest{
		path:            r.URL.Path,
		contentType:     r.Header.Get("Content-Type"),
		contentEncoding: r.Header.Get("Content-Encoding"),
		userAgent:       r.Header.Get("User-Agent"),
		header:          r.Header.Clone(),
		body:            body,
	}
	n := int(atomic.AddInt32(&s.calls, 1))

	if s.handlerHook != nil && s.handlerHook(w, r) {
		return
	}

	status := s.statuses[len(s.statuses)-1]
	if n <= len(s.statuses) {
		status = s.statuses[n-1]
	}
	if s.retryAfter != "" && (status == http.StatusTooManyRequests || status == http.StatusServiceUnavailable) {
		w.Header().Set("Retry-After", s.retryAfter)
	}
	w.WriteHeader(status)
	if len(s.respBody) > 0 {
		_, _ = w.Write(s.respBody)
	}
}

func (s *otlpHTTPTestServer) callCount() int { return int(atomic.LoadInt32(&s.calls)) }

func (s *otlpHTTPTestServer) close() { s.server.Close() }

// decodeLogRequest turns a captured request body back into an OTLP logs export request.
func decodeLogRequest(t *testing.T, req capturedRequest, encoding string) plogotlp.ExportRequest {
	t.Helper()
	raw := req.body
	if req.contentEncoding == otlpHTTPCompressionGzip {
		gr, err := gzip.NewReader(bytes.NewReader(raw))
		convey.So(err, convey.ShouldBeNil)
		raw, err = io.ReadAll(gr)
		convey.So(err, convey.ShouldBeNil)
		convey.So(gr.Close(), convey.ShouldBeNil)
	}

	out := plogotlp.NewExportRequest()
	var err error
	if encoding == otlpHTTPEncodingJSON {
		err = out.UnmarshalJSON(raw)
	} else {
		err = out.UnmarshalProto(raw)
	}
	convey.So(err, convey.ShouldBeNil)
	return out
}

// newTestFlusherOTLPHTTP builds an initialized flusher pointing at the given base endpoint.
func newTestFlusherOTLPHTTP(t *testing.T, mutate func(f *FlusherOTLPHTTP)) *FlusherOTLPHTTP {
	t.Helper()
	f := NewFlusherOTLPHTTP()
	f.Retry.InitialDelay = 10 * time.Millisecond
	f.Retry.MaxDelay = 20 * time.Millisecond
	if mutate != nil {
		mutate(f)
	}
	err := f.Init(mock.NewEmptyContext("p", "l", "c"))
	convey.So(err, convey.ShouldBeNil)
	return f
}

func Test_FlusherOTLPHTTP_Init(t *testing.T) {
	convey.Convey("Given a FlusherOTLPHTTP", t, func() {
		logCtx := mock.NewEmptyContext("p", "l", "c")

		convey.Convey("When no endpoint is configured, Init should fail", func() {
			f := NewFlusherOTLPHTTP()
			err := f.Init(logCtx)
			convey.So(err, convey.ShouldBeError)
			convey.So(err.Error(), convey.ShouldEqual, "invalid_otlp_http_configs")
		})

		convey.Convey("When only the base endpoint is set, the signal path should be appended", func() {
			f := NewFlusherOTLPHTTP()
			f.Endpoint = "http://127.0.0.1:4318"
			convey.So(f.Init(logCtx), convey.ShouldBeNil)
			convey.So(f.logClient, convey.ShouldNotBeNil)
			convey.So(f.logClient.url, convey.ShouldEqual, "http://127.0.0.1:4318/v1/logs")
			convey.So(f.IsReady("p", "l", 1), convey.ShouldBeTrue)
			convey.So(f.Stop(), convey.ShouldBeNil)
		})

		convey.Convey("When the base endpoint has a trailing slash, no double slash should appear", func() {
			f := NewFlusherOTLPHTTP()
			f.Endpoint = "http://127.0.0.1:4318/"
			convey.So(f.Init(logCtx), convey.ShouldBeNil)
			convey.So(f.logClient.url, convey.ShouldEqual, "http://127.0.0.1:4318/v1/logs")
		})

		convey.Convey("When the Logs endpoint is set, it should be used verbatim", func() {
			f := NewFlusherOTLPHTTP()
			f.Endpoint = "http://127.0.0.1:4318"
			f.Logs = &otlpHTTPSignalConfig{Endpoint: "http://otherhost:8080/custom/logs"}
			convey.So(f.Init(logCtx), convey.ShouldBeNil)
			convey.So(f.logClient.url, convey.ShouldEqual, "http://otherhost:8080/custom/logs")
		})

		convey.Convey("When only the Logs endpoint is set, Init should succeed", func() {
			f := NewFlusherOTLPHTTP()
			f.Logs = &otlpHTTPSignalConfig{Endpoint: "http://127.0.0.1:4318/v1/logs"}
			convey.So(f.Init(logCtx), convey.ShouldBeNil)
			convey.So(f.logClient.url, convey.ShouldEqual, "http://127.0.0.1:4318/v1/logs")
		})

		convey.Convey("Defaults should be filled in", func() {
			f := NewFlusherOTLPHTTP()
			f.Endpoint = "http://127.0.0.1:4318"
			convey.So(f.Init(logCtx), convey.ShouldBeNil)
			convey.So(f.Version, convey.ShouldEqual, v1)
			convey.So(f.Encoding, convey.ShouldEqual, otlpHTTPEncodingProto)
			convey.So(f.Compression, convey.ShouldEqual, otlpHTTPCompressionGzip)
			convey.So(f.Timeout, convey.ShouldEqual, otlpHTTPDefaultTimeout)
			convey.So(f.Retry.MaxRetryTimes, convey.ShouldEqual, otlpHTTPDefaultMaxRetry)
		})

		convey.Convey("An unsupported encoding should fail Init", func() {
			f := NewFlusherOTLPHTTP()
			f.Endpoint = "http://127.0.0.1:4318"
			f.Encoding = "yaml"
			convey.So(f.Init(logCtx), convey.ShouldBeError)
		})

		convey.Convey("An unsupported compression should fail Init", func() {
			f := NewFlusherOTLPHTTP()
			f.Endpoint = "http://127.0.0.1:4318"
			f.Compression = "snappy"
			convey.So(f.Init(logCtx), convey.ShouldBeError)
		})

		convey.Convey("An unsupported version should fail Init", func() {
			f := NewFlusherOTLPHTTP()
			f.Endpoint = "http://127.0.0.1:4318"
			f.Version = "v9"
			convey.So(f.Init(logCtx), convey.ShouldBeError)
		})

		convey.Convey("An empty compression means no compression", func() {
			f := NewFlusherOTLPHTTP()
			f.Endpoint = "http://127.0.0.1:4318"
			f.Compression = ""
			convey.So(f.Init(logCtx), convey.ShouldBeNil)
		})
	})
}

func Test_FlusherOTLPHTTP_BuildSignalURL(t *testing.T) {
	convey.Convey("buildSignalURL should follow the otlphttpexporter rules", t, func() {
		cases := []struct {
			name     string
			base     string
			signal   *otlpHTTPSignalConfig
			expected string
		}{
			{"base only", "http://h:4318", nil, "http://h:4318/v1/logs"},
			{"base with trailing slashes", "http://h:4318//", nil, "http://h:4318/v1/logs"},
			{"signal override wins", "http://h:4318", &otlpHTTPSignalConfig{Endpoint: "http://o/l"}, "http://o/l"},
			{"signal without endpoint falls back", "http://h:4318", &otlpHTTPSignalConfig{}, "http://h:4318/v1/logs"},
			{"no base and no override", "", nil, ""},
			{"no base but override", "", &otlpHTTPSignalConfig{Endpoint: "http://o/l"}, "http://o/l"},
		}

		for _, c := range cases {
			convey.Convey(c.name, func() {
				f := &FlusherOTLPHTTP{Endpoint: c.base}
				convey.So(f.buildSignalURL(c.signal, otlpHTTPLogsPath), convey.ShouldEqual, c.expected)
			})
		}
	})
}

func Test_FlusherOTLPHTTP_BuildSignalHeaders(t *testing.T) {
	convey.Convey("buildSignalHeaders should merge with the signal winning", t, func() {
		f := &FlusherOTLPHTTP{Headers: map[string]string{"X-Base": "1", "X-Both": "base"}}

		convey.Convey("Without a signal config only the base headers are used", func() {
			convey.So(f.buildSignalHeaders(nil), convey.ShouldResemble, map[string]string{"X-Base": "1", "X-Both": "base"})
		})

		convey.Convey("With a signal config the signal headers override", func() {
			got := f.buildSignalHeaders(&otlpHTTPSignalConfig{Headers: map[string]string{"X-Both": "signal", "X-Signal": "2"}})
			convey.So(got, convey.ShouldResemble, map[string]string{"X-Base": "1", "X-Both": "signal", "X-Signal": "2"})
			// The base headers must not be mutated.
			convey.So(f.Headers, convey.ShouldResemble, map[string]string{"X-Base": "1", "X-Both": "base"})
		})
	})
}

func Test_FlusherOTLPHTTP_Flush_Proto(t *testing.T) {
	convey.Convey("Given an OTLP/HTTP server", t, func() {
		server := newOTLPHTTPTestServer(http.StatusOK)
		defer server.close()

		convey.Convey("When flushing v1 log groups with proto encoding", func() {
			f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) {
				f.Endpoint = server.server.URL
				f.Headers = map[string]string{"X-AppKey": "test-key"}
			})

			groupList := makeTestLogGroupList().GetLogGroupList()
			convey.So(f.Flush("p", "l", "c", groupList), convey.ShouldBeNil)

			req := <-server.requests
			convey.So(req.path, convey.ShouldEqual, otlpHTTPLogsPath)
			convey.So(req.contentType, convey.ShouldEqual, otlpHTTPProtobufContentType)
			convey.So(req.contentEncoding, convey.ShouldEqual, otlpHTTPCompressionGzip)
			convey.So(req.userAgent, convey.ShouldNotBeEmpty)
			convey.So(req.header.Get("X-AppKey"), convey.ShouldEqual, "test-key")

			got := decodeLogRequest(t, req, otlpHTTPEncodingProto)
			expected := otlpConvertLogGroupToRequest(f.converter, groupList)
			convey.So(got.Logs().ResourceLogs().Len(), convey.ShouldEqual, expected.Logs().ResourceLogs().Len())
			convey.So(got.Logs().LogRecordCount(), convey.ShouldEqual, expected.Logs().LogRecordCount())
			convey.So(got.Logs().ResourceLogs().At(0).ScopeLogs().At(0).LogRecords().At(0).Body().AsString(),
				convey.ShouldEqual,
				expected.Logs().ResourceLogs().At(0).ScopeLogs().At(0).LogRecords().At(0).Body().AsString())
		})
	})
}

func Test_FlusherOTLPHTTP_Flush_JSON(t *testing.T) {
	convey.Convey("Given an OTLP/HTTP server", t, func() {
		server := newOTLPHTTPTestServer(http.StatusOK)
		defer server.close()

		convey.Convey("When flushing with json encoding", func() {
			f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) {
				f.Endpoint = server.server.URL
				f.Encoding = otlpHTTPEncodingJSON
			})

			groupList := makeTestLogGroupList().GetLogGroupList()
			convey.So(f.Flush("p", "l", "c", groupList), convey.ShouldBeNil)

			req := <-server.requests
			convey.So(req.contentType, convey.ShouldEqual, otlpHTTPJSONContentType)

			got := decodeLogRequest(t, req, otlpHTTPEncodingJSON)
			expected := otlpConvertLogGroupToRequest(f.converter, groupList)
			convey.So(got.Logs().LogRecordCount(), convey.ShouldEqual, expected.Logs().LogRecordCount())
		})
	})
}

func Test_FlusherOTLPHTTP_Flush_NoCompression(t *testing.T) {
	convey.Convey("Given an OTLP/HTTP server", t, func() {
		server := newOTLPHTTPTestServer(http.StatusOK)
		defer server.close()

		convey.Convey("When compression is disabled, no Content-Encoding should be sent", func() {
			f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) {
				f.Endpoint = server.server.URL
				f.Compression = otlpHTTPCompressionNone
			})

			groupList := makeTestLogGroupList().GetLogGroupList()
			convey.So(f.Flush("p", "l", "c", groupList), convey.ShouldBeNil)

			req := <-server.requests
			convey.So(req.contentEncoding, convey.ShouldBeEmpty)

			got := decodeLogRequest(t, req, otlpHTTPEncodingProto)
			convey.So(got.Logs().LogRecordCount(), convey.ShouldBeGreaterThan, 0)
		})
	})
}

func Test_FlusherOTLPHTTP_Flush_HeadersCannotCorruptContentType(t *testing.T) {
	convey.Convey("Given an OTLP/HTTP server", t, func() {
		server := newOTLPHTTPTestServer(http.StatusOK)
		defer server.close()

		convey.Convey("A stale Content-Type in the config must not override the real encoding", func() {
			f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) {
				f.Endpoint = server.server.URL
				f.Headers = map[string]string{
					"Content-Type":     otlpHTTPJSONContentType,
					"Content-Encoding": "snappy",
					"User-Agent":       "custom-agent",
				}
			})

			convey.So(f.Flush("p", "l", "c", makeTestLogGroupList().GetLogGroupList()), convey.ShouldBeNil)

			req := <-server.requests
			convey.So(req.contentType, convey.ShouldEqual, otlpHTTPProtobufContentType)
			convey.So(req.contentEncoding, convey.ShouldEqual, otlpHTTPCompressionGzip)
			// User-Agent stays overridable.
			convey.So(req.userAgent, convey.ShouldEqual, "custom-agent")
		})
	})
}

func Test_FlusherOTLPHTTP_Flush_Empty(t *testing.T) {
	convey.Convey("Given an OTLP/HTTP server", t, func() {
		server := newOTLPHTTPTestServer(http.StatusOK)
		defer server.close()

		convey.Convey("Flushing an empty log group list still succeeds", func() {
			f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) { f.Endpoint = server.server.URL })
			convey.So(f.Flush("p", "l", "c", nil), convey.ShouldBeNil)
			convey.So(server.callCount(), convey.ShouldEqual, 1)
		})
	})
}

func Test_FlusherOTLPHTTP_Export_Logs(t *testing.T) {
	convey.Convey("Given an OTLP/HTTP server", t, func() {
		server := newOTLPHTTPTestServer(http.StatusOK)
		defer server.close()

		convey.Convey("When exporting v2 log events", func() {
			f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) { f.Endpoint = server.server.URL })

			slice := makeTestPipelineGroupEventsLogSlice()
			convey.So(f.Export(slice, helper.NewNoopPipelineContext()), convey.ShouldBeNil)

			req := <-server.requests
			convey.So(req.path, convey.ShouldEqual, otlpHTTPLogsPath)

			got := decodeLogRequest(t, req, otlpHTTPEncodingProto)
			expected, _, _ := otlpConvertPipelineEventsToRequests(f.converter, slice, f.context.GetRuntimeContext())
			convey.So(got.Logs().ResourceLogs().Len(), convey.ShouldEqual, expected.Logs().ResourceLogs().Len())
			convey.So(got.Logs().LogRecordCount(), convey.ShouldEqual, expected.Logs().LogRecordCount())
		})
	})
}

func Test_FlusherOTLPHTTP_Export_MetricsAndTracesAreDropped(t *testing.T) {
	convey.Convey("Given an OTLP/HTTP server", t, func() {
		server := newOTLPHTTPTestServer(http.StatusOK)
		defer server.close()

		convey.Convey("Exporting metrics should not fail and should not carry metric data", func() {
			f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) { f.Endpoint = server.server.URL })
			convey.So(f.Export(makeTestPipelineGroupEventsMetricSlice(), helper.NewNoopPipelineContext()), convey.ShouldBeNil)

			// The logs request is still sent, but it is empty.
			req := <-server.requests
			convey.So(req.path, convey.ShouldEqual, otlpHTTPLogsPath)
			convey.So(decodeLogRequest(t, req, otlpHTTPEncodingProto).Logs().LogRecordCount(), convey.ShouldEqual, 0)
		})

		convey.Convey("Exporting traces should not fail", func() {
			f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) { f.Endpoint = server.server.URL })
			convey.So(f.Export(makeTestPipelineGroupEventsTraceSlice(), helper.NewNoopPipelineContext()), convey.ShouldBeNil)

			req := <-server.requests
			convey.So(decodeLogRequest(t, req, otlpHTTPEncodingProto).Logs().LogRecordCount(), convey.ShouldEqual, 0)
		})
	})
}

func Test_FlusherOTLPHTTP_Retry_Retryable(t *testing.T) {
	convey.Convey("Retryable status codes should be retried until success", t, func() {
		cases := []struct {
			name       string
			statuses   []int
			retryAfter string
			wantCalls  int
		}{
			{"429 then 200", []int{http.StatusTooManyRequests, http.StatusOK}, "", 2},
			{"429 with Retry-After then 200", []int{http.StatusTooManyRequests, http.StatusOK}, "1", 2},
			{"503 twice then 200", []int{http.StatusServiceUnavailable, http.StatusServiceUnavailable, http.StatusOK}, "", 3},
			{"502 then 200", []int{http.StatusBadGateway, http.StatusOK}, "", 2},
			{"504 then 200", []int{http.StatusGatewayTimeout, http.StatusOK}, "", 2},
		}

		for _, c := range cases {
			convey.Convey(c.name, func() {
				server := newOTLPHTTPTestServer(c.statuses...)
				// Keep the Retry-After honoured but short so the test stays fast.
				server.retryAfter = c.retryAfter
				defer server.close()

				f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) { f.Endpoint = server.server.URL })
				convey.So(f.Flush("p", "l", "c", makeTestLogGroupList().GetLogGroupList()), convey.ShouldBeNil)
				convey.So(server.callCount(), convey.ShouldEqual, c.wantCalls)
			})
		}
	})
}

func Test_FlusherOTLPHTTP_NoRetry_Permanent(t *testing.T) {
	convey.Convey("Permanent status codes must not be retried", t, func() {
		// 500 is explicitly NOT retryable per the OTLP/HTTP specification.
		for _, status := range []int{http.StatusBadRequest, http.StatusUnauthorized, http.StatusForbidden,
			http.StatusNotFound, http.StatusRequestEntityTooLarge, http.StatusInternalServerError} {
			convey.Convey(http.StatusText(status), func() {
				server := newOTLPHTTPTestServer(status)
				defer server.close()

				f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) { f.Endpoint = server.server.URL })
				convey.So(f.Flush("p", "l", "c", makeTestLogGroupList().GetLogGroupList()), convey.ShouldBeError)
				convey.So(server.callCount(), convey.ShouldEqual, 1)
			})
		}
	})
}

func Test_FlusherOTLPHTTP_Retry_Disabled(t *testing.T) {
	convey.Convey("With retry disabled a retryable failure is attempted once", t, func() {
		server := newOTLPHTTPTestServer(http.StatusServiceUnavailable)
		defer server.close()

		f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) {
			f.Endpoint = server.server.URL
			f.Retry.Enable = false
		})
		convey.So(f.Flush("p", "l", "c", makeTestLogGroupList().GetLogGroupList()), convey.ShouldBeError)
		convey.So(server.callCount(), convey.ShouldEqual, 1)
	})
}

func Test_FlusherOTLPHTTP_Retry_Exhausted(t *testing.T) {
	convey.Convey("When retries are exhausted the error is returned", t, func() {
		server := newOTLPHTTPTestServer(http.StatusServiceUnavailable)
		defer server.close()

		f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) {
			f.Endpoint = server.server.URL
			f.Retry.MaxRetryTimes = 2
		})
		convey.So(f.Flush("p", "l", "c", makeTestLogGroupList().GetLogGroupList()), convey.ShouldBeError)
		// One initial attempt plus two retries.
		convey.So(server.callCount(), convey.ShouldEqual, 3)
	})
}

func Test_FlusherOTLPHTTP_Retry_NetworkError(t *testing.T) {
	convey.Convey("A transport level error should be retried", t, func() {
		f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) {
			f.Endpoint = "http://127.0.0.1:4318"
			f.Retry.MaxRetryTimes = 2
		})

		doer := &countingErrDoer{err: errors.New("connection refused")}
		f.SetHTTPDoer(doer)

		convey.So(f.Flush("p", "l", "c", makeTestLogGroupList().GetLogGroupList()), convey.ShouldBeError)
		convey.So(doer.calls, convey.ShouldEqual, 3)
	})
}

type countingErrDoer struct {
	err   error
	calls int
}

func (d *countingErrDoer) Do(*http.Request) (*http.Response, error) {
	d.calls++
	return nil, d.err
}

func Test_FlusherOTLPHTTP_Timeout(t *testing.T) {
	convey.Convey("A server slower than Timeout should produce a retryable error", t, func() {
		server := newOTLPHTTPTestServer(http.StatusOK)
		server.handlerHook = func(w http.ResponseWriter, r *http.Request) bool {
			time.Sleep(300 * time.Millisecond)
			return false
		}
		defer server.close()

		f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) {
			f.Endpoint = server.server.URL
			f.Timeout = 50 * time.Millisecond
			f.Retry.MaxRetryTimes = 1
		})

		convey.So(f.Flush("p", "l", "c", makeTestLogGroupList().GetLogGroupList()), convey.ShouldBeError)
		convey.So(server.callCount(), convey.ShouldEqual, 2)
	})
}

func Test_FlusherOTLPHTTP_PartialSuccess(t *testing.T) {
	convey.Convey("A partial success response is warned about but not treated as an error", t, func() {
		resp := plogotlp.NewExportResponse()
		resp.PartialSuccess().SetRejectedLogRecords(5)
		resp.PartialSuccess().SetErrorMessage("5 records rejected")
		body, err := resp.MarshalProto()
		convey.So(err, convey.ShouldBeNil)

		server := newOTLPHTTPTestServer(http.StatusOK)
		server.respBody = body
		defer server.close()

		f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) { f.Endpoint = server.server.URL })
		convey.So(f.Flush("p", "l", "c", makeTestLogGroupList().GetLogGroupList()), convey.ShouldBeNil)
		convey.So(server.callCount(), convey.ShouldEqual, 1)
	})
}

func Test_FlusherOTLPHTTP_PartialSuccess_JSON(t *testing.T) {
	convey.Convey("A json encoded partial success response is handled too", t, func() {
		resp := plogotlp.NewExportResponse()
		resp.PartialSuccess().SetRejectedLogRecords(2)
		body, err := resp.MarshalJSON()
		convey.So(err, convey.ShouldBeNil)

		server := newOTLPHTTPTestServer(http.StatusOK)
		server.respBody = body
		defer server.close()

		f := newTestFlusherOTLPHTTP(t, func(f *FlusherOTLPHTTP) {
			f.Endpoint = server.server.URL
			f.Encoding = otlpHTTPEncodingJSON
		})
		convey.So(f.Flush("p", "l", "c", makeTestLogGroupList().GetLogGroupList()), convey.ShouldBeNil)
	})
}

func Test_FlusherOTLPHTTP_Stop_And_NotReady(t *testing.T) {
	convey.Convey("Given an uninitialized flusher", t, func() {
		convey.Convey("Stop should be safe", func() {
			f := NewFlusherOTLPHTTP()
			convey.So(f.Stop(), convey.ShouldBeNil)
		})

		convey.Convey("Flush without a log client is a no-op", func() {
			f := NewFlusherOTLPHTTP()
			f.context = mock.NewEmptyContext("p", "l", "c")
			convey.So(f.Flush("p", "l", "c", makeTestLogGroupList().GetLogGroupList()), convey.ShouldBeNil)
		})

		convey.Convey("IsReady should be false", func() {
			f := NewFlusherOTLPHTTP()
			f.context = mock.NewEmptyContext("p", "l", "c")
			convey.So(f.IsReady("p", "l", 1), convey.ShouldBeFalse)
		})
	})
}

func Test_FlusherOTLPHTTP_Registered(t *testing.T) {
	convey.Convey("flusher_otlp_http should be registered", t, func() {
		creator, ok := pipeline.Flushers["flusher_otlp_http"]
		convey.So(ok, convey.ShouldBeTrue)
		f, ok := creator().(*FlusherOTLPHTTP)
		convey.So(ok, convey.ShouldBeTrue)
		convey.So(f.Description(), convey.ShouldNotBeEmpty)
		convey.So(f.Encoding, convey.ShouldEqual, otlpHTTPEncodingProto)
	})
}

func Test_ParseRetryAfterDuration(t *testing.T) {
	convey.Convey("parseRetryAfterDuration", t, func() {
		convey.Convey("An empty header yields zero", func() {
			convey.So(parseRetryAfterDuration(""), convey.ShouldEqual, time.Duration(0))
		})
		convey.Convey("Integer seconds are parsed", func() {
			convey.So(parseRetryAfterDuration("3"), convey.ShouldEqual, 3*time.Second)
			convey.So(parseRetryAfterDuration(" 7 "), convey.ShouldEqual, 7*time.Second)
		})
		convey.Convey("Non positive seconds yield zero", func() {
			convey.So(parseRetryAfterDuration("0"), convey.ShouldEqual, time.Duration(0))
			convey.So(parseRetryAfterDuration("-5"), convey.ShouldEqual, time.Duration(0))
		})
		convey.Convey("A future HTTP date yields a positive delay", func() {
			header := time.Now().Add(30 * time.Second).UTC().Format(http.TimeFormat)
			got := parseRetryAfterDuration(header)
			convey.So(got, convey.ShouldBeGreaterThan, time.Duration(0))
			convey.So(got, convey.ShouldBeLessThanOrEqualTo, 31*time.Second)
		})
		convey.Convey("A past HTTP date yields zero", func() {
			header := time.Now().Add(-time.Minute).UTC().Format(http.TimeFormat)
			convey.So(parseRetryAfterDuration(header), convey.ShouldEqual, time.Duration(0))
		})
		convey.Convey("An unparsable header yields zero", func() {
			convey.So(parseRetryAfterDuration("later"), convey.ShouldEqual, time.Duration(0))
		})
	})
}

func Test_FlusherOTLPHTTP_NextRetryDelay(t *testing.T) {
	convey.Convey("nextRetryDelay", t, func() {
		f := NewFlusherOTLPHTTP()
		f.Retry.InitialDelay = time.Second
		f.Retry.MaxDelay = 4 * time.Second

		convey.Convey("The delay grows and stays within [d/2, d]", func() {
			d0 := f.nextRetryDelay(0, 0)
			convey.So(d0, convey.ShouldBeGreaterThanOrEqualTo, 500*time.Millisecond)
			convey.So(d0, convey.ShouldBeLessThanOrEqualTo, time.Second)

			d2 := f.nextRetryDelay(2, 0)
			convey.So(d2, convey.ShouldBeGreaterThanOrEqualTo, 2*time.Second)
			convey.So(d2, convey.ShouldBeLessThanOrEqualTo, 4*time.Second)
		})

		convey.Convey("The delay is capped at MaxDelay", func() {
			convey.So(f.nextRetryDelay(20, 0), convey.ShouldBeLessThanOrEqualTo, 4*time.Second)
		})

		convey.Convey("A larger Retry-After wins", func() {
			convey.So(f.nextRetryDelay(0, 10*time.Second), convey.ShouldEqual, 10*time.Second)
		})

		convey.Convey("A smaller Retry-After is ignored", func() {
			convey.So(f.nextRetryDelay(2, time.Nanosecond), convey.ShouldBeGreaterThanOrEqualTo, 2*time.Second)
		})
	})
}

func Test_FlusherOTLPHTTP_DecodeErrorStatus(t *testing.T) {
	convey.Convey("decodeErrorStatus", t, func() {
		f := NewFlusherOTLPHTTP()

		convey.Convey("An empty body is reported as such", func() {
			convey.So(f.decodeErrorStatus(nil), convey.ShouldEqual, "<empty body>")
		})

		convey.Convey("A google.rpc.Status body is rendered", func() {
			body, err := proto.Marshal(&spb.Status{Code: 3, Message: "bad field"})
			convey.So(err, convey.ShouldBeNil)
			convey.So(f.decodeErrorStatus(body), convey.ShouldEqual, "code=3 message=bad field")
		})

		convey.Convey("A non protobuf body falls back to the raw text", func() {
			convey.So(f.decodeErrorStatus([]byte("plain text error")), convey.ShouldEqual, "plain text error")
		})
	})
}
