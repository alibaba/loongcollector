// Copyright 2024 iLogtail Authors
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

package prometheus

import (
	"bytes"
	"encoding/base64"
	"fmt"
	"io"
	"net/http"
	"sort"
	"strings"
	"testing"

	"github.com/golang/snappy"
	"github.com/jarcoal/httpmock"
	"github.com/prometheus/prometheus/prompb"
	. "github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
	"github.com/alibaba/ilogtail/pkg/protocol/encoder/prometheus"
	_ "github.com/alibaba/ilogtail/plugins/extension/basicauth"
	defaultencoder "github.com/alibaba/ilogtail/plugins/extension/default_encoder"
	hf "github.com/alibaba/ilogtail/plugins/flusher/http"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

// 场景：插件初始化
// 因子：不正确的config
// 因子：必需字段Endpoint缺失
// 预期：初始化失败
func TestPrometheusFlusher_ShouldInitFailed_GivenEmptyEndpoint(t *testing.T) {
	Convey("Given a prometheus flusher with empty Endpoint", t, func() {
		flusher := &FlusherPrometheus{}

		Convey("Then Init() should return error", func() {
			err := flusher.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldNotBeNil)

			ready := flusher.IsReady("p", "l", 1)
			So(ready, ShouldBeFalse)
		})
	})
}

// 场景：插件初始化
// 因子：正确的config
// 因子：必要的config
// 预期：初始化成功
func TestPrometheusFlusher_ShouldInitSuccess_GivenNecessaryConfig(t *testing.T) {
	Convey("Given a prometheus flusher with necessary config", t, func() {
		flusher := &FlusherPrometheus{
			config: config{
				Endpoint: "http://localhost:9090/write",
			},
		}

		Convey("Then Init() should implement prometheus encoder success", func() {
			err := flusher.Init(mock.NewEmptyContext("p", "l", "c"))
			So(err, ShouldBeNil)

			ready := flusher.IsReady("p", "l", 1)
			So(ready, ShouldBeTrue)

			Convey("extension should be *encoder.ExtensionDefaultEncoder", func() {
				ext, err := flusher.ctx.GetExtension(flusher.Encoder.Type, flusher.Encoder.Options)
				So(err, ShouldBeNil)

				enc, ok := ext.(extensions.Encoder)
				So(ok, ShouldBeTrue)

				defEnc, ok := enc.(*defaultencoder.ExtensionDefaultEncoder)
				So(ok, ShouldBeTrue)

				Convey("encoder should be *prometheus.Encoder", func() {
					promEnc, ok := defEnc.Encoder.(*prometheus.Encoder)
					So(ok, ShouldBeTrue)

					Convey("series limit should be default series limit", func() {
						So(promEnc.SeriesLimit, ShouldEqual, defaultSeriesLimit)
					})
				})
			})
		})
	})
}

