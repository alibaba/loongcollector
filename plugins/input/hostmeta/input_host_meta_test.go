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

package hostmeta

import (
	"encoding/json"
	"fmt"
	"os"
	"strings"
	"testing"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"github.com/shirou/gopsutil/process"
)

func TestInputNodeMeta_Collect(t *testing.T) {
	currentPid := os.Getpid()
	newProcess, err := process.NewProcess(int32(currentPid))
	if err != nil {
		t.Errorf("cannot get current process proc: %v", err)
	}
	cmd, err := newProcess.Cmdline()
	if err != nil {
		t.Errorf("cannot get current process cmd: %v", err)
	}
	regexps := []string{
		"^" + strings.Split(cmd, " ")[0] + ".*",
	}
	type args struct {
		CPU     bool
		Memory  bool
		Net     bool
		Disk    bool
		Process bool
	}
	tests := []struct {
		name              string
		args              args
		wantNodeType      string
		wantNodeNum       int
		wantAttributeType string
		wantAttributesNum int
		wantLabelNum      int
	}{
		{"off_all", args{false, false, false, false, false}, Host, 0, "", 0, 12},
		{"open_cpu", args{true, false, false, false, false}, Host, 1, CPU, 8, 12},
		{"open_mem", args{false, true, false, false, false}, Host, 1, Memory, 3, 12},
		{"open_net", args{false, false, true, false, false}, Host, 1, Net, 6, 12},
		{"open_disk", args{false, false, false, true, false}, Host, 1, Disk, 4, 12},
		{"open_process", args{false, false, false, false, true}, Process, 1, Process, 5, 2},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			ctx := mock.NewEmptyContext("project", "store", "config")
			p := pipeline.MetricInputs[pluginType]().(*InputNodeMeta)
			c := new(test.MockMetricCollector)
			p.Disk = tt.args.Disk
			p.CPU = tt.args.CPU
			p.Memory = tt.args.Memory
			p.Net = tt.args.Net
			p.Process = tt.args.Process
			p.ProcessNamesRegex = regexps
			if _, err := p.Init(ctx); err != nil {
				t.Errorf("cannot init meta node plugin: %v", err)
				return
			}
			if err := p.Collect(c); err != nil {
				t.Errorf("Collect() error = %v", err)
				return
			} else if len(c.Logs) != tt.wantNodeNum {
				t.Errorf("Collect() want collect %d logs, but got %d", tt.wantNodeNum, len(c.Logs))
				return
			}
			for _, log := range c.Logs {
				contents := log.Contents
				metaLogItemNum := 5
				if len(contents) != metaLogItemNum {
					t.Errorf("Meta log contents should have %d items ,but got %d", metaLogItemNum, len(contents))
					return
				}
				convert2Map := func(str string) (res map[string]interface{}, err error) {
					res = make(map[string]interface{})
					err = json.Unmarshal([]byte(str), &res)
					return
				}
				for _, content := range contents {
					switch content.Key {
					case "id":
						if content.Value == "" {
							t.Error("meta log id must not be null")
						}
					case "labels":
						m, _ := convert2Map(content.Value)
						if len(m) != tt.wantLabelNum {
							t.Errorf("meta log host labels want %d num, but got %d", tt.wantLabelNum, len(m))
						}
					case "type":
						if content.Value != tt.wantNodeType {
							t.Errorf("meta log type want %s, but got %s", tt.wantNodeType, content.Value)
						}
					case "attributes":
						res, err := convert2Map(content.Value)
						if err != nil {
							t.Errorf("deserialize error: %v", err)
							return
						}
						if tt.wantAttributeType == "" && len(res) == 0 {
							return
						}
						if tt.wantAttributeType == Process {
							if len(res) != tt.wantAttributesNum {
								t.Errorf("meta log arrtributes want %d num, but got %d", tt.wantAttributesNum, len(res))
							}
							continue
						}
						wantMeta, ok := res[tt.wantAttributeType]
						if !ok {
							t.Errorf("cannot find %s attributes", tt.wantAttributeType)

						}
						switch meta := wantMeta.(type) {
						case []interface{}:
							m := meta[0].(map[string]interface{})
							if len(m) != tt.wantAttributesNum {
								t.Errorf("meta log arrtributes want %d num, but got %d", tt.wantAttributesNum, len(m))
							}
						case map[string]interface{}:
							if len(meta) != tt.wantAttributesNum {
								t.Errorf("meta log arrtributes want %d num, but got %d", tt.wantAttributesNum, len(meta))
							}
						default:
							t.Errorf("the attributes pattern is error: %v", content.Value)
						}
					}
				}
				fmt.Printf("got:%s\n", log.String())
			}
		})
	}
}

func Test_formatCmd(t *testing.T) {
	const specialFlag = "@"
	expendStr := func(str string) string {
		res := ""
		for i := 0; i < 4000; i++ {
			res += str
		}
		res += "@"
		for i := 0; i < 4000; i++ {
			res += str
		}
		return res
	}

	tests := []struct {
		name    string
		cmd     string
		checker func(string) bool
	}{
		{
			"below threshold",
			"12345",
			func(str string) bool {
				return str == "12345"
			},
		},
		{
			"over threshold",
			expendStr("1"),
			func(s string) bool {
				return !strings.Contains(s, specialFlag)
			},
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := formatCmd(tt.cmd); !tt.checker(got) {
				t.Errorf("formatCmd() = %v", got)
			}
		})
	}
}
