package protocol

import (
	"strconv"
	"testing"

	"github.com/smartystreets/goconvey/convey"
	"go.opentelemetry.io/collector/pdata/plog"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/otlp"

	systime "time"
)

func TestNewConvertToOtlpLogs(t *testing.T) {
	convey.Convey("When constructing converter with unsupported encoding", t, func() {
		_, err := NewConverter(ProtocolOtlpV1, EncodingJSON, nil, nil)
		convey.So(err, convey.ShouldNotBeNil)
	})

	convey.Convey("When constructing converter with supported encoding", t, func() {
		c, err := NewConverter(ProtocolOtlpV1, EncodingNone, nil, nil)
		convey.So(err, convey.ShouldBeNil)

		convey.Convey("When the logGroup is generated from files", func() {
			time := []uint32{1662434209, 1662434487}
			method := []string{"PUT", "GET"}
			status := []string{"200", "404"}
			logs := make([]*protocol.Log, 2)
			for i := 0; i < 2; i++ {
				logs[i] = &protocol.Log{
					Time: time[i],
					Contents: []*protocol.Log_Content{
						{Key: "method", Value: method[i]},
						{Key: "status", Value: status[i]},
						{Key: "__tag__:__path__", Value: "/root/test/origin/example.log"},
						{Key: "__log_topic__", Value: "file"},
						{Key: "content", Value: "test log content"},
					},
				}
			}
			tags := []*protocol.LogTag{
				{Key: "__hostname__", Value: "alje834hgf"},
				{Key: "__pack_id__", Value: "AEDCFGHNJUIOPLMN-1E"},
			}
			logGroup := &protocol.LogGroup{
				Logs:        logs,
				Category:    "test",
				Topic:       "file",
				Source:      "172.10.0.56",
				LogTags:     tags,
				MachineUUID: "machine_id",
			}
			d, err := c.Do(logGroup)
			convey.Convey("Then the converted log should be valid", func() {
				convey.So(err, convey.ShouldBeNil)
				resourceLogs, ok := d.(plog.ResourceLogs)
				convey.So(ok, convey.ShouldBeTrue)
				convey.So(1, convey.ShouldEqual, resourceLogs.ScopeLogs().Len())
				convey.So(5, convey.ShouldEqual, resourceLogs.Resource().Attributes().Len())

				convey.Convey("Then the LogRecords should be valid", func() {
					for i := 0; i < resourceLogs.ScopeLogs().Len(); i++ {
						scope := resourceLogs.ScopeLogs().At(i)
						convey.So(2, convey.ShouldEqual, scope.LogRecords().Len())

						for j := 0; j < scope.LogRecords().Len(); j++ {
							logRecord := scope.LogRecords().At(j)
							convey.So(logs[i].Contents[4].Value, convey.ShouldEqual, logRecord.Body().AsString())

							// Convert timestamp to unix seconds and compare with the original time
							unixTimeSec := logRecord.Timestamp().AsTime().Unix()
							convey.So(uint64(logs[j].Time), convey.ShouldEqual, unixTimeSec)
						}
					}
				})
			})
		})
	})

	config.LogtailGlobalConfig.EnableTimestampNanosecond = true
	convey.Convey("When constructing converter with supported encoding", t, func() {
		c, err := NewConverter(ProtocolOtlpV1, EncodingNone, nil, nil)
		convey.So(err, convey.ShouldBeNil)

		convey.Convey("When the logGroup is generated from files", func() {
			time := []uint32{1662434209, 1662434487}
			namosec := systime.Now().Nanosecond()
			timeNs := []uint32{uint32(namosec), uint32(namosec + 10)}
			method := []string{"PUT", "GET"}
			status := []string{"200", "404"}
			logs := make([]*protocol.Log, 2)
			for i := 0; i < 2; i++ {
				logs[i] = &protocol.Log{
					Time: time[i],
					Contents: []*protocol.Log_Content{
						{Key: "method", Value: method[i]},
						{Key: "status", Value: status[i]},
						{Key: "__tag__:__path__", Value: "/root/test/origin/example.log"},
						{Key: "__log_topic__", Value: "file"},
						{Key: "content", Value: "test log content"},
					},
					TimeNs: &timeNs[i],
				}
			}
			tags := []*protocol.LogTag{
				{Key: "__hostname__", Value: "alje834hgf"},
				{Key: "__pack_id__", Value: "AEDCFGHNJUIOPLMN-1E"},
			}
			logGroup := &protocol.LogGroup{
				Logs:        logs,
				Category:    "test",
				Topic:       "file",
				Source:      "172.10.0.56",
				LogTags:     tags,
				MachineUUID: "machine_id",
			}
			d, err := c.Do(logGroup)
			convey.Convey("Then the converted log should be valid", func() {
				convey.So(err, convey.ShouldBeNil)
				resourceLogs, ok := d.(plog.ResourceLogs)
				convey.So(ok, convey.ShouldBeTrue)
				convey.So(1, convey.ShouldEqual, resourceLogs.ScopeLogs().Len())
				convey.So(5, convey.ShouldEqual, resourceLogs.Resource().Attributes().Len())

				convey.Convey("Then the LogRecords should be valid", func() {
					for i := 0; i < resourceLogs.ScopeLogs().Len(); i++ {
						scope := resourceLogs.ScopeLogs().At(i)
						convey.So(2, convey.ShouldEqual, scope.LogRecords().Len())

						for j := 0; j < scope.LogRecords().Len(); j++ {
							logRecord := scope.LogRecords().At(j)
							convey.So(logs[i].Contents[4].Value, convey.ShouldEqual, logRecord.Body().AsString())

							// Convert timestamp to unix nanoseconds and compare with the original timeNs
							unixTimeNano := logRecord.Timestamp().AsTime().UnixNano()
							convey.So(uint64(logs[j].Time)*uint64(systime.Second)+uint64(*logs[j].TimeNs), convey.ShouldEqual, unixTimeNano)
						}
					}
				})
			})
		})
	})
}