// 场景：Prometheus指标数据 写 RemoteStorage
// 因子：数据模型V2
// 因子：正确的数据
// 因子：Basic Auth鉴权方案
// 因子：相同 labelsets 的 Sample 不聚合（i.e. []prompb.Sample 就 1 条记录）
// 因子：同一个 *models.PipelineGroupEvents，其 tags 相同（i.e. 仅一组tags）
// 预期：写成功
// PS：
// 1. “相同 labelsets 的 Sample 不聚合”的原因：从实际使用场景看，一般每 30s 才抓一次点，所以一般时间戳只会有1个
// 2. “同一个 *models.PipelineGroupEvents”，从实际使用场景看，一般有 1至多组 tags，这里先考虑 仅1组tags 的情况
func TestPrometheusFlusher_ShouldWriteToRemoteStorageSuccess_GivenCorrectDataWithV2Model_OnlyOneGroupOfTags(t *testing.T) {
	Convey("Given correct data with []*models.PipelineGroupEvents type", t, func() {
		var actualWriteRequests []*prompb.WriteRequest
		endpoint := "http://localhost:9090/write"
		expectedUsername, expectedPassword := "user", "password"

		httpmock.Activate()
		defer httpmock.DeactivateAndReset()

		httpmock.RegisterResponder("POST", endpoint, func(req *http.Request) (*http.Response, error) {
			username, password, err := parseBasicAuth(req.Header.Get("Authorization"))
			if err != nil {
				return httpmock.NewStringResponse(http.StatusUnauthorized, "Invalid Authorization"), fmt.Errorf("invalid authentication: %w", err)
			}

			if username != expectedUsername {
				return httpmock.NewStringResponse(http.StatusUnauthorized, "Invalid Username"), fmt.Errorf("invalid username: %s", username)
			}
			if password != expectedPassword {
				return httpmock.NewStringResponse(http.StatusForbidden, "Invalid Password"), fmt.Errorf("invalid password: %s", password)
			}

			if !validateHTTPHeader(req.Header) {
				return httpmock.NewStringResponse(http.StatusBadRequest, "Invalid HTTP Header"), fmt.Errorf("invalid http header: %+v", req.Header)
			}

			wr, err := parsePromWriteRequest(req.Body)
			if err != nil {
				return httpmock.NewStringResponse(http.StatusBadRequest, "Invalid Write Request"), fmt.Errorf("parse prometheus write request error: %w", err)
			}

			actualWriteRequests = append(actualWriteRequests, wr)

			return httpmock.NewStringResponse(http.StatusOK, "ok"), nil
		})

		flusher := &FlusherPrometheus{
			config: config{
				Endpoint:    endpoint,
				SeriesLimit: 1024,
			},
			FlusherHTTP: hf.NewHTTPFlusher(),
		}
		flusher.Concurrency = 3
		flusher.Authenticator = &extensions.ExtensionConfig{
			Type: "ext_basicauth",
			Options: map[string]any{
				"Username": expectedUsername,
				"Password": expectedPassword,
			},
		}

		err := flusher.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		mockMetadata := models.NewMetadata()
		mockTags := models.NewTags()

		Convey("Export data", func() {
			groupEventsSlice := []*models.PipelineGroupEvents{
				{
					Group: models.NewGroup(mockMetadata, mockTags),
					Events: []models.PipelineEvent{
						models.NewSingleValueMetric("test_metric", models.MetricTypeGauge, models.NewTagsWithKeyValues("label1", "value1"), 1234567890*1e6, 1.23),
						models.NewSingleValueMetric("test_metric", models.MetricTypeGauge, models.NewTagsWithKeyValues("label1", "value1"), 1234567891*1e6, 2.34),
					},
				},
				{
					Group: models.NewGroup(mockMetadata, mockTags),
					Events: []models.PipelineEvent{
						models.NewSingleValueMetric("test_metric", models.MetricTypeGauge, models.NewTagsWithKeyValues("label2", "value2"), 1234567890*1e6, 1.23),
						models.NewSingleValueMetric("test_metric", models.MetricTypeGauge, models.NewTagsWithKeyValues("label2", "value2"), 1234567891*1e6, 2.34),
					},
				},
			}

			// expected prometheus metric samples:
			// `test_metric{label1="value1"} 1.23 1234567890`,
			// `test_metric{label1="value1"} 2.34 1234567891`,
			// `test_metric{label2="value2"} 1.23 1234567890`,
			// `test_metric{label2="value2"} 2.34 1234567891`,
			expectedWriteRequest := []*prompb.WriteRequest{
				{
					Timeseries: []prompb.TimeSeries{
						{
							Labels: []prompb.Label{
								{Name: "label1", Value: "value1"},
								{Name: "__name__", Value: "test_metric"},
							},
							Samples: []prompb.Sample{
								{Value: 1.23, Timestamp: 1234567890},
							},
						},
						{
							Labels: []prompb.Label{
								{Name: "label1", Value: "value1"},
								{Name: "__name__", Value: "test_metric"},
							},
							Samples: []prompb.Sample{
								{Value: 2.34, Timestamp: 1234567891},
							},
						},
					},
				},
				{
					Timeseries: []prompb.TimeSeries{
						{
							Labels: []prompb.Label{
								{Name: "label2", Value: "value2"},
								{Name: "__name__", Value: "test_metric"},
							},
							Samples: []prompb.Sample{
								{Value: 1.23, Timestamp: 1234567890},
							},
						},
						{
							Labels: []prompb.Label{
								{Name: "label2", Value: "value2"},
								{Name: "__name__", Value: "test_metric"},
							},
							Samples: []prompb.Sample{
								{Value: 2.34, Timestamp: 1234567891},
							},
						},
					},
				},
			}
			expectedWriteRequest = sortPromLabelsInWriteRequest(expectedWriteRequest)
			httpmock.ZeroCallCounters()
			err := flusher.Export(groupEventsSlice, nil)
			So(err, ShouldBeNil)

			err = flusher.Stop()
			So(err, ShouldBeNil)

			Convey("each GroupEvents should send in a single request", func() {
				So(httpmock.GetTotalCallCount(), ShouldEqual, 2)
			})
			Convey("request body should be valid", func() {
				sort.Sort(promWriteRequest(actualWriteRequests))
				actualWriteRequests = sortPromLabelsInWriteRequest(actualWriteRequests) // ensure that the slice order is consistent with expected
				So(actualWriteRequests, ShouldResemble, expectedWriteRequest)
			})
		})
	})
}

