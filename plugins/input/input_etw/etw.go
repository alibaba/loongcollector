//go:build windows
// +build windows

package input_etw

import (
	"encoding/json"
	"fmt"
	"os"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"
	"unicode"

	"github.com/bi-zone/etw"
	"golang.org/x/sys/windows"
	"golang.org/x/sys/windows/registry"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

const (
	pluginType             = "service_etw"
	etwAlarmType           = selfmonitor.AlarmType("ETW_ALARM")
	maxSessionRetryBackoff = 30 * time.Second
)

var asyncEnqueueTimeout = 5 * time.Second

type etwSession interface {
	Process(etw.EventCallback) error
	Close() error
}

type etwSessionOption = etw.Option

var newEtwSession = func(guid windows.GUID, options ...etwSessionOption) (etwSession, error) {
	return etw.NewSession(guid, options...)
}

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
	Source       string
	Level        int
	Keywords     KeywordMask
	// DNSQueryDomainFilters drops Microsoft-Windows-DNSServer events whose dns_query matches.
	// It supports exact domains and leading wildcard suffixes such as "*.azure.cn".
	DNSQueryDomainFilters []string
	// ParsePacketData controls DNS PacketData parsing. Nil keeps the legacy default: enabled.
	ParsePacketData *bool
	AsyncProcess    bool
	EventQueueSize  int
	WorkerCount     int
	// DropWhenQueueFull drops events when the async queue is full. When false, ETW callbacks block.
	DropWhenQueueFull bool
	BufferSizeKB      uint32
	MinBuffers        uint32
	MaxBuffers        uint32
	FlushTimerSec     uint32

	parsedGUID  windows.GUID
	session     etwSession
	context     pipeline.Context
	collector   pipeline.Collector
	hostname    string
	domain      string
	domainType  string
	osName      string
	osVersion   string
	serverIP    string
	serverIPMu  sync.RWMutex
	collectorMu sync.Mutex
	mu          sync.Mutex
	waitGroup   sync.WaitGroup
	eventCh     chan etwEventData
	workerWG    sync.WaitGroup
	statsStopCh chan struct{}
	stopCh      chan struct{}
	statsWG     sync.WaitGroup
	stopped     bool
	stats       etwStats
}

type etwEventData struct {
	eventID    uint16
	opcode     uint8
	level      uint8
	keywords   uint64
	processID  uint32
	threadID   uint32
	properties map[string]interface{}
	timestamp  time.Time
}

type etwStats struct {
	received             uint64
	enqueued             uint64
	droppedQueueFull     uint64
	droppedDomainFilter  uint64
	packetDataParseError uint64
}

func (d *EtwInput) Init(context pipeline.Context) (int, error) {
	d.context = context
	d.hostname, _ = os.Hostname()
	d.domain, d.domainType = getWindowsDomainInfo()
	d.osName = "Windows"
	d.osVersion = getWindowsOSVersion()
	if d.Source == "" {
		d.Source = "etw"
	}

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
	if d.Level < 1 || d.Level > 5 {
		return 0, fmt.Errorf("invalid Level %d: must be 1..5, or 0 for default", d.Level)
	}
	if d.BufferSizeKB > 0 && d.BufferSizeKB < 4 {
		return 0, fmt.Errorf("invalid BufferSizeKB %d: must be >= 4, or 0 for Windows default", d.BufferSizeKB)
	}
	if d.BufferSizeKB > 4096 {
		return 0, fmt.Errorf("invalid BufferSizeKB %d: must be <= 4096", d.BufferSizeKB)
	}
	if d.MinBuffers > 1024 {
		return 0, fmt.Errorf("invalid MinBuffers %d: must be <= 1024", d.MinBuffers)
	}
	if d.MaxBuffers > 1024 {
		return 0, fmt.Errorf("invalid MaxBuffers %d: must be <= 1024", d.MaxBuffers)
	}
	if d.MinBuffers > 0 && d.MaxBuffers > 0 && d.MinBuffers > d.MaxBuffers {
		return 0, fmt.Errorf("invalid ETW buffers: MinBuffers %d is greater than MaxBuffers %d", d.MinBuffers, d.MaxBuffers)
	}
	if d.FlushTimerSec > 60 {
		return 0, fmt.Errorf("invalid FlushTimerSec %d: must be <= 60", d.FlushTimerSec)
	}
	if d.AsyncProcess {
		if d.EventQueueSize <= 0 {
			d.EventQueueSize = 8192
		}
		if d.EventQueueSize > 1000000 {
			return 0, fmt.Errorf("invalid EventQueueSize %d: must be <= 1000000", d.EventQueueSize)
		}
		if d.WorkerCount <= 0 {
			d.WorkerCount = minInt(runtime.NumCPU(), 2)
		}
		if d.WorkerCount > 64 {
			return 0, fmt.Errorf("invalid WorkerCount %d: must be <= 64", d.WorkerCount)
		}
	}

	logger.Infof(d.context.GetRuntimeContext(),
		"service_etw init: guid=%s level=%d keywords=0x%X async=%t queue=%d workers=%d dropWhenFull=%t parsePacketData=%t bufferSizeKB=%d minBuffers=%d maxBuffers=%d flushTimerSec=%d",
		guidStr, d.Level, uint64(d.Keywords), d.AsyncProcess, d.EventQueueSize, d.WorkerCount, d.DropWhenQueueFull, d.parsePacketDataEnabled(),
		d.BufferSizeKB, d.MinBuffers, d.MaxBuffers, d.FlushTimerSec)
	return 0, nil
}

