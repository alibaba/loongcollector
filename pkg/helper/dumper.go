// Copyright 2023 iLogtail Authors
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

package helper

import (
	"bytes"
	"context"
	"encoding/binary"
	"encoding/json"
	"io"
	"os"
	"path"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/helper/async"
	"github.com/alibaba/ilogtail/pkg/logger"
)

type DumpDataReq struct {
	Body   []byte
	URL    string
	Header map[string][]string
}
type DumpDataResp struct {
	Body   []byte
	Header map[string]string
}

// DumpData current only for http protocol
type DumpData struct {
	Req  DumpDataReq
	Resp DumpDataResp
}

type Dumper struct {
	input             chan *DumpData
	dumpDataKeepFiles []string
	prefix            string
	maxKeepFiles      int
	wg                sync.WaitGroup
	stop              chan struct{}
	// unittest
	writeCounter *async.UnitTestCounterHelper
}

func (d *Dumper) Init() {
	// 只有 service_http_server 插件会使用这个模块
	_ = os.MkdirAll(path.Join(config.LoongcollectorGlobalConfig.LoongcollectorDebugDir, "dump"), 0750)
	d.input = make(chan *DumpData, 10)
	d.stop = make(chan struct{})
	files, err := GetFileListByPrefix(path.Join(config.LoongcollectorGlobalConfig.LoongcollectorDebugDir, "dump"), d.prefix, true, 0)
	if err != nil {
		logger.Warning(context.Background(), "LIST_HISTORY_DUMP_ALARM", "err", err)
	} else {
		d.dumpDataKeepFiles = files
	}

}

func (d *Dumper) Start() {
	if d == nil {
		return
	}
	d.wg.Add(1)
	go func() {
		defer d.wg.Done()
		d.doDumpFile()
	}()
}

func (d *Dumper) Close() {
	close(d.stop)
	d.wg.Wait()
}

func (d *Dumper) doDumpFile() {
	var err error
	fileName := d.prefix + ".dump"
	logger.Info(context.Background(), "http server start dump data", fileName)
	var f *os.File
	closeFile := func() {
		if f != nil {
			_ = f.Close()
		}
	}
	cutFile := func() (f *os.File, err error) {
		nFile := path.Join(path.Join(config.LoongcollectorGlobalConfig.LoongcollectorDebugDir, "dump"), fileName+"_"+time.Now().Format("2006-01-02_15"))
		if len(d.dumpDataKeepFiles) == 0 || d.dumpDataKeepFiles[len(d.dumpDataKeepFiles)-1] != nFile {
			d.dumpDataKeepFiles = append(d.dumpDataKeepFiles, nFile)
		}
		if len(d.dumpDataKeepFiles) > d.maxKeepFiles {
			rmLen := len(d.dumpDataKeepFiles) - d.maxKeepFiles
			for i := 0; i < rmLen; i++ {
				_ = os.Remove(d.dumpDataKeepFiles[i])
			}
			d.dumpDataKeepFiles = d.dumpDataKeepFiles[rmLen:]
		}
		closeFile()
		return os.OpenFile(d.dumpDataKeepFiles[len(d.dumpDataKeepFiles)-1], os.O_CREATE|os.O_WRONLY, 0600)
	}
	lastHour := 0
	offset := int64(0)
	for {
		select {
		case data := <-d.input:
			if time.Now().Hour() != lastHour {
				file, cerr := cutFile()
				if cerr != nil {
					logger.Error(context.Background(), "DUMP_FILE_ALARM", "cut new file error", err)
				} else {
					offset, _ = file.Seek(0, io.SeekEnd)
					f = file
				}
			}
			if f != nil {
				d.AddDelta(1)
				buffer := bytes.NewBuffer([]byte{})
				b, _ := json.Marshal(data)
				if err = binary.Write(buffer, binary.BigEndian, uint32(len(b))); err != nil {
					continue
				}
				if _, err = f.WriteAt(buffer.Bytes(), offset); err != nil {
					continue
				}
				if _, err = f.WriteAt(b, offset+4); err != nil {
					continue
				}
				offset += int64(4 + len(b))
			}
		case <-d.stop:
			return
		}
	}
}

func (d *Dumper) InputChannel() chan *DumpData {
	return d.input
}

func NewDumper(prefix string, maxFiles int) *Dumper {
	return &Dumper{
		prefix:       prefix,
		maxKeepFiles: maxFiles,
	}
}

func (d *Dumper) Begin(callback func()) bool {
	d.writeCounter = new(async.UnitTestCounterHelper)
	d.writeCounter.Begin(callback)
	return true
}

func (d *Dumper) End(timeout time.Duration, expectNum int64) error {
	return d.writeCounter.End(timeout, expectNum)
}

func (d *Dumper) AddDelta(num int64) {
	if d.writeCounter != nil {
		d.writeCounter.AddDelta(num)
	}
}