func TestConvertPipelineGroupEventsToOtlpLogs(t *testing.T) {
	convey.Convey("When constructing converter with supported encoding", t, func() {
		c, err := NewConverter(ProtocolOtlpV1, EncodingNone, nil, nil)
		convey.So(err, convey.ShouldBeNil)

		convey.Convey("When the logGroup is generated from files", func() {
			time := []uint32{1662434209, 1662434487}
			method := []string{"PUT", "GET"}
			status := []string{"200", "404"}
			pipelineGroupEvent := &models.PipelineGroupEvents{
				Group: &models.GroupInfo{
					Metadata: models.NewMetadata(),
					Tags:     models.NewTags(),
				},
			}

			pipelineGroupEvent.Group.Metadata.Add("__hostname__", "alje834hgf")
			pipelineGroupEvent.Group.Metadata.Add("__pack_id__", "AEDCFGHNJUIOPLMN-1E")
			for i := 0; i < 2; i++ {
				tags := models.NewTagsWithMap(
					map[string]string{
						"method":           method[i],
						"status":           status[i],
						"__tag__:__path__": "/root/test/origin/example.log",
						"__log_topic__":    "file",
						"content":          "test log content",
					},
				)

				event := models.NewLog(
					"log_name_"+strconv.Itoa(i),
					[]byte("message"),
					"INFO",
					"",
					"",
					tags,
					uint64(time[i]),
				)
				event.ObservedTimestamp = uint64(time[i] + 100)
				pipelineGroupEvent.Events = append(pipelineGroupEvent.Events, event)
			}

			logs, metrics, traces, err := c.ConvertPipelineGroupEventsToOTLPEventsV1(pipelineGroupEvent)
			convey.Convey("Then the converted logs should be valid", func() {
				convey.So(err, convey.ShouldBeNil)
				convey.So(1, convey.ShouldEqual, logs.ScopeLogs().Len())
				convey.So(2, convey.ShouldEqual, logs.Resource().Attributes().Len())
				convey.So(0, convey.ShouldEqual, metrics.ScopeMetrics().Len())
				convey.So(0, convey.ShouldEqual, traces.ScopeSpans().Len())

				convey.Convey("Then the logs should be valid", func() {
					for i := 0; i < logs.ScopeLogs().Len(); i++ {
						scope := logs.ScopeLogs().At(i)
						convey.So(2, convey.ShouldEqual, scope.LogRecords().Len())

						for j := 0; j < scope.LogRecords().Len(); j++ {
							log := scope.LogRecords().At(j)
							event := pipelineGroupEvent.Events[j].(*models.Log)
							convey.So(event.GetTimestamp(), convey.ShouldEqual, uint64(log.Timestamp()))
							convey.So(event.GetObservedTimestamp(), convey.ShouldEqual, uint64(log.ObservedTimestamp()))
							convey.So(event.GetBody(), convey.ShouldResemble, log.Body().Bytes().AsRaw())

							for k, v := range pipelineGroupEvent.Events[j].GetTags().Iterator() {
								otTagValue, ok := log.Attributes().Get(k)
								convey.So(ok, convey.ShouldBeTrue)
								convey.So(v, convey.ShouldEqual, otTagValue.AsString())
							}

						}
					}
				})
			})
		})
	})
}