// 场景：Prometheus指标数据 写 RemoteStorage
// 因子：数据模型V2
// 因子：正确的数据
// 因子：Basic Auth鉴权方案
// 因子：相同 labelsets 的 Sample 不聚合（i.e. []prompb.Sample 就 1 条记录）
// 因子：同一个 *models.PipelineGroupEvents，其 tags 不完全相同（i.e. 有多组tags）
// 预期：写成功
// PS：
// 1. “相同 labelsets 的 Sample 不聚合”的原因：从实际使用场景看，一般每 30s 才抓一次点，所以一般时间戳只会有1个
// 2. “同一个 *models.PipelineGroupEvents”，从实际使用场景看，一般有 1至多组 tags，这里考虑 多组tags 的情况
func TestPrometheusFlusher_ShouldWriteToRemoteStorageSuccess_GivenCorrectDataWithV2Model_MultiGroupsOfTags(t *testing.T) {
	Convey("Given correct data with []*models.PipelineGroupEvents type", t, func() {
		var actualWriteRequests []*prompb.WriteRequest
		endpoint := "http://localhost:9090/write"
		expectedUsername, expectedPassword := "user", "password"

		httpmock.Activate()
		defer httpmock.DeactivateAndReset()

		httpmock.RegisterResponder("POST", endpoint, func(req *http.Request) (*http.Response, error) {
			username, password, err := parseBasicAuth(req.Header.Get("Authorization"))
			if err != nil {
				return httpmock.NewStringResponse(http.StatusUnauthorized, "Invalid Authorization"), fmt.Errorf("invalid authentication: %w", err)
			}

			if username != expectedUsername {
				return httpmock.NewStringResponse(http.StatusUnauthorized, "Invalid Username"), fmt.Errorf("invalid username: %s", username)
			}
			if password != expectedPassword {
				return httpmock.NewStringResponse(http.StatusForbidden, "Invalid Password"), fmt.Errorf("invalid password: %s", password)
			}

			if !validateHTTPHeader(req.Header) {
				return httpmock.NewStringResponse(http.StatusBadRequest, "Invalid HTTP Header"), fmt.Errorf("invalid http header: %+v", req.Header)
			}

			wr, err := parsePromWriteRequest(req.Body)
			if err != nil {
				return httpmock.NewStringResponse(http.StatusBadRequest, "Invalid Write Request"), fmt.Errorf("parse prometheus write request error: %w", err)
			}

			actualWriteRequests = append(actualWriteRequests, wr)

			return httpmock.NewStringResponse(http.StatusOK, "ok"), nil
		})

		flusher := &FlusherPrometheus{
			config: config{
				Endpoint:    endpoint,
				SeriesLimit: 1024,
			},
			FlusherHTTP: hf.NewHTTPFlusher(),
		}
		flusher.Concurrency = 3
		flusher.Authenticator = &extensions.ExtensionConfig{
			Type: "ext_basicauth",
			Options: map[string]any{
				"Username": expectedUsername,
				"Password": expectedPassword,
			},
		}

		err := flusher.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		mockMetadata := models.NewMetadata()
		mockTags := models.NewTags()

		Convey("Export data", func() {
			groupEventsSlice := []*models.PipelineGroupEvents{
				{
					Group: models.NewGroup(mockMetadata, mockTags),
					Events: []models.PipelineEvent{
						models.NewSingleValueMetric("test_metric", models.MetricTypeGauge, models.NewTagsWithKeyValues("label1", "value1"), 1234567890*1e6, 1.23),
						models.NewSingleValueMetric("test_metric", models.MetricTypeGauge, models.NewTagsWithKeyValues("label1", "value1"), 1234567891*1e6, 2.34),
						models.NewSingleValueMetric("test_metric", models.MetricTypeGauge, models.NewTagsWithKeyValues("label2", "value2"), 1234567890*1e6, 1.23),
						models.NewSingleValueMetric("test_metric", models.MetricTypeGauge, models.NewTagsWithKeyValues("label2", "value2"), 1234567891*1e6, 2.34),
					},
				},
			}

			// expected prometheus metric samples:
			// `test_metric{label1="value1"} 1.23 1234567890`,
			// `test_metric{label1="value1"} 2.34 1234567891`,
			// `test_metric{label2="value2"} 1.23 1234567890`,
			// `test_metric{label2="value2"} 2.34 1234567891`,
			expectedWriteRequest := []*prompb.WriteRequest{
				{
					Timeseries: []prompb.TimeSeries{
						{
							Labels: []prompb.Label{
								{Name: "label1", Value: "value1"},
								{Name: "__name__", Value: "test_metric"},
							},
							Samples: []prompb.Sample{
								{Value: 1.23, Timestamp: 1234567890},
							},
						},
						{
							Labels: []prompb.Label{
								{Name: "label1", Value: "value1"},
								{Name: "__name__", Value: "test_metric"},
							},
							Samples: []prompb.Sample{
								{Value: 2.34, Timestamp: 1234567891},
							},
						},
						{
							Labels: []prompb.Label{
								{Name: "label2", Value: "value2"},
								{Name: "__name__", Value: "test_metric"},
							},
							Samples: []prompb.Sample{
								{Value: 1.23, Timestamp: 1234567890},
							},
						},
						{
							Labels: []prompb.Label{
								{Name: "label2", Value: "value2"},
								{Name: "__name__", Value: "test_metric"},
							},
							Samples: []prompb.Sample{
								{Value: 2.34, Timestamp: 1234567891},
							},
						},
					},
				},
			}
			expectedWriteRequest = sortPromLabelsInWriteRequest(expectedWriteRequest)
			httpmock.ZeroCallCounters()
			err := flusher.Export(groupEventsSlice, nil)
			So(err, ShouldBeNil)

			err = flusher.Stop()
			So(err, ShouldBeNil)

			Convey("each GroupEvents should send in a single request", func() {
				So(httpmock.GetTotalCallCount(), ShouldEqual, 1)
			})
			Convey("request body should be valid", func() {
				So(actualWriteRequests, ShouldResemble, expectedWriteRequest)
			})
		})
	})
}

