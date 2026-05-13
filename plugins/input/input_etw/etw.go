//go:build windows
// +build windows

package input_etw

import (
	"encoding/json"
	"fmt"
	"os"
	"strconv"
	"strings"
	"sync"

	"github.com/bi-zone/etw"
	"golang.org/x/sys/windows"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

const pluginType = "service_etw"

type KeywordMask uint64

func (k *KeywordMask) UnmarshalJSON(data []byte) error {
	var numeric uint64
	if err := json.Unmarshal(data, &numeric); err == nil {
		*k = KeywordMask(numeric)
		return nil
	}

	var text string
	if err := json.Unmarshal(data, &text); err != nil {
		return fmt.Errorf("unmarshal Keywords: %w", err)
	}
	text = strings.TrimSpace(text)
	if text == "" {
		*k = 0
		return nil
	}
	parsed, err := strconv.ParseUint(text, 0, 64)
	if err != nil {
		return fmt.Errorf("invalid Keywords %q: %w", text, err)
	}
	*k = KeywordMask(parsed)
	return nil
}

type EtwInput struct {
	ProviderName string
	ProviderGUID string
	Level        int
	Keywords     KeywordMask

	parsedGUID windows.GUID
	session    *etw.Session
	context    pipeline.Context
	collector  pipeline.Collector
	hostname   string
	serverIP   string
	mu         sync.Mutex
	stopped    bool
}

func (d *EtwInput) Init(context pipeline.Context) (int, error) {
	d.context = context
	d.hostname, _ = os.Hostname()

	var guidStr string
	if d.ProviderName != "" {
		resolved, err := resolveProviderName(d.ProviderName)
		if err != nil {
			return 0, fmt.Errorf("resolve ProviderName %q: %w", d.ProviderName, err)
		}
		guidStr = resolved
		logger.Infof(d.context.GetRuntimeContext(),
			"resolved ProviderName %q to GUID %s", d.ProviderName, guidStr)
	} else if d.ProviderGUID != "" {
		guidStr = d.ProviderGUID
	} else {
		return 0, fmt.Errorf("either ProviderName or ProviderGUID must be specified")
	}

	guid, err := windows.GUIDFromString(guidStr)
	if err != nil {
		return 0, fmt.Errorf("invalid GUID %q: %w", guidStr, err)
	}
	d.parsedGUID = guid

	if d.Level == 0 {
		d.Level = 4
	}

	logger.Infof(d.context.GetRuntimeContext(),
		"service_etw init: guid=%s level=%d keywords=0x%X",
		guidStr, d.Level, uint64(d.Keywords))
	return 0, nil
}

func (d *EtwInput) Description() string {
	return "Generic ETW (Event Tracing for Windows) real-time event consumer"
}

func (d *EtwInput) Start(collector pipeline.Collector) error {
	d.collector = collector

	d.mu.Lock()
	if d.stopped {
		d.mu.Unlock()
		return nil
	}
	session, err := etw.NewSession(d.parsedGUID,
		etw.WithLevel(etw.TraceLevel(d.Level)),
		etw.WithMatchKeywords(uint64(d.Keywords), 0))
	if err != nil {
		d.mu.Unlock()
		return fmt.Errorf("create ETW session: %w", err)
	}
	d.session = session
	d.mu.Unlock()

	cb := func(e *etw.Event) { d.handleEvent(e) }
	if err := session.Process(cb); err != nil {
		d.mu.Lock()
		stopped := d.stopped
		d.mu.Unlock()
		if stopped {
			return nil
		}
		logger.Warningf(d.context.GetRuntimeContext(),
			"ETW_ALARM", "session.Process error: %v", err)
		return err
	}
	return nil
}

func (d *EtwInput) Stop() error {
	d.mu.Lock()
	d.stopped = true
	session := d.session
	d.mu.Unlock()
	if session != nil {
		_ = session.Close()
	}
	return nil
}

func (d *EtwInput) handleEvent(e *etw.Event) {
	data, err := e.EventProperties()
	if err != nil {
		return
	}

	eventID := e.Header.EventDescriptor.ID
	fields := make(map[string]string, len(data)+20)
	fields["event_id"] = strconv.FormatUint(uint64(eventID), 10)
	fields["opcode"] = strconv.FormatUint(uint64(e.Header.EventDescriptor.OpCode), 10)
	fields["level"] = strconv.FormatUint(uint64(e.Header.EventDescriptor.Level), 10)
	fields["keywords"] = fmt.Sprintf("0x%X", e.Header.EventDescriptor.Keyword)
	fields["process_id"] = strconv.FormatUint(uint64(e.Header.ProcessID), 10)
	fields["thread_id"] = strconv.FormatUint(uint64(e.Header.ThreadID), 10)

	for k, v := range data {
		fields[k] = fmt.Sprintf("%v", v)
	}

	if d.isDNSProvider() {
		d.enrichDNSFields(fields, eventID)
	}

	tags := map[string]string{"source": "etw"}
	d.collector.AddData(tags, fields, e.Header.TimeStamp)
}

func init() {
	pipeline.ServiceInputs[pluginType] = func() pipeline.ServiceInput {
		return &EtwInput{}
	}
}
