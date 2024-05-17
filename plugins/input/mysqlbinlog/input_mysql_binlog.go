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

package mysqlbinlog

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strconv"

	"github.com/go-mysql-org/go-mysql/replication"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
)

type CheckPoint struct {
	IndexFileDir   string
	BinlogFileName string
	Offset         int
}

type InputMysqlBinlog struct {
	IndexFilePath      string
	FromBegining       bool
	AutoMap            bool
	MaxSendEventPerSec int
	IntervalMs         int
	RowMode            bool

	checkpoint   CheckPoint
	context      pipeline.Context
	parser       *replication.BinlogParser
	collector    pipeline.Collector
	rotateFile   string
	collectCount int
}

func (b *InputMysqlBinlog) getBinLogPath(indexDir, binlogFilename string) string {
	if len(binlogFilename) > 2 && (binlogFilename[1] == ':' || binlogFilename[0] == '/') {
		return binlogFilename
	}
	if len(binlogFilename) > 2 && binlogFilename[0] == '.' && (binlogFilename[1] == '/' || binlogFilename[1] == '\\') {
		return indexDir + "/" + binlogFilename[2:]
	}
	return indexDir + "/" + binlogFilename
}

func (b *InputMysqlBinlog) skipToEndFile(path string) (int, error) {
	f, err := os.Open(filepath.Clean(path))
	if err != nil {
		return 0, err
	}
	defer func(f *os.File) {
		_ = f.Close()
	}(f)

	buf := make([]byte, 4)
	if _, err = f.Read(buf); err != nil {
		return 0, err
	} else if !bytes.Equal(buf, replication.BinLogFileHeader) {
		return 0, fmt.Errorf("%s is not a valid binlog file, head 4 bytes must fe'bin' ", path)
	}

	offset := 4

	headBuf := make([]byte, replication.EventHeaderSize)
	var h replication.EventHeader

	for {
		if _, err = io.ReadFull(f, headBuf); err == io.EOF {
			return offset, nil
		} else if err != nil {
			return 0, err
		}
		err := h.Decode(headBuf)
		if err != nil {
			return 0, err
		}

		if h.EventSize <= uint32(replication.EventHeaderSize) {
			return 0, fmt.Errorf("invalid event header, event size is %d, too small", h.EventSize)
		}

		rst, err := f.Seek(int64(h.EventSize)-int64(replication.EventHeaderSize), os.SEEK_CUR)

		if err != nil || rst != int64(h.EventSize)+int64(offset) {
			return offset, nil
		}
	}
}

func (b *InputMysqlBinlog) findOffset(fromBegining bool) error {
	dir, filename := util.SplitPath(b.IndexFilePath)
	if len(dir) == 0 || len(filename) == 0 {
		return fmt.Errorf("invalid binlog index file path:%s", b.IndexFilePath)
	}
	b.checkpoint.IndexFileDir = dir
	nameList, err := util.ReadLines(b.IndexFilePath)
	if err != nil {
		return err
	}
	if len(nameList) == 0 {
		return nil
	}
	if fromBegining {
		binlogFile := nameList[0]
		b.checkpoint.BinlogFileName = b.getBinLogPath(dir, binlogFile)
		b.checkpoint.Offset = 0
		return nil
	}
	binlogFile := nameList[len(nameList)-1]
	b.checkpoint.BinlogFileName = b.getBinLogPath(dir, binlogFile)
	offset, err := b.skipToEndFile(b.checkpoint.BinlogFileName)
	if err != nil {
		return err
	}
	b.checkpoint.Offset = offset
	return nil
}

func (b *InputMysqlBinlog) Init(context pipeline.Context) (int, error) {
	b.context = context
	b.parser = replication.NewBinlogParser()
	// if b.RowMode {
	// 	b.parser.SetRawMode(true)
	// }
	if b.IntervalMs < 1 {
		b.IntervalMs = 3000
	}
	data, exist := b.context.GetCheckPoint("mysql_bin_log")
	if data != nil && exist {
		if err := json.Unmarshal(data, &b.checkpoint); err != nil {
			logger.Error(b.context.GetRuntimeContext(), "INIT_CHECKPOINT_ALARM", "load checkpoint error", err)
			b.checkpoint.BinlogFileName = ""
			b.checkpoint.IndexFileDir = ""
			b.checkpoint.Offset = 0
		}
	}

	return b.IntervalMs, nil
}

func (b *InputMysqlBinlog) Description() string {
	return "mysql binlog input plugin for logtail"
}

// BinlogEventToLog is a callback for replication.parser.ParseFile to accept bin log events.
func (b *InputMysqlBinlog) BinlogEventToLog(event *replication.BinlogEvent) error {
	if event == nil {
		return nil
	}

	if b.checkpoint.Offset == 0 {
		b.checkpoint.Offset = 4
	}

	valueMap := map[string]string{
		"__file__":       b.checkpoint.BinlogFileName,
		"__offset__":     strconv.Itoa(b.checkpoint.Offset),
		"__event__":      event.Header.EventType.String(),
		"__event_size__": strconv.Itoa(int(event.Header.EventSize)),
	}

	err := binLogEventToSlsLog(event, valueMap, b.collector)
	if err != nil {
		return err
	}

	b.checkpoint.Offset += int(event.Header.EventSize)
	b.collectCount++
	if event.Header.EventType == replication.ROTATE_EVENT {
		rotateEvent, cvt := event.Event.(*replication.RotateEvent)
		if cvt {
			b.rotateFile = string(rotateEvent.NextLogName)
			b.checkpoint.Offset = int(rotateEvent.Position)

			logger.Info(b.context.GetRuntimeContext(), "process rotate, last file", b.checkpoint.BinlogFileName, "now file", b.rotateFile)
			b.checkpoint.BinlogFileName = b.rotateFile
		}
	}
	return nil
}

func (b *InputMysqlBinlog) collectOneFile(collector pipeline.Collector) error {
	b.collector = collector
	b.rotateFile = ""
	b.collectCount = 0
	filePath := b.getBinLogPath(b.checkpoint.IndexFileDir, b.checkpoint.BinlogFileName)
	if err := b.parser.ParseFile(filePath, int64(b.checkpoint.Offset), b.BinlogEventToLog); err != nil {
		return err
	}
	if b.collectCount > 0 {
		logger.Debug(b.context.GetRuntimeContext(), "collect end, binlog", filePath, "count", b.collectCount)
		cpBytes, err := json.Marshal(&b.checkpoint)
		if err != nil {
			return err
		}
		if err := b.context.SaveCheckPoint("mysql_bin_log", cpBytes); err != nil {
			return err
		}
	}
	return nil
}

func (b *InputMysqlBinlog) Collect(collector pipeline.Collector) error {
	if b.checkpoint.Offset == 0 {
		err := b.findOffset(b.FromBegining)
		if err != nil {
			return err
		}
	}

	err := b.collectOneFile(collector)

	return err
}

func init() {
	pipeline.MetricInputs["metric_binlog"] = func() pipeline.MetricInput {
		return &InputMysqlBinlog{AutoMap: true, RowMode: true}
	}
}