// 场景：Prometheus指标数据 写 RemoteStorage
// 因子：数据模型V2
// 因子：错误的数据
// 因子：空指针/零值/非Metric数据
// 因子：Basic Auth鉴权方案
// 因子：相同 labelsets 的 Sample 不聚合（i.e. []prompb.Sample 就 1 条记录）
// 因子：同一个 *models.PipelineGroupEvents，其 tags 不完全相同（i.e. 有多组tags）
// 预期：不会向 RemoteStorage 发起写请求
// PS：
// 1. “相同 labelsets 的 Sample 不聚合”的原因：从实际使用场景看，一般每 30s 才抓一次点，所以一般时间戳只会有1个
// 2. “同一个 *models.PipelineGroupEvents”，从实际使用场景看，一般有 1至多组 tags，这里考虑 多组tags 的情况
func TestPrometheusFlusher_ShouldNotWriteToRemoteStorage_GivenIncorrectDataWithV2Model_MultiGroupsOfTags(t *testing.T) {
	Convey("Given incorrect data with []*models.PipelineGroupEvents type", t, func() {
		var actualWriteRequests []*prompb.WriteRequest
		endpoint := "http://localhost:9090/write"
		expectedUsername, expectedPassword := "user", "password"

		httpmock.Activate()
		defer httpmock.DeactivateAndReset()

		httpmock.RegisterResponder("POST", endpoint, func(req *http.Request) (*http.Response, error) {
			username, password, err := parseBasicAuth(req.Header.Get("Authorization"))
			if err != nil {
				return httpmock.NewStringResponse(http.StatusUnauthorized, "Invalid Authorization"), fmt.Errorf("invalid authentication: %w", err)
			}

			if username != expectedUsername {
				return httpmock.NewStringResponse(http.StatusUnauthorized, "Invalid Username"), fmt.Errorf("invalid username: %s", username)
			}
			if password != expectedPassword {
				return httpmock.NewStringResponse(http.StatusForbidden, "Invalid Password"), fmt.Errorf("invalid password: %s", password)
			}

			if !validateHTTPHeader(req.Header) {
				return httpmock.NewStringResponse(http.StatusBadRequest, "Invalid HTTP Header"), fmt.Errorf("invalid http header: %+v", req.Header)
			}

			wr, err := parsePromWriteRequest(req.Body)
			if err != nil {
				return httpmock.NewStringResponse(http.StatusBadRequest, "Invalid Write Request"), fmt.Errorf("parse prometheus write request error: %w", err)
			}

			actualWriteRequests = append(actualWriteRequests, wr)

			return httpmock.NewStringResponse(http.StatusOK, "ok"), nil
		})

		flusher := &FlusherPrometheus{
			config: config{
				Endpoint:    endpoint,
				SeriesLimit: 1024,
			},
			FlusherHTTP: hf.NewHTTPFlusher(),
		}
		flusher.Concurrency = 3
		flusher.Authenticator = &extensions.ExtensionConfig{
			Type: "ext_basicauth",
			Options: map[string]any{
				"Username": expectedUsername,
				"Password": expectedPassword,
			},
		}

		err := flusher.Init(mock.NewEmptyContext("p", "l", "c"))
		So(err, ShouldBeNil)

		mockMetadata := models.NewMetadata()
		mockTags := models.NewTags()

		Convey("Export data", func() {
			incorrectGroupEventsSlice := []*models.PipelineGroupEvents{
				nil,
				{},
				{
					Group:  models.NewGroup(mockMetadata, mockTags),
					Events: []models.PipelineEvent{},
				},
				{
					Group: models.NewGroup(mockMetadata, mockTags),
					Events: []models.PipelineEvent{
						nil,
						models.NewSimpleLog(nil, nil, 0),
					},
				},
			}

			var expectedWriteRequest []*prompb.WriteRequest
			httpmock.ZeroCallCounters()
			err := flusher.Export(incorrectGroupEventsSlice, nil)
			So(err, ShouldBeNil)

			err = flusher.Stop()
			So(err, ShouldBeNil)

			Convey("each GroupEvents should send in a single request", func() {
				So(httpmock.GetTotalCallCount(), ShouldEqual, 0)
			})
			Convey("request body should be valid", func() {
				So(actualWriteRequests, ShouldResemble, expectedWriteRequest)
			})
		})
	})
}

