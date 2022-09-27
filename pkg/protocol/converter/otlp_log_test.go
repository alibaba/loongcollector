package protocol

import (
	"testing"

	"github.com/smartystreets/goconvey/convey"
	logv1 "go.opentelemetry.io/proto/otlp/logs/v1"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestNewConvertToOtlpLogs(t *testing.T) {
	convey.Convey("When constructing converter with unsupported encoding", t, func() {
		_, err := NewConverter(ProtocolOtlpLogV1, EncodingJSON, nil, nil)
		convey.So(err, convey.ShouldNotBeNil)
	})

	convey.Convey("When constructing converter with supported encoding", t, func() {
		c, err := NewConverter(ProtocolOtlpLogV1, EncodingNone, nil, nil)
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
				Logs:     logs,
				Category: "test",
				Topic:    "file",
				Source:   "172.10.0.56",
				LogTags:  tags,
			}
			d, err := c.Do(logGroup)
			convey.Convey("Then the converted log should be valid", func() {
				convey.So(err, convey.ShouldBeNil)
				resourceLogs, ok := d.(*logv1.ResourceLogs)
				convey.So(ok, convey.ShouldBeTrue)
				convey.So(1, convey.ShouldEqual, len(resourceLogs.ScopeLogs))
				convey.So(5, convey.ShouldEqual, len(resourceLogs.Resource.Attributes))
				convey.Convey("Then the LogRecords should be valid", func() {
					for _, scope := range resourceLogs.ScopeLogs {
						convey.So(2, convey.ShouldEqual, len(scope.LogRecords))
						for i := range scope.LogRecords {
							convey.So(logs[i].Contents[4].Value, convey.ShouldEqual, scope.LogRecords[i].Body.GetStringValue())
						}
					}
				})
			})
		})
	})
}
