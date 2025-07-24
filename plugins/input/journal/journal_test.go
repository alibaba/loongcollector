package journal

import (
	"fmt"
	"sync"
	"testing"
	"time"

	"net/http"
	_ "net/http/pprof"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/coreos/go-systemd/sdjournal"
)

type mockLog struct {
	tags    map[string]string
	fields  map[string]string
	t       time.Time
	isValid bool
}

type mockJournalCollector struct {
	lock  sync.Mutex
	logCh chan *protocol.Log
}

func (c *mockJournalCollector) Init() {
	c.logCh = make(chan *protocol.Log, 10)
}
func (c *mockJournalCollector) AddData(
	tags map[string]string, fields map[string]string, t ...time.Time) {
	c.AddDataWithContext(tags, fields, nil, t...)
}

func (c *mockJournalCollector) AddDataArray(
	tags map[string]string, columns []string, values []string, t ...time.Time) {
	c.AddDataArrayWithContext(tags, columns, values, nil, t...)
}

func (c *mockJournalCollector) AddRawLog(log *protocol.Log) {
	c.AddRawLogWithContext(log, nil)
}

func (c *mockJournalCollector) AddDataWithContext(tags map[string]string, fields map[string]string, ctx map[string]interface{}, t ...time.Time) {
	c.lock.Lock()
	defer c.lock.Unlock()
	slsLog, _ := helper.CreateLog(t[0], true, tags, tags, fields)
	c.logCh <- slsLog
}

func (c *mockJournalCollector) AddDataArrayWithContext(
	tags map[string]string, columns []string, values []string, ctx map[string]interface{}, t ...time.Time) {
	var logTime time.Time
	if len(t) == 0 {
		logTime = time.Now()
	} else {
		logTime = t[0]
	}
	slsLog, _ := helper.CreateLogByArray(logTime, len(t) != 0, tags, tags, columns, values)

	c.logCh <- slsLog
}

func (c *mockJournalCollector) AddRawLogWithContext(log *protocol.Log, ctx map[string]interface{}) {

}

func handleData(data *protocol.Log) {
	//fmt.Println(data)
}

func TestSystemdJournal(t *testing.T) {

	// go func() {
	// 	// 启动一个 goroutine, 不阻止正常代码运行
	// 	http.ListenAndServe("localhost:6060", nil) // 使用 pprof 监听端口
	// }()
	go func() {
		// pprof 服务器，将暴露在 6060 端口
		if err := http.ListenAndServe(":6061", nil); err != nil {
			panic(err)
		}
	}()

	stopCh := make(chan struct{})
	var wg sync.WaitGroup
	sj := &ServiceJournal{
		SeekPosition:        "tail",
		CursorFlushPeriodMs: 5000,
		CursorSeekFallback:  "tail",
		Units:               []string{}, //"log-gererate.service","log-generator-1.service"
		Kernel:              true,
		Identifiers:         []string{},
		JournalPaths:        []string{"/var/log/journal/"}, //"/var/log/journal/"
		MatchPatterns:       []string{},
		ParseSyslogFacility: true,
		ParsePriority:       true,
		UseJournalEventTime: true,
		ResetIntervalSecond: 600000, //ms

		journal:        &sdjournal.Journal{},
		lastSaveCPTime: time.Now(),
		lastCPCursor:   "",
		shutdown:       make(chan struct{}),
		waitGroup:      wg,
	}

	var context helper.LocalContext
	var collector mockJournalCollector
	collector.Init()

	context.InitContext("yili1667-1-heyuan", "aaa-test", "yili-journal")

	go func() {
		defer fmt.Println("STOP JOURNAL")
		sj.Init(&context)
		sj.Start(&collector)
	}()

	go func() {
		defer fmt.Println("STOP COLLECTOR")
	Loop:
		for {
			select {
			case data := <-collector.logCh:
				//fmt.Println(data)
				handleData(data)
			case <-stopCh:
				fmt.Println("STOP RECEIVED")
				break Loop
			}
		}

	}()

	time.Sleep(6000 * time.Second)
	sj.Stop()
	stopCh <- struct{}{}

}
