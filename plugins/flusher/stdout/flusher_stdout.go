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

package stdout

import (
	"encoding/json"
	"fmt"
	"strconv"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"github.com/cihub/seelog"
	jsoniter "github.com/json-iterator/go"
)

const flushMsg = `
<seelog minlevel="info" >
<outputs formatid="common">
	 %s
 </outputs>
 <formats>
	 <format id="common" format="%%Date %%Time %%Msg%%n" />
 </formats>
</seelog>
`

// FlusherStdout is a flusher plugin in plugin system.
// It has two usages:
// 1. flusher the logGroup to the stdout
// 2. flusher the logGroup to the file. If the specified file name is not configured,
// the logGroups would append to the global log file.
type FlusherStdout struct {
	FileName      string
	MaxSize       int
	MaxRolls      int
	KeyValuePairs bool
	Tags          bool
	OnlyStdout    bool

	context   ilogtail.Context
	outLogger seelog.LoggerInterface
}

// Init method would be trigger before working. For the plugin, init method choose the log output
// channel.
func (p *FlusherStdout) Init(context ilogtail.Context) error {
	p.context = context

	pattern := ""
	if p.OnlyStdout {
		pattern = "<console/>"
		logger.CloseCatchStdout()
	} else if p.FileName != "" {
		pattern = `<rollingfile type="size" filename="%s" maxsize="%d" maxrolls="%d"/>`
		if p.MaxSize <= 0 {
			p.MaxSize = 1024 * 1024
		}
		if p.MaxRolls <= 0 {
			p.MaxRolls = 1
		}
		pattern = fmt.Sprintf(pattern, p.FileName, p.MaxSize, p.MaxRolls)
	}
	if pattern != "" {
		var err error
		p.outLogger, err = seelog.LoggerFromConfigAsString(fmt.Sprintf(flushMsg, pattern))
		if err != nil {
			logger.Error(p.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init stdout flusher fail, error", err)
			p.outLogger = seelog.Disabled
		}
	}
	return nil
}

func (*FlusherStdout) Description() string {
	return "stdout flusher for logtail"
}

// Flush the logGroup list to stdout or files.
func (p *FlusherStdout) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		if p.Tags {
			if p.outLogger != nil {
				p.outLogger.Infof("[LogGroup] topic %s, logstore %s, logcount %d, tags %v", logGroup.Topic, logGroup.Category, len(logGroup.Logs), logGroup.LogTags)
			} else {
				logger.Info(p.context.GetRuntimeContext(), "[LogGroup] topic", logGroup.Topic, "logstore", logGroup.Category, "logcount", len(logGroup.Logs), "tags", logGroup.LogTags)
			}
		}

		if p.KeyValuePairs {
			for _, log := range logGroup.Logs {
				writer := jsoniter.NewStream(jsoniter.ConfigDefault, nil, 128)
				writer.WriteObjectStart()
				for _, c := range log.Contents {
					writer.WriteObjectField(c.Key)
					writer.WriteString(c.Value)
					_, _ = writer.Write([]byte{','})
				}
				writer.WriteObjectField("__time__")
				writer.WriteString(strconv.Itoa(int(log.Time)))
				writer.WriteObjectEnd()

				if p.outLogger != nil {
					p.outLogger.Infof("%s", writer.Buffer())
				} else {
					logger.Info(p.context.GetRuntimeContext(), string(writer.Buffer()))
				}
			}

		} else {
			for _, log := range logGroup.Logs {
				buf, _ := json.Marshal(log)
				if p.outLogger != nil {
					p.outLogger.Infof("%s", buf)
				} else {
					logger.Info(p.context.GetRuntimeContext(), string(buf))
				}
			}
		}

	}
	return nil
}

func (p *FlusherStdout) SetUrgent(flag bool) {
}

// IsReady is ready to flush
func (*FlusherStdout) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return true
}

// Stop ...
func (p *FlusherStdout) Stop() error {
	if p.outLogger != nil {
		p.outLogger.Close()
	}
	return nil
}

// Register the plugin to the Flushers array.
func init() {
	ilogtail.Flushers["flusher_stdout"] = func() ilogtail.Flusher {
		return &FlusherStdout{
			KeyValuePairs: true,
		}
	}
}