func TestConvertPipelineGroupEventsToOtlpMetrics(t *testing.T) {
	convey.Convey("When constructing converter with supported encoding", t, func() {
		c, err := NewConverter(ProtocolOtlpV1, EncodingNone, nil, nil)
		convey.So(err, convey.ShouldBeNil)

		convey.Convey("When the logGroup is generated from files", func() {
			time := []uint32{1662434209, 1662434487}
			method := []string{"PUT", "GET"}
			status := []string{"200", "404"}
			pipelineGroupEvent := &models.PipelineGroupEvents{
				Group: &models.GroupInfo{
					Metadata: models.NewMetadata(),
					Tags:     models.NewTags(),
				},
			}

			pipelineGroupEvent.Group.Metadata.Add("__hostname__", "alje834hgf")
			pipelineGroupEvent.Group.Metadata.Add("__pack_id__", "AEDCFGHNJUIOPLMN-1E")
			for i := 0; i < 2; i++ {
				tags := models.NewTagsWithMap(
					map[string]string{
						"method":           method[i],
						"status":           status[i],
						"__tag__:__path__": "/root/test/origin/example.log",
						"__log_topic__":    "file",
						"content":          "test log content",
					},
				)

				event := models.NewSingleValueMetric(
					"metric_name_"+strconv.Itoa(i),
					models.MetricTypeCounter,
					tags,
					int64(time[i]),
					i+1,
				)
				pipelineGroupEvent.Events = append(pipelineGroupEvent.Events, event)
			}

			logs, metrics, traces, err := c.ConvertPipelineGroupEventsToOTLPEventsV1(pipelineGroupEvent)
			convey.Convey("Then the converted metrics should be valid", func() {
				convey.So(err, convey.ShouldBeNil)
				convey.So(0, convey.ShouldEqual, logs.ScopeLogs().Len())
				convey.So(0, convey.ShouldEqual, traces.ScopeSpans().Len())
				convey.So(1, convey.ShouldEqual, metrics.ScopeMetrics().Len())
				convey.So(2, convey.ShouldEqual, metrics.Resource().Attributes().Len())

				convey.Convey("Then the metrics should be valid", func() {
					for i := 0; i < metrics.ScopeMetrics().Len(); i++ {
						scope := metrics.ScopeMetrics().At(i)
						convey.So(2, convey.ShouldEqual, scope.Metrics().Len())
						for j := 0; j < scope.Metrics().Len(); j++ {
							metric := scope.Metrics().At(j)
							convey.So("metric_name_"+strconv.Itoa(j), convey.ShouldEqual, metric.Name())
							metricSum := metric.Sum()
							convey.So(1, convey.ShouldEqual, metricSum.DataPoints().Len())

							convey.So(pipelineGroupEvent.Events[j].GetTimestamp(), convey.ShouldEqual, uint64(metricSum.DataPoints().At(0).Timestamp()))
							for k, v := range pipelineGroupEvent.Events[j].GetTags().Iterator() {
								otTagValue, ok := metricSum.DataPoints().At(0).Attributes().Get(k)
								convey.So(ok, convey.ShouldBeTrue)
								convey.So(v, convey.ShouldEqual, otTagValue.AsString())
							}

						}
					}
				})
			})
		})
	})
}

