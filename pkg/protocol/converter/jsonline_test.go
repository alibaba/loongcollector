package protocol

import (
	"bytes"
	"encoding/json"
	"testing"

	. "github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestNewConvertToJsonlineLogs(t *testing.T) {
	Convey("When constructing converter with unsupported encoding", t, func() {
		_, err := NewConverter(ProtocolJsonline, EncodingNone, nil, nil, &config.GlobalConfig{})
		So(err, ShouldNotBeNil)
	})

	Convey("Given a converter with protocol: single, encoding: json, with tag rename and protocol key rename", t, func() {
		keyRenameMap := map[string]string{
			"k8s.node.ip": "ip",
			"host.name":   "hostname",
			"label":       "tag",
			"env":         "env_tag",
		}
		protocolKeyRenameMap := map[string]string{
			"time": "@timestamp",
		}
		c, err := NewConverter(ProtocolJsonline, EncodingJSON, keyRenameMap, protocolKeyRenameMap, &config.GlobalConfig{})
		So(err, ShouldBeNil)

		Convey("When the logGroup is generated from files and from k8s daemonset environment", func() {
			*flags.K8sFlag = true
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
						{Key: "__tag__:__user_defined_id__", Value: "machine"},
						{Key: "__tag__:__path__", Value: "/root/test/origin/example.log"},
						{Key: "__tag__:_node_name_", Value: "node"},
						{Key: "__tag__:_node_ip_", Value: "172.10.1.19"},
						{Key: "__tag__:_namespace_", Value: "default"},
						{Key: "__tag__:_pod_name_", Value: "container"},
						{Key: "__tag__:_pod_uid_", Value: "12AFERR234SG-SBH6D67HJ9-AAD-VF34"},
						{Key: "__tag__:_container_name_", Value: "container"},
						{Key: "__tag__:_container_ip_", Value: "172.10.0.45"},
						{Key: "__tag__:_image_name_", Value: "image"},
						{Key: "__tag__:label", Value: "tag"},
						{Key: "__log_topic__", Value: "file"},
					},
				}
			}
			tags := []*protocol.LogTag{
				{Key: "__hostname__", Value: "alje834hgf"},
				{Key: "__pack_id__", Value: "AEDCFGHNJUIOPLMN-1E"},
				{Key: "env", Value: "K8S"},
			}
			logGroup := &protocol.LogGroup{
				Logs:     logs,
				Category: "test",
				Topic:    "file",
				Source:   "172.10.0.56",
				LogTags:  tags,
			}

			Convey("Then the converted log should be valid", func() {
				b, err := c.ToByteStream(logGroup)
				So(err, ShouldBeNil)

				for _, s := range bytes.Split(b.([]byte), []byte("\n")) {
					unmarshaledLog := make(map[string]interface{})
					err = json.Unmarshal(s, &unmarshaledLog)
					So(err, ShouldBeNil)
					So(unmarshaledLog, ShouldContainKey, "method")
					So(unmarshaledLog, ShouldContainKey, "@timestamp")
					So(unmarshaledLog, ShouldContainKey, "log.file.path")
					So(unmarshaledLog, ShouldContainKey, "hostname")
					So(unmarshaledLog, ShouldContainKey, "host.ip")
					So(unmarshaledLog, ShouldContainKey, "log.topic")
					So(unmarshaledLog, ShouldContainKey, "ip")
					So(unmarshaledLog, ShouldContainKey, "k8s.node.name")
					So(unmarshaledLog, ShouldContainKey, "k8s.namespace.name")
					So(unmarshaledLog, ShouldContainKey, "k8s.pod.name")
					So(unmarshaledLog, ShouldContainKey, "k8s.pod.uid")
					So(unmarshaledLog, ShouldContainKey, "k8s.container.name")
					So(unmarshaledLog, ShouldContainKey, "k8s.container.ip")
					So(unmarshaledLog, ShouldContainKey, "k8s.container.image.name")
					So(unmarshaledLog, ShouldContainKey, "tag")
					So(unmarshaledLog, ShouldContainKey, "env_tag")
				}
			})

			Convey("Then the corresponding value of the required fields are returned correctly", func() {
				_, values, err := c.ToByteStreamWithSelectedFields(logGroup, []string{"content.method", "tag.host.name", "tag.ip"})
				So(err, ShouldBeNil)
				So(values, ShouldHaveLength, 0)
			})
		})
	})

	Convey("Given a converter with protocol: single, encoding: json, with null tag rename", t, func() {
		keyRenameMap := map[string]string{
			"k8s.node.ip": "",
			"host.name":   "",
			"label":       "",
			"env":         "",
		}
		c, err := NewConverter(ProtocolJsonline, EncodingJSON, keyRenameMap, nil, &config.GlobalConfig{})
		So(err, ShouldBeNil)

		Convey("When the logGroup is generated from files and from k8s daemonset environment", func() {
			*flags.K8sFlag = true
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
						{Key: "__tag__:__user_defined_id__", Value: "machine"},
						{Key: "__tag__:__path__", Value: "/root/test/origin/example.log"},
						{Key: "__tag__:_node_name_", Value: "node"},
						{Key: "__tag__:_node_ip_", Value: "172.10.1.19"},
						{Key: "__tag__:_namespace_", Value: "default"},
						{Key: "__tag__:_pod_name_", Value: "container"},
						{Key: "__tag__:_pod_uid_", Value: "12AFERR234SG-SBH6D67HJ9-AAD-VF34"},
						{Key: "__tag__:_container_name_", Value: "container"},
						{Key: "__tag__:_container_ip_", Value: "172.10.0.45"},
						{Key: "__tag__:_image_name_", Value: "image"},
						{Key: "__tag__:label", Value: "tag"},
						{Key: "__log_topic__", Value: "file"},
					},
				}
			}
			tags := []*protocol.LogTag{
				{Key: "__hostname__", Value: "alje834hgf"},
				{Key: "__pack_id__", Value: "AEDCFGHNJUIOPLMN-1E"},
				{Key: "env", Value: "K8S"},
			}
			logGroup := &protocol.LogGroup{
				Logs:     logs,
				Category: "test",
				Topic:    "file",
				Source:   "172.10.0.56",
				LogTags:  tags,
			}

			Convey("Then the converted log should be valid", func() {
				b, err := c.ToByteStream(logGroup)
				So(err, ShouldBeNil)

				for _, s := range bytes.Split(b.([]byte), []byte("\n")) {
					unmarshaledLog := make(map[string]interface{})
					err = json.Unmarshal(s, &unmarshaledLog)
					So(err, ShouldBeNil)
					So(unmarshaledLog, ShouldContainKey, "time")
					So(unmarshaledLog, ShouldContainKey, "method")
					So(unmarshaledLog, ShouldContainKey, "status")
					So(unmarshaledLog, ShouldContainKey, "log.file.path")
					So(unmarshaledLog, ShouldContainKey, "host.ip")
					So(unmarshaledLog, ShouldContainKey, "log.topic")
					So(unmarshaledLog, ShouldContainKey, "k8s.node.name")
					So(unmarshaledLog, ShouldContainKey, "k8s.namespace.name")
					So(unmarshaledLog, ShouldContainKey, "k8s.pod.name")
					So(unmarshaledLog, ShouldContainKey, "k8s.pod.uid")
					So(unmarshaledLog, ShouldContainKey, "k8s.container.name")
					So(unmarshaledLog, ShouldContainKey, "k8s.container.ip")
					So(unmarshaledLog, ShouldContainKey, "k8s.container.image.name")
				}
			})
		})

		Convey("When the log is standardized", func() {
			*flags.K8sFlag = true
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
					},
				}
			}
			tags := []*protocol.LogTag{
				{Key: "__user_defined_id__", Value: "machine"},
				{Key: "__hostname__", Value: "alje834hgf"},
				{Key: "__pack_id__", Value: "AEDCFGHNJUIOPLMN-1E"},
				{Key: "__path__", Value: "/root/test/origin/example.log"},
				{Key: "_node_name_", Value: "node"},
				{Key: "_node_ip_", Value: "172.10.1.19"},
				{Key: "_namespace_", Value: "default"},
				{Key: "_pod_name_", Value: "container"},
				{Key: "_pod_uid_", Value: "12AFERR234SG-SBH6D67HJ9-AAD-VF34"},
				{Key: "_container_name_", Value: "container"},
				{Key: "_container_ip_", Value: "172.10.0.45"},
				{Key: "_image_name_", Value: "image"},
				{Key: "label", Value: "tag"},
			}
			logGroup := &protocol.LogGroup{
				Logs:     logs,
				Category: "test",
				Topic:    "topic",
				Source:   "172.10.0.56",
				LogTags:  tags,
			}

			Convey("Then the converted log should be valid", func() {
				b, err := c.ToByteStream(logGroup)
				So(err, ShouldBeNil)

				for _, s := range bytes.Split(b.([]byte), []byte("\n")) {
					unmarshaledLog := make(map[string]interface{})
					err = json.Unmarshal(s, &unmarshaledLog)
					So(err, ShouldBeNil)
					So(unmarshaledLog, ShouldContainKey, "time")
					So(unmarshaledLog, ShouldContainKey, "method")
					So(unmarshaledLog, ShouldContainKey, "status")
					So(unmarshaledLog, ShouldContainKey, "log.file.path")
					So(unmarshaledLog, ShouldContainKey, "host.ip")
					So(unmarshaledLog, ShouldContainKey, "log.topic")
					So(unmarshaledLog, ShouldContainKey, "k8s.node.name")
					So(unmarshaledLog, ShouldContainKey, "k8s.namespace.name")
					So(unmarshaledLog, ShouldContainKey, "k8s.pod.name")
					So(unmarshaledLog, ShouldContainKey, "k8s.pod.uid")
					So(unmarshaledLog, ShouldContainKey, "k8s.container.name")
					So(unmarshaledLog, ShouldContainKey, "k8s.container.ip")
					So(unmarshaledLog, ShouldContainKey, "k8s.container.image.name")
				}
			})
		})
	})
}