func parseBasicAuth(auth string) (string, string, error) {
	if !strings.HasPrefix(auth, "Basic ") {
		return "", "", fmt.Errorf("invalid authentication: %s", auth)
	}

	decodedBytes, err := base64.StdEncoding.DecodeString(strings.TrimPrefix(auth, "Basic "))
	if err != nil {
		return "", "", fmt.Errorf("base64 decode error: %w", err)
	}
	authBytes := bytes.Split(decodedBytes, []byte(":"))
	if len(authBytes) != 2 {
		return "", "", fmt.Errorf("invalid auth parts: %d", len(authBytes))
	}

	username, password := string(authBytes[0]), string(authBytes[1])
	return username, password, nil
}

func validateHTTPHeader(header http.Header) bool {
	if header == nil {
		return false
	}

	return header.Get(headerKeyUserAgent) == headerValUserAgent &&
		header.Get(headerKeyContentType) == headerValContentType &&
		header.Get(headerKeyContentEncoding) == headerValContentEncoding &&
		header.Get(headerKeyPromRemoteWriteVersion) == headerValPromRemoteWriteVersion
}

func parsePromWriteRequest(r io.Reader) (*prompb.WriteRequest, error) {
	compressedData, err := io.ReadAll(r)
	if err != nil {
		return nil, fmt.Errorf("failed to read request body: %w", err)
	}

	data, err := snappy.Decode(nil, compressedData)
	if err != nil {
		return nil, fmt.Errorf("failed to decode compressed data: %w", err)
	}

	wr := new(prompb.WriteRequest)
	if err := wr.Unmarshal(data); err != nil {
		return nil, fmt.Errorf("failed to unmarshal prometheus write request: %w", err)
	}

	return wr, nil
}

type promWriteRequest []*prompb.WriteRequest

func (p promWriteRequest) Len() int {
	return len(p)
}

func (p promWriteRequest) Less(i, j int) bool {
	labelI := p[i].Timeseries[0].Labels[0]
	if p[i].Timeseries[0].Labels[0].Name == "__name__" {
		labelI = p[i].Timeseries[0].Labels[1]
	}

	labelJ := p[j].Timeseries[0].Labels[0]
	if p[j].Timeseries[0].Labels[0].Name == "__name__" {
		labelJ = p[j].Timeseries[0].Labels[1]
	}

	return labelI.Name < labelJ.Name
}

func (p promWriteRequest) Swap(i, j int) {
	p[i], p[j] = p[j], p[i]
}

type promLabels []prompb.Label

func (p promLabels) Len() int {
	return len(p)
}

func (p promLabels) Less(i, j int) bool {
	return p[i].Name < p[j].Name
}

func (p promLabels) Swap(i, j int) {
	p[i], p[j] = p[j], p[i]
}

func sortPromLabelsInWriteRequest(req []*prompb.WriteRequest) []*prompb.WriteRequest {
	res := make([]*prompb.WriteRequest, 0, len(req))
	for _, w := range req {
		for i := range w.Timeseries {
			sort.Sort(promLabels(w.Timeseries[i].Labels))
		}
		res = append(res, w)
	}

	return res
}
