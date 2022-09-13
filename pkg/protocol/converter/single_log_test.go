// Copyright 2022 iLogtail Authors
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

package protocol

import (
	"encoding/json"
	"testing"

	. "github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestConvertToSimple(t *testing.T) {
	Convey("Given a converter with protocol: single, encoding: json, and no tag rename and protocol key rename", t, func() {
		c, err := NewConverter("single", "json", nil, nil)
		So(err, ShouldBeNil)

		Convey("When the logGroup is generated from host environment", func() {
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

			Convey("Then the converted log should be valid", func() {
				b, err := c.Do(logGroup)
				So(err, ShouldBeNil)

				for _, s := range b {
					unmarshaledLog := make(map[string]interface{})
					err = json.Unmarshal(s, &unmarshaledLog)
					So(err, ShouldBeNil)
					So(unmarshaledLog, ShouldHaveLength, 3)
					So(unmarshaledLog, ShouldContainKey, "time")
					So(unmarshaledLog, ShouldContainKey, "contents")
					So(unmarshaledLog, ShouldContainKey, "tags")
					So(unmarshaledLog["contents"], ShouldHaveLength, 2)
					So(unmarshaledLog["contents"], ShouldContainKey, "method")
					So(unmarshaledLog["contents"], ShouldContainKey, "status")
					So(unmarshaledLog["tags"], ShouldHaveLength, 4)
					So(unmarshaledLog["tags"], ShouldContainKey, "log.file.path")
					So(unmarshaledLog["tags"], ShouldContainKey, "host.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "host.ip")
					So(unmarshaledLog["tags"], ShouldContainKey, "log.topic")
				}
			})
		})

		Convey("When the logGroup is generated from docker environment", func() {
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
						{Key: "__tag__:_container_name_", Value: "container"},
						{Key: "__tag__:_container_ip_", Value: "172.10.0.45"},
						{Key: "__tag__:_image_name_", Value: "image"},
						{Key: "__log_topic__", Value: "file"},
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

			Convey("Then the converted log should be valid", func() {
				b, err := c.Do(logGroup)
				So(err, ShouldBeNil)

				for _, s := range b {
					unmarshaledLog := make(map[string]interface{})
					err = json.Unmarshal(s, &unmarshaledLog)
					So(err, ShouldBeNil)
					So(unmarshaledLog, ShouldHaveLength, 3)
					So(unmarshaledLog, ShouldContainKey, "time")
					So(unmarshaledLog, ShouldContainKey, "contents")
					So(unmarshaledLog, ShouldContainKey, "tags")
					So(unmarshaledLog["contents"], ShouldHaveLength, 2)
					So(unmarshaledLog["contents"], ShouldContainKey, "method")
					So(unmarshaledLog["contents"], ShouldContainKey, "status")
					So(unmarshaledLog["tags"], ShouldHaveLength, 7)
					So(unmarshaledLog["tags"], ShouldContainKey, "log.file.path")
					So(unmarshaledLog["tags"], ShouldContainKey, "host.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "host.ip")
					So(unmarshaledLog["tags"], ShouldContainKey, "log.topic")
					So(unmarshaledLog["tags"], ShouldContainKey, "container.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "container.ip")
					So(unmarshaledLog["tags"], ShouldContainKey, "container.image.name")
				}
			})
		})

		Convey("When the logGroup is generated from k8s daemonset environment", func() {
			*inK8s = true
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
			}
			logGroup := &protocol.LogGroup{
				Logs:     logs,
				Category: "test",
				Topic:    "file",
				Source:   "172.10.0.56",
				LogTags:  tags,
			}

			Convey("Then the converted log should be valid", func() {
				b, err := c.Do(logGroup)
				So(err, ShouldBeNil)

				for _, s := range b {
					unmarshaledLog := make(map[string]interface{})
					err = json.Unmarshal(s, &unmarshaledLog)
					So(err, ShouldBeNil)
					So(unmarshaledLog, ShouldHaveLength, 3)
					So(unmarshaledLog, ShouldContainKey, "time")
					So(unmarshaledLog, ShouldContainKey, "contents")
					So(unmarshaledLog, ShouldContainKey, "tags")
					So(unmarshaledLog["contents"], ShouldHaveLength, 2)
					So(unmarshaledLog["contents"], ShouldContainKey, "method")
					So(unmarshaledLog["contents"], ShouldContainKey, "status")
					So(unmarshaledLog["tags"], ShouldHaveLength, 13)
					So(unmarshaledLog["tags"], ShouldContainKey, "log.file.path")
					So(unmarshaledLog["tags"], ShouldContainKey, "host.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "host.ip")
					So(unmarshaledLog["tags"], ShouldContainKey, "log.topic")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.node.ip")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.node.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.namespace.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.pod.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.pod.uid")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.container.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.container.ip")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.container.image.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "label")
				}
			})
		})

		Convey("When the logGroup is generated from k8s sidecar environment", func() {
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
						{Key: "__tag__:_pod_ip_", Value: "172.10.0.45"},
						{Key: "__log_topic__", Value: "file"},
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

			Convey("Then the converted log should be valid", func() {
				b, err := c.Do(logGroup)
				So(err, ShouldBeNil)

				for _, s := range b {
					unmarshaledLog := make(map[string]interface{})
					err = json.Unmarshal(s, &unmarshaledLog)
					So(err, ShouldBeNil)
					So(unmarshaledLog, ShouldHaveLength, 3)
					So(unmarshaledLog, ShouldContainKey, "time")
					So(unmarshaledLog, ShouldContainKey, "contents")
					So(unmarshaledLog, ShouldContainKey, "tags")
					So(unmarshaledLog["contents"], ShouldHaveLength, 2)
					So(unmarshaledLog["contents"], ShouldContainKey, "method")
					So(unmarshaledLog["contents"], ShouldContainKey, "status")
					So(unmarshaledLog["tags"], ShouldHaveLength, 9)
					So(unmarshaledLog["tags"], ShouldContainKey, "log.file.path")
					So(unmarshaledLog["tags"], ShouldContainKey, "host.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "host.ip")
					So(unmarshaledLog["tags"], ShouldContainKey, "log.topic")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.node.ip")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.node.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.namespace.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.pod.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.pod.ip")
				}
			})
		})

		Convey("When the topic is null but __log_topic__ is not", func() {
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
				Topic:    "",
				Source:   "172.10.0.56",
				LogTags:  tags,
			}

			Convey("Then the converted log should be valid", func() {
				b, err := c.Do(logGroup)
				So(err, ShouldBeNil)

				for _, s := range b {
					unmarshaledLog := make(map[string]interface{})
					err = json.Unmarshal(s, &unmarshaledLog)
					So(err, ShouldBeNil)
					So(unmarshaledLog, ShouldHaveLength, 3)
					So(unmarshaledLog, ShouldContainKey, "time")
					So(unmarshaledLog, ShouldContainKey, "contents")
					So(unmarshaledLog, ShouldContainKey, "tags")
					So(unmarshaledLog["contents"], ShouldHaveLength, 2)
					So(unmarshaledLog["contents"], ShouldContainKey, "method")
					So(unmarshaledLog["contents"], ShouldContainKey, "status")
					So(unmarshaledLog["tags"], ShouldHaveLength, 4)
					So(unmarshaledLog["tags"], ShouldContainKey, "log.file.path")
					So(unmarshaledLog["tags"], ShouldContainKey, "host.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "host.ip")
					So(unmarshaledLog["tags"], ShouldContainKey, "log.topic")
				}
			})
		})

		Convey("When the topic and __log_topic__ are null", func() {
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
				Topic:    "",
				Source:   "172.10.0.56",
				LogTags:  tags,
			}

			Convey("Then the converted log should be valid", func() {
				b, err := c.Do(logGroup)
				So(err, ShouldBeNil)

				for _, s := range b {
					unmarshaledLog := make(map[string]interface{})
					err = json.Unmarshal(s, &unmarshaledLog)
					So(err, ShouldBeNil)
					So(unmarshaledLog, ShouldHaveLength, 3)
					So(unmarshaledLog, ShouldContainKey, "time")
					So(unmarshaledLog, ShouldContainKey, "contents")
					So(unmarshaledLog, ShouldContainKey, "tags")
					So(unmarshaledLog["contents"], ShouldHaveLength, 2)
					So(unmarshaledLog["contents"], ShouldContainKey, "method")
					So(unmarshaledLog["contents"], ShouldContainKey, "status")
					So(unmarshaledLog["tags"], ShouldHaveLength, 3)
					So(unmarshaledLog["tags"], ShouldContainKey, "log.file.path")
					So(unmarshaledLog["tags"], ShouldContainKey, "host.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "host.ip")
					So(unmarshaledLog["tags"], ShouldNotContainKey, "log.topic")
				}
			})
		})

		Convey("When the log is standardized", func() {
			*inK8s = true
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
				b, err := c.Do(logGroup)
				So(err, ShouldBeNil)

				for _, s := range b {
					unmarshaledLog := make(map[string]interface{})
					err = json.Unmarshal(s, &unmarshaledLog)
					So(err, ShouldBeNil)
					So(unmarshaledLog, ShouldHaveLength, 3)
					So(unmarshaledLog, ShouldContainKey, "time")
					So(unmarshaledLog, ShouldContainKey, "contents")
					So(unmarshaledLog, ShouldContainKey, "tags")
					So(unmarshaledLog["contents"], ShouldHaveLength, 2)
					So(unmarshaledLog["contents"], ShouldContainKey, "method")
					So(unmarshaledLog["contents"], ShouldContainKey, "status")
					So(unmarshaledLog["tags"], ShouldHaveLength, 13)
					So(unmarshaledLog["tags"], ShouldContainKey, "log.file.path")
					So(unmarshaledLog["tags"], ShouldContainKey, "host.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "host.ip")
					So(unmarshaledLog["tags"], ShouldContainKey, "log.topic")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.node.ip")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.node.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.namespace.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.pod.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.pod.uid")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.container.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.container.ip")
					So(unmarshaledLog["tags"], ShouldContainKey, "k8s.container.image.name")
					So(unmarshaledLog["tags"], ShouldContainKey, "label")
				}
			})
		})
	})

	Convey("Given a converter with protocol: single, encoding: json, with tag rename and protocol key rename", t, func() {
		keyRenameMap := map[string]string{
			"k8s.node.ip": "ip",
			"host.name":   "hostname",
			"label":       "tag",
			"env":         "env_tag",
		}
		protocolKeyRenameMap := map[string]string{
			"time":     "@timestamp",
			"contents": "values",
			"tags":     "annos",
		}
		c, err := NewConverter("single", "json", keyRenameMap, protocolKeyRenameMap)
		So(err, ShouldBeNil)

		Convey("When the logGroup is generated from k8s daemonset environment", func() {
			*inK8s = true
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
				b, err := c.Do(logGroup)
				So(err, ShouldBeNil)

				for _, s := range b {
					unmarshaledLog := make(map[string]interface{})
					err = json.Unmarshal(s, &unmarshaledLog)
					So(err, ShouldBeNil)
					So(unmarshaledLog, ShouldHaveLength, 3)
					So(unmarshaledLog, ShouldContainKey, "@timestamp")
					So(unmarshaledLog, ShouldContainKey, "values")
					So(unmarshaledLog, ShouldContainKey, "annos")
					So(unmarshaledLog["values"], ShouldHaveLength, 2)
					So(unmarshaledLog["values"], ShouldContainKey, "method")
					So(unmarshaledLog["values"], ShouldContainKey, "status")
					So(unmarshaledLog["annos"], ShouldHaveLength, 14)
					So(unmarshaledLog["annos"], ShouldContainKey, "log.file.path")
					So(unmarshaledLog["annos"], ShouldContainKey, "hostname")
					So(unmarshaledLog["annos"], ShouldContainKey, "host.ip")
					So(unmarshaledLog["annos"], ShouldContainKey, "log.topic")
					So(unmarshaledLog["annos"], ShouldContainKey, "ip")
					So(unmarshaledLog["annos"], ShouldContainKey, "k8s.node.name")
					So(unmarshaledLog["annos"], ShouldContainKey, "k8s.namespace.name")
					So(unmarshaledLog["annos"], ShouldContainKey, "k8s.pod.name")
					So(unmarshaledLog["annos"], ShouldContainKey, "k8s.pod.uid")
					So(unmarshaledLog["annos"], ShouldContainKey, "k8s.container.name")
					So(unmarshaledLog["annos"], ShouldContainKey, "k8s.container.ip")
					So(unmarshaledLog["annos"], ShouldContainKey, "k8s.container.image.name")
					So(unmarshaledLog["annos"], ShouldContainKey, "tag")
					So(unmarshaledLog["annos"], ShouldContainKey, "env_tag")
				}
			})

			Convey("Then the corresponding value of the required fields are returned correctly", func() {
				_, values, err := c.DoWithSelectedFields(logGroup, []string{"content.method", "tag.host.name", "tag.ip", "content.unknown"})
				So(err, ShouldBeNil)
				So(values, ShouldHaveLength, 2)
				So(values[0], ShouldHaveLength, 4)
				So(values[0][0], ShouldEqual, "PUT")
				So(values[0][1], ShouldEqual, "alje834hgf")
				So(values[0][2], ShouldEqual, "172.10.1.19")
				So(values[0][3], ShouldBeEmpty)
				So(values[1], ShouldHaveLength, 4)
				So(values[1][0], ShouldEqual, "GET")
				So(values[1][1], ShouldEqual, "alje834hgf")
				So(values[1][2], ShouldEqual, "172.10.1.19")
				So(values[1][3], ShouldBeEmpty)
			})

			Convey("Then error should be returned given invalid target field", func() {
				_, _, err := c.DoWithSelectedFields(logGroup, []string{"values.method"})
				So(err, ShouldNotBeNil)
			})
		})
	})

	Convey("When constructing converter with unsupported encoding", t, func() {
		_, err := NewConverter("single", "pb", nil, nil)

		Convey("Then error should be returned", func() {
			So(err, ShouldNotBeNil)
		})
	})

	Convey("Given a converter with unsupported encoding", t, func() {
		c := &Converter{
			Protocol: "single",
			Encoding: "pb",
		}

		Convey("When performing conversion", func() {
			logs := []*protocol.Log{
				{
					Time: 1662434209,
					Contents: []*protocol.Log_Content{
						{Key: "method", Value: "PUT"},
					},
				},
			}
			tags := []*protocol.LogTag{
				{Key: "__hostname__", Value: "alje834hgf"},
				{Key: "__pack_id__", Value: "AEDCFGHNJUIOPLMN-1E"},
				{Key: "__path__", Value: "/root/test/origin/example.log"},
				{Key: "__log_topic__", Value: "file"},
			}
			logGroup := &protocol.LogGroup{
				Logs:     logs,
				Category: "test",
				Topic:    "topic",
				Source:   "172.10.0.56",
				LogTags:  tags,
			}

			_, err := c.Do(logGroup)

			Convey("Then error should be returned", func() {
				So(err, ShouldNotBeNil)
			})
		})
	})
}