func TestConvertPipelineGroupEventsToOtlpTraces(t *testing.T) {
	convey.Convey("When constructing converter with supported encoding", t, func() {
		c, err := NewConverter(ProtocolOtlpV1, EncodingNone, nil, nil)
		convey.So(err, convey.ShouldBeNil)

		convey.Convey("When the logGroup is generated from files", func() {
			time := []uint32{1662434209, 1662434487}
			method := []string{"PUT", "GET"}
			status := []string{"200", "404"}
			pipelineGroupEvent := &models.PipelineGroupEvents{
				Group: &models.GroupInfo{
					Metadata: models.NewMetadata(),
					Tags:     models.NewTags(),
				},
			}

			pipelineGroupEvent.Group.Metadata.Add("__hostname__", "alje834hgf")
			pipelineGroupEvent.Group.Metadata.Add("__pack_id__", "AEDCFGHNJUIOPLMN-1E")
			pipelineGroupEvent.Group.Tags.Add(otlp.TagKeyScopeName, "scopename")
			pipelineGroupEvent.Group.Tags.Add(otlp.TagKeyScopeVersion, "")
			for i := 0; i < 2; i++ {
				tags := models.NewTagsWithMap(
					map[string]string{
						"method":           method[i],
						"status":           status[i],
						"__tag__:__path__": "/root/test/origin/example.log",
						"__log_topic__":    "file",
						"content":          "test log content",
					},
				)

				event := models.NewSpan(
					"span_name_"+strconv.Itoa(i),
					"",
					"",
					models.SpanKindClient,
					uint64(time[i]),
					uint64(time[i]+100),
					tags,
					nil,
					nil,
				)
				pipelineGroupEvent.Events = append(pipelineGroupEvent.Events, event)
			}

			logs, metrics, traces, err := c.ConvertPipelineGroupEventsToOTLPEventsV1(pipelineGroupEvent)
			convey.Convey("Then the converted traces should be valid", func() {
				convey.So(err, convey.ShouldBeNil)
				convey.So(0, convey.ShouldEqual, logs.ScopeLogs().Len())
				convey.So(0, convey.ShouldEqual, metrics.ScopeMetrics().Len())
				convey.So(1, convey.ShouldEqual, traces.ScopeSpans().Len())
				convey.So(2, convey.ShouldEqual, traces.Resource().Attributes().Len())

				convey.Convey("Then the traces should be valid", func() {
					for i := 0; i < traces.ScopeSpans().Len(); i++ {
						scope := traces.ScopeSpans().At(i)
						convey.So(scope.Scope().Version(), convey.ShouldEqual, "")
						convey.So(scope.Scope().Name(), convey.ShouldEqual, "scopename")
						convey.So(2, convey.ShouldEqual, scope.Spans().Len())

						for j := 0; j < scope.Spans().Len(); j++ {
							span := scope.Spans().At(j)
							convey.So("span_name_"+strconv.Itoa(j), convey.ShouldEqual, span.Name())
							event := pipelineGroupEvent.Events[j].(*models.Span)
							convey.So(event.GetTimestamp(), convey.ShouldEqual, uint64(span.StartTimestamp()))
							convey.So(event.EndTime, convey.ShouldEqual, uint64(span.EndTimestamp()))

							for k, v := range pipelineGroupEvent.Events[j].GetTags().Iterator() {
								otTagValue, ok := span.Attributes().Get(k)
								convey.So(ok, convey.ShouldBeTrue)
								convey.So(v, convey.ShouldEqual, otTagValue.AsString())
							}

						}
					}
				})
			})
		})
	})
}