func getWindowsDomainInfo() (string, string) {
	if domain := strings.TrimSpace(os.Getenv("USERDNSDOMAIN")); domain != "" {
		return domain, "FQDN"
	}
	if domain, ok := readRegistryString(registry.LOCAL_MACHINE, `SYSTEM\CurrentControlSet\Services\Tcpip\Parameters`, "Domain"); ok {
		return domain, "FQDN"
	}
	if domain, ok := readRegistryString(registry.LOCAL_MACHINE, `SYSTEM\CurrentControlSet\Services\Tcpip\Parameters`, "NV Domain"); ok {
		return domain, "FQDN"
	}
	if domain := strings.TrimSpace(os.Getenv("USERDOMAIN")); domain != "" && !strings.EqualFold(domain, os.Getenv("COMPUTERNAME")) {
		return domain, "NetBIOS"
	}
	return "", ""
}

func getWindowsOSVersion() string {
	key, err := registry.OpenKey(registry.LOCAL_MACHINE, `SOFTWARE\Microsoft\Windows NT\CurrentVersion`, registry.QUERY_VALUE)
	if err != nil {
		return ""
	}
	defer key.Close()

	major, _, majorErr := key.GetIntegerValue("CurrentMajorVersionNumber")
	minor, _, minorErr := key.GetIntegerValue("CurrentMinorVersionNumber")
	currentVersion, _, _ := key.GetStringValue("CurrentVersion")
	build, ok := readRegistryString(registry.LOCAL_MACHINE, `SOFTWARE\Microsoft\Windows NT\CurrentVersion`, "CurrentBuildNumber")
	if !ok {
		return ""
	}
	return buildWindowsOSVersion(major, minor, majorErr == nil && major != 0, minorErr == nil, currentVersion, build)
}

func buildWindowsOSVersion(major, minor uint64, hasMajor, hasMinor bool, currentVersion, build string) string {
	if (!hasMajor || !hasMinor) && strings.TrimSpace(currentVersion) != "" {
		parts := strings.SplitN(strings.TrimSpace(currentVersion), ".", 3)
		if len(parts) >= 2 {
			if parsedMajor, err := strconv.ParseUint(parts[0], 10, 64); err == nil {
				major = parsedMajor
				hasMajor = true
			}
			if parsedMinor, err := strconv.ParseUint(parts[1], 10, 64); err == nil {
				minor = parsedMinor
				hasMinor = true
			}
		}
	}
	if !hasMajor {
		major = 10
	}
	if !hasMinor {
		minor = 0
	}
	return fmt.Sprintf("%d.%d.%s.0", major, minor, build)
}

func readRegistryString(root registry.Key, path, name string) (string, bool) {
	key, err := registry.OpenKey(root, path, registry.QUERY_VALUE)
	if err != nil {
		return "", false
	}
	defer key.Close()

	value, _, err := key.GetStringValue(name)
	value = strings.TrimSpace(value)
	return value, err == nil && value != ""
}

func (d *EtwInput) Description() string {
	return "Generic ETW (Event Tracing for Windows) real-time event consumer"
}

func (d *EtwInput) Start(collector pipeline.Collector) error {
	d.collector = collector
	d.resetStats()
	stopCh := d.prepareRun()
	d.waitGroup.Add(1)
	defer d.waitGroup.Done()
	if d.AsyncProcess {
		d.startWorkers()
		defer d.stopWorkers()
	}

	backoff := time.Second
	for {
		err := d.runSession()
		if d.isStopped() {
			return nil
		}
		if err != nil {
			logger.Warningf(d.context.GetRuntimeContext(),
				etwAlarmType, "ETW session error: %v; retrying in %s", err, backoff)
		} else {
			logger.Warningf(d.context.GetRuntimeContext(),
				etwAlarmType, "ETW session stopped unexpectedly; retrying in %s", backoff)
		}
		select {
		case <-stopCh:
			return nil
		case <-time.After(backoff):
		}
		if backoff < maxSessionRetryBackoff {
			backoff *= 2
			if backoff > maxSessionRetryBackoff {
				backoff = maxSessionRetryBackoff
			}
		}
	}
}

