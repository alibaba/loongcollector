package helper

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"path"
	"strconv"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/pkg/util"

	"github.com/stretchr/testify/require"
)

func TestServiceHTTP_doDumpFile(t *testing.T) {
	_, err := os.Stat(path.Join(util.GetCurrentBinaryPath(), "dump"))
	if err == nil {
		files, findErr := GetFileListByPrefix(path.Join(util.GetCurrentBinaryPath(), "dump"), "custom", true, 0)
		require.NoError(t, findErr)
		for _, file := range files {
			_ = os.Remove(file)

		}
	}

	var ch chan *DumpData

	insertFun := func(num int, start int) {
		for i := start; i < start+num; i++ {
			m := map[string][]string{
				"header": {strconv.Itoa(i)},
			}
			ch <- &DumpData{
				Req: DumpDataReq{
					Body:   []byte(fmt.Sprintf("body_%d", i)),
					URL:    fmt.Sprintf("url_%d", i),
					Header: m,
				},
			}

		}
	}
	readFunc := func(file string, expectLen int) {
		data, rerr := ioutil.ReadFile(file)
		require.NoError(t, rerr)
		offset := 0
		num := 0
		for {
			if offset == len(data) {
				break
			}
			var length uint32
			buffer := bytes.NewBuffer(data[offset:])
			require.NoError(t, binary.Read(buffer, binary.BigEndian, &length))

			data := data[offset+4 : offset+4+int(length)] //nolint: govet
			var d DumpData
			require.NoError(t, json.Unmarshal(data, &d))

			require.Equal(t, fmt.Sprintf("url_%d", num), d.Req.URL)
			require.Equal(t, fmt.Sprintf("body_%d", num), string(d.Req.Body))
			require.Equal(t, len(d.Req.Header), 1)
			require.Equal(t, strconv.Itoa(num), d.Req.Header["header"][0])
			offset = offset + 4 + int(length)
			num++
		}
		require.Equal(t, num, expectLen)
	}

	// test dump and read
	s := NewDumper("custom", 3)
	s.Init()
	ch = s.InputChannel()
	s.Start()
	insertFun(100, 0)
	time.Sleep(time.Millisecond * 100)
	s.Close()
	readFunc(s.dumpDataKeepFiles[len(s.dumpDataKeepFiles)-1], 100)

	// append
	s2 := NewDumper("custom", 3)
	s2.Init()
	ch = s2.InputChannel()
	s2.Start()
	insertFun(100, 100)
	time.Sleep(time.Millisecond * 100)
	s2.Close()
	readFunc(s.dumpDataKeepFiles[len(s.dumpDataKeepFiles)-1], 200)
}
