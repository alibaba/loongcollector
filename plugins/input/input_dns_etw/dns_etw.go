//go:build windows
// +build windows

package input_dns_etw

import (
	"fmt"
	"strconv"
	"sync"

	"github.com/bi-zone/etw"
	"golang.org/x/sys/windows"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

const pluginType = "service_dns_etw"

// DnsEtwInput consumes DNS Analytical ETW events in real-time.
// The standard Windows Event Log API (EvtSubscribe) cannot read ETW Direct Channel
// events while the session is active. This plugin uses the bi-zone/etw library to
// create a real-time ETW consumer session that captures DNS query/response events.
type DnsEtwInput struct {
	// ProviderGUID is the ETW provider GUID for Microsoft-Windows-DNSServer.
	ProviderGUID string
	// Level is the ETW trace level. 4 = Informational (default).
	Level int
	// Keywords is the ETW keywords bitmask.
	// Default: 0x8000000000000037 = Analytical | QUERY_RECEIVED | RESPONSE_SUCCESS |
	// RESPONSE_FAILURE | RECURSE_QUERY_OUT | RECURSE_RESPONSE_IN
	Keywords uint64

	session   *etw.Session
	context   pipeline.Context
	collector pipeline.Collector
	wg        sync.WaitGroup
	stopOnce  sync.Once
}

func (d *DnsEtwInput) Init(context pipeline.Context) (int, error) {
	d.context = context
	if d.ProviderGUID == "" {
		d.ProviderGUID = "{EB79061A-A566-4698-9119-3ED2807060E7}"
	}
	if d.Level == 0 {
		d.Level = 4
	}
	if d.Keywords == 0 {
		d.Keywords = 0x8000000000000037
	}
	logger.Infof(d.context.GetRuntimeContext(),
		"DnsEtwInput init: provider=%s level=%d keywords=0x%X",
		d.ProviderGUID, d.Level, d.Keywords)
	return 0, nil
}

func (d *DnsEtwInput) Description() string {
	return "DNS Analytical ETW real-time consumer for Windows DNS Server"
}

func (d *DnsEtwInput) Start(collector pipeline.Collector) error {
	d.collector = collector

	guid, err := windows.GUIDFromString(d.ProviderGUID)
	if err != nil {
		return fmt.Errorf("invalid provider GUID %q: %w", d.ProviderGUID, err)
	}

	session, err := etw.NewSession(guid,
		etw.WithLevel(etw.TraceLevel(d.Level)),
		etw.WithMatchKeywords(d.Keywords, 0))
	if err != nil {
		return fmt.Errorf("failed to create ETW session (requires Windows Server 2012+ with DNS Server role and Analytical log enabled): %w", err)
	}
	d.session = session

	logger.Infof(d.context.GetRuntimeContext(),
		"DnsEtwInput started ETW session for %s", d.ProviderGUID)

	cb := func(e *etw.Event) {
		d.handleEvent(e)
	}

	d.wg.Add(1)
	defer d.wg.Done()

	if err := d.session.Process(cb); err != nil {
		logger.Warningf(d.context.GetRuntimeContext(),
			"DNS_ETW_ALARM", "ETW session.Process error: %v", err)
		return err
	}
	return nil
}

func (d *DnsEtwInput) Stop() error {
	d.stopOnce.Do(func() {
		if d.session != nil {
			logger.Infof(d.context.GetRuntimeContext(),
				"DnsEtwInput stopping ETW session")
			_ = d.session.Close()
		}
	})
	d.wg.Wait()
	logger.Infof(d.context.GetRuntimeContext(), "DnsEtwInput stopped")
	return nil
}

func (d *DnsEtwInput) handleEvent(e *etw.Event) {
	data, err := e.EventProperties()
	if err != nil {
		return
	}

	fields := make(map[string]string, len(data)+5)
	fields["event_id"] = strconv.FormatUint(
		uint64(e.Header.EventDescriptor.ID), 10)
	fields["opcode"] = strconv.FormatUint(
		uint64(e.Header.EventDescriptor.OpCode), 10)
	fields["level"] = strconv.FormatUint(
		uint64(e.Header.EventDescriptor.Level), 10)
	fields["keywords"] = fmt.Sprintf("0x%X",
		e.Header.EventDescriptor.Keyword)
	fields["process_id"] = strconv.FormatUint(
		uint64(e.Header.ProcessID), 10)
	fields["thread_id"] = strconv.FormatUint(
		uint64(e.Header.ThreadID), 10)

	for k, v := range data {
		fields[k] = fmt.Sprintf("%v", v)
	}

	tags := map[string]string{
		"source": "dns_etw",
	}

	ts := e.Header.TimeStamp
	d.collector.AddData(tags, fields, ts)
}

func init() {
	pipeline.ServiceInputs[pluginType] = func() pipeline.ServiceInput {
		return &DnsEtwInput{}
	}
}