func (d *EtwInput) prepareRun() <-chan struct{} {
	d.mu.Lock()
	defer d.mu.Unlock()
	d.stopped = false
	d.stopCh = make(chan struct{})
	return d.stopCh
}

func (d *EtwInput) runSession() error {
	options := []etwSessionOption{
		etw.WithLevel(etw.TraceLevel(d.Level)),
		etw.WithMatchKeywords(uint64(d.Keywords), 0),
	}
	if d.BufferSizeKB > 0 {
		options = append(options, etw.WithBufferSizeKB(d.BufferSizeKB))
	}
	if d.MinBuffers > 0 {
		options = append(options, etw.WithMinBuffers(d.MinBuffers))
	}
	if d.MaxBuffers > 0 {
		options = append(options, etw.WithMaxBuffers(d.MaxBuffers))
	}
	if d.FlushTimerSec > 0 {
		options = append(options, etw.WithFlushTimerSec(d.FlushTimerSec))
	}

	d.mu.Lock()
	if d.stopped {
		d.mu.Unlock()
		return nil
	}
	session, err := newEtwSession(d.parsedGUID, options...)
	if err != nil {
		d.mu.Unlock()
		return fmt.Errorf("create ETW session: %w", err)
	}
	d.session = session
	d.mu.Unlock()
	defer func() {
		shouldClose := false
		d.mu.Lock()
		if d.session == session {
			d.session = nil
			shouldClose = true
		}
		d.mu.Unlock()
		if shouldClose {
			_ = session.Close()
		}
	}()

	cb := func(e *etw.Event) { d.handleEvent(e) }
	if err := session.Process(cb); err != nil {
		if d.isStopped() {
			return nil
		}
		return fmt.Errorf("process ETW session: %w", err)
	}
	return nil
}

func (d *EtwInput) isStopped() bool {
	d.mu.Lock()
	defer d.mu.Unlock()
	return d.stopped
}

func minInt(a, b int) int {
	if a < b {
		return a
	}
	return b
}

func (d *EtwInput) parsePacketDataEnabled() bool {
	return d.ParsePacketData == nil || *d.ParsePacketData
}

func (d *EtwInput) getServerIP() string {
	d.serverIPMu.RLock()
	defer d.serverIPMu.RUnlock()
	return d.serverIP
}

func (d *EtwInput) setServerIP(ip string) {
	d.serverIPMu.Lock()
	defer d.serverIPMu.Unlock()
	d.serverIP = ip
}

func (d *EtwInput) startWorkers() {
	d.eventCh = make(chan etwEventData, d.EventQueueSize)
	d.statsStopCh = make(chan struct{})
	d.statsWG.Add(1)
	go func() {
		defer d.statsWG.Done()
		ticker := time.NewTicker(30 * time.Minute)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				d.logStats("periodic")
			case <-d.statsStopCh:
				return
			}
		}
	}()
	for i := 0; i < d.WorkerCount; i++ {
		d.workerWG.Add(1)
		go func() {
			defer d.workerWG.Done()
			for event := range d.eventCh {
				d.processEvent(event)
			}
		}()
	}
}

func (d *EtwInput) stopWorkers() {
	if d.statsStopCh != nil {
		close(d.statsStopCh)
		d.statsWG.Wait()
		d.statsStopCh = nil
	}
	if d.eventCh != nil {
		close(d.eventCh)
		d.workerWG.Wait()
		d.eventCh = nil
	}
	d.logStats("final")
}

func (d *EtwInput) logStats(reason string) {
	logger.Infof(d.context.GetRuntimeContext(),
		"service_etw stats reason=%s received=%d enqueued=%d dropped_queue_full=%d dropped_domain_filter=%d packet_data_parse_error=%d",
		reason,
		atomic.LoadUint64(&d.stats.received),
		atomic.LoadUint64(&d.stats.enqueued),
		atomic.LoadUint64(&d.stats.droppedQueueFull),
		atomic.LoadUint64(&d.stats.droppedDomainFilter),
		atomic.LoadUint64(&d.stats.packetDataParseError))
}

