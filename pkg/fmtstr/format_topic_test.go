package fmtstr

import (
	"testing"

	"github.com/smartystreets/goconvey/convey"
)

func TestFormatTopic(t *testing.T) {
	convey.Convey("Kafka topic format  ", t, func() {
		targetValues := map[string]string{
			"app": "ilogtail",
		}
		kafkaTopic := "test_%{app}"
		actualTopic, _ := FormatTopic(targetValues, kafkaTopic)
		desiredTopic := "test_ilogtail"
		convey.So(desiredTopic, convey.ShouldEqual, *actualTopic)
	})
}
