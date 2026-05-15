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
	"unicode"

	"github.com/bi-zone/etw"
	"golang.org/x/sys/windows"
	"golang.org/x/sys/windows/registry"

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
	// DNSQueryDomainFilters drops Microsoft-Windows-DNSServer events whose dns_query matches.
	// It supports exact domains and leading wildcard suffixes such as "*.azure.cn".
	DNSQueryDomainFilters []string

	parsedGUID windows.GUID
	session    *etw.Session
	context    pipeline.Context
	collector  pipeline.Collector
	hostname   string
	domain     string
	domainType string
	osName     string
	osVersion  string
	serverIP   string
	mu         sync.Mutex
	waitGroup  sync.WaitGroup
	stopped    bool
}

func (d *EtwInput) Init(context pipeline.Context) (int, error) {
	d.context = context
	d.hostname, _ = os.Hostname()
	d.domain, d.domainType = getWindowsDomainInfo()
	d.osName = "Windows"
	d.osVersion = getWindowsOSVersion()

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

	logger.Infof(d.context.GetRuntimeContext(),
		"service_etw init: guid=%s level=%d keywords=0x%X",
		guidStr, d.Level, uint64(d.Keywords))
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
	d.waitGroup.Add(1)
	defer d.waitGroup.Done()

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
	d.waitGroup.Wait()
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
		fields[normalizeETWFieldName(k)] = fmt.Sprintf("%v", v)
	}

	if d.isDNSProvider() {
		if d.shouldDropDNSEvent(fields) {
			return
		}
		d.enrichDNSFields(fields, eventID)
	}

	tags := map[string]string{"source": "etw"}
	d.collector.AddData(tags, fields, e.Header.TimeStamp)
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