func (d *EtwInput) resetStats() {
	atomic.StoreUint64(&d.stats.received, 0)
	atomic.StoreUint64(&d.stats.enqueued, 0)
	atomic.StoreUint64(&d.stats.droppedQueueFull, 0)
	atomic.StoreUint64(&d.stats.droppedDomainFilter, 0)
	atomic.StoreUint64(&d.stats.packetDataParseError, 0)
}

func (d *EtwInput) Stop() error {
	d.mu.Lock()
	if d.stopped {
		d.mu.Unlock()
		return nil
	}
	d.stopped = true
	session := d.session
	d.session = nil
	stopCh := d.stopCh
	d.stopCh = nil
	if stopCh != nil {
		close(stopCh)
	}
	d.mu.Unlock()
	if session != nil {
		_ = session.Close()
	}
	d.waitGroup.Wait()
	return nil
}

func (d *EtwInput) handleEvent(e *etw.Event) {
	atomic.AddUint64(&d.stats.received, 1)
	data, err := e.EventProperties()
	if err != nil {
		return
	}

	event := etwEventData{
		eventID:    e.Header.EventDescriptor.ID,
		opcode:     e.Header.EventDescriptor.OpCode,
		level:      e.Header.EventDescriptor.Level,
		keywords:   e.Header.EventDescriptor.Keyword,
		processID:  e.Header.ProcessID,
		threadID:   e.Header.ThreadID,
		properties: data,
		timestamp:  e.Header.TimeStamp,
	}
	if d.AsyncProcess {
		d.enqueueEvent(event)
		return
	}
	d.processEvent(event)
}

func (d *EtwInput) enqueueEvent(event etwEventData) {
	stopCh := d.currentStopCh()
	if d.DropWhenQueueFull {
		select {
		case d.eventCh <- event:
			atomic.AddUint64(&d.stats.enqueued, 1)
		case <-stopCh:
		default:
			atomic.AddUint64(&d.stats.droppedQueueFull, 1)
		}
		return
	}

	timer := time.NewTimer(asyncEnqueueTimeout)
	defer timer.Stop()
	select {
	case d.eventCh <- event:
		atomic.AddUint64(&d.stats.enqueued, 1)
	case <-stopCh:
	case <-timer.C:
		atomic.AddUint64(&d.stats.droppedQueueFull, 1)
	}
}

func (d *EtwInput) currentStopCh() <-chan struct{} {
	d.mu.Lock()
	defer d.mu.Unlock()
	return d.stopCh
}

func (d *EtwInput) processEvent(event etwEventData) {
	fields := make(map[string]string, len(event.properties)+20)
	fields["event_id"] = strconv.FormatUint(uint64(event.eventID), 10)
	fields["opcode"] = strconv.FormatUint(uint64(event.opcode), 10)
	fields["level"] = strconv.FormatUint(uint64(event.level), 10)
	fields["keywords"] = fmt.Sprintf("0x%X", event.keywords)
	fields["process_id"] = strconv.FormatUint(uint64(event.processID), 10)
	fields["thread_id"] = strconv.FormatUint(uint64(event.threadID), 10)
	for k, v := range event.properties {
		fields[normalizeETWFieldName(k)] = fmt.Sprintf("%v", v)
	}
	if d.isDNSProvider() {
		if d.shouldDropDNSEvent(fields) {
			atomic.AddUint64(&d.stats.droppedDomainFilter, 1)
			return
		}
		d.enrichDNSFields(fields, event.eventID)
	}

	tags := map[string]string{"source": d.Source}
	d.collectorMu.Lock()
	defer d.collectorMu.Unlock()
	d.collector.AddData(tags, fields, event.timestamp)
}

func normalizeETWFieldName(name string) string {
	if name == "" {
		return ""
	}

	var builder strings.Builder
	runes := []rune(name)
	for i, r := range runes {
		if r == '-' || r == ' ' || r == '.' {
			if builder.Len() > 0 {
				builder.WriteRune('_')
			}
			continue
		}
		if r == '_' {
			if builder.Len() > 0 {
				builder.WriteRune('_')
			}
			continue
		}
		if unicode.IsUpper(r) {
			prevLowerOrDigit := i > 0 && (unicode.IsLower(runes[i-1]) || unicode.IsDigit(runes[i-1]))
			nextLower := i+1 < len(runes) && unicode.IsLower(runes[i+1])
			if builder.Len() > 0 && (prevLowerOrDigit || nextLower) {
				builder.WriteRune('_')
			}
			builder.WriteRune(unicode.ToLower(r))
			continue
		}
		builder.WriteRune(unicode.ToLower(r))
	}
	return strings.Trim(builder.String(), "_")
}

func init() {
	pipeline.ServiceInputs[pluginType] = func() pipeline.ServiceInput {
		return &EtwInput{}
	}
}
