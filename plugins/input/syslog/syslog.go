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

package inputsyslog

import (
	"bufio"
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"net/url"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/pkg/util"
)

const (
	maxMessageSize = 512 * 1024
)

// Syslog is a service input plugin to collect syslog.
// It works with syslog agents, such as rsyslog. It uses TCP or UDP to receive syslog
// from agents, and then parses them with parsers specified by users.
// It allows users to offer ParseFailField, if a failure happened in parsing pharse,
// it can stop parse and copy whole data to specify field, and returns to caller.
type Syslog struct {
	Address            string // Address to receive logs from agents, eg. [tcp/udp]://[host]:[port].
	MaxConnections     int    // Max connections, for TCP only.
	TimeoutSeconds     int    // The number of seconds of inactivity before a remote connection is closed.
	MaxMessageSize     int    // Maximum size of message in bytes received over transport protocol.
	KeepAliveSeconds   int    // The number of seconds to set up keep-alive, for TCP only.
	ParseProtocol      string // ["", rfc3164, rfc5424, auto], empty means no parser.
	IgnoreParseFailure bool   // When parse failure happened, ignore error and set content field if it is set.
	AddHostname        bool   // When listen unixgram from /dev/log, the hostname field is not included in the log, so use rfc3164 will cause parse error, so AddHostname give parser it's own hostname, then parser can parse tag, program, content field currently.

	// AutoConfigRsyslog enables automatic rsyslog forwarding configuration.
	// When true, LoongCollector will generate a forwarding rule in /etc/rsyslog.d/
	// and restart rsyslogd. Only supports TCP and UDP (not unixgram).
	// Requires root privileges. Default: false.
	AutoConfigRsyslog bool
	// RsyslogFilters specifies which syslog messages to forward via rsyslog filter rules.
	// Each entry should be in rsyslog "facility.severity" format (e.g. "auth.warning").
	// If empty or nil, all messages (*.*) are forwarded.
	// Only effective when AutoConfigRsyslog is true.
	RsyslogFilters []string

	done chan struct{}
	mu   sync.Mutex
	wg   sync.WaitGroup
	io.Closer

	context       pipeline.Context
	isStream      bool
	isUnix        bool // If scheme is "unixgram", need to flag it and delete file when closed.
	connections   map[string]net.Conn
	connectionsMu sync.Mutex
	connectionsWg sync.WaitGroup
	tcpListener   net.Listener
	udpListener   net.PacketConn
	parser        parser
}

// Init ...
func (s *Syslog) Init(context pipeline.Context) (int, error) {
	if s.MaxMessageSize > maxMessageSize {
		s.MaxMessageSize = maxMessageSize
	}

	s.ParseProtocol = strings.ToLower(strings.TrimSpace(s.ParseProtocol))
	if "" == s.ParseProtocol && !s.IgnoreParseFailure {
		return 0, errors.New("Default parser must set IgnoreParseFailure")
	}
	creator := syslogParsers[s.ParseProtocol]
	if creator == nil {
		return 0, errors.New("Unsupported parser protocol: " + s.ParseProtocol)
	}
	s.parser = creator(&parserConfig{
		ignoreParseFailure: s.IgnoreParseFailure,
		addHostname:        s.AddHostname,
	})

	s.context = context
	logger.Debug(s.context.GetRuntimeContext(), "syslog load config", s.context.GetConfigName())
	return 0, nil
}

// Description ...
func (s *Syslog) Description() string {
	return "A input plugin for syslog"
}

// Collect ...
func (s *Syslog) Collect(collector pipeline.Collector) error {
	return nil
}

// Start ...
func (s *Syslog) Start(collector pipeline.Collector) error {
	s.done = make(chan struct{}, 1)
	scheme, host, err := getAddressParts(s.Address)
	if err != nil {
		return err
	}

	switch scheme {
	case "tcp", "tcp4", "tcp6":
		s.isStream = true
	case "udp", "udp4", "udp6":
		s.isStream = false
	case "unixgram":
		s.isStream = false
		s.isUnix = true
	default:
		return fmt.Errorf("unknown protocol '%s' in '%s'", scheme, host)
	}

	// Auto-configure rsyslog if enabled
	if s.AutoConfigRsyslog {
		s.configureRsyslogIfNeeded(scheme, host)
	}

	if s.isStream {
		l, err := net.Listen(scheme, host)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), selfmonitor.ServiceSyslogInitAlarm, "net.Listen error", err,
				"Address", s.Address, "scheme", scheme, "host", host)
			return err
		}
		s.tcpListener = l
		s.Closer = l

		s.wg.Add(1)
		go s.listenStream(collector)
	} else {
		l, err := net.ListenPacket(scheme, host)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), selfmonitor.ServiceSyslogInitAlarm, "net.ListenPacket error", err,
				"Address", s.Address, "scheme", scheme, "host", host)
			return err
		}
		s.Closer = l
		s.udpListener = l

		s.wg.Add(1)
		go s.listenPacket(collector)
	}

	return nil
}

// Stop ...
func (s *Syslog) Stop() error {
	close(s.done)
	s.mu.Lock()
	defer s.mu.Unlock()

	if s.Closer != nil {
		if err := s.Close(); err != nil {
			logger.Warning(s.context.GetRuntimeContext(), selfmonitor.ServiceSyslogCloseAlarm, "Syslog.Close error", err,
				"Address", s.Address)
		}
	}
	s.wg.Wait()
	s.connectionsWg.Wait()

	// Tear down auto-generated rsyslog forwarding so a removed pipeline does not leave
	// rsyslog forwarding (and accumulating a disk queue) to a port no longer listened on.
	if s.AutoConfigRsyslog {
		s.cleanupRsyslogConfig()
	}

	// If scheme type is "unixgram", remove unix socket file after close.
	if s.isUnix {
		_, host, err := getAddressParts(s.Address)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), selfmonitor.ServiceSyslogCloseAlarm, "getAddressParts error", err,
				"Address", s.Address)
		}
		err = os.Remove(host)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), selfmonitor.ServiceSyslogCloseAlarm, "os.Remove error", err,
				"Host", host)
		}
	}
	return nil
}

// configureRsyslogIfNeeded generates rsyslog forwarding config and restarts rsyslogd.
// It only proceeds if: the protocol is TCP or UDP (not unixgram), running as root,
// rsyslog is v8+, and the config content has changed. On any precondition failure it
// alarms and skips without affecting the plugin's normal operation. Before restarting,
// the newly written config is validated with `rsyslogd -N1`; if validation fails, the
// change is rolled back so existing rsyslog forwarding is never broken.
func (s *Syslog) configureRsyslogIfNeeded(scheme, host string) {
	ctx := s.context.GetRuntimeContext()

	// Check protocol support.
	if scheme == "unixgram" {
		logger.Warning(ctx, selfmonitor.ServiceSyslogRsyslogConfigAlarm,
			"AutoConfigRsyslog does not support unixgram protocol, skipping rsyslog auto-configuration",
			"Address", s.Address)
		return
	}

	// Check root permission.
	if !isRoot() {
		logger.Warning(ctx, selfmonitor.ServiceSyslogRsyslogConfigAlarm,
			"AutoConfigRsyslog requires root privileges, skipping rsyslog auto-configuration",
			"Address", s.Address)
		return
	}

	// Reject filters that could break the generated config structure.
	if filterErr := validateRsyslogFilters(s.RsyslogFilters); filterErr != nil {
		logger.Warning(ctx, selfmonitor.ServiceSyslogRsyslogConfigAlarm,
			"invalid RsyslogFilters, skipping rsyslog auto-configuration", filterErr,
			"Address", s.Address)
		return
	}

	// Check rsyslog version (needs v8+ for the action(type="omfwd" ...) syntax).
	major, verErr := checkRsyslogVersion(ctx)
	if verErr != nil {
		logger.Warning(ctx, selfmonitor.ServiceSyslogRsyslogConfigAlarm,
			"failed to detect rsyslog version, skipping rsyslog auto-configuration", verErr)
		return
	}
	if major < minRsyslogMajorVersion {
		logger.Warning(ctx, selfmonitor.ServiceSyslogRsyslogConfigAlarm,
			"rsyslog version too old for auto-configuration (requires v8+), skipping",
			"detectedMajorVersion", major)
		return
	}

	// Parse address for rsyslog config.
	configName := s.context.GetConfigName()
	target, port, err := parseTargetPort(host)
	if err != nil {
		logger.Warning(ctx, selfmonitor.ServiceSyslogRsyslogConfigAlarm,
			"failed to parse address for rsyslog config", err,
			"Address", s.Address)
		return
	}

	cfg := &SyslogForwardingConfig{
		ConfigName: configName,
		Protocol:   normalizeRsyslogProtocol(scheme),
		Target:     target,
		Port:       port,
		Filters:    s.RsyslogFilters,
	}

	content := generateRsyslogConfig(cfg)

	// Serialize the host-global write+validate+restart against other pipelines and
	// against Stop-time cleanup, so a concurrent drop-in cannot fail this pipeline's
	// `rsyslogd -N1` and cause a spurious rollback.
	rsyslogOpMu.Lock()
	defer rsyslogOpMu.Unlock()

	changed, oldContent, writeErr := writeRsyslogConfig(configName, content)
	if writeErr != nil {
		logger.Warning(ctx, selfmonitor.ServiceSyslogRsyslogConfigAlarm,
			"failed to write rsyslog config", writeErr,
			"configName", configName)
		return
	}

	if !changed {
		logger.Info(ctx, "rsyslog config unchanged (no restart needed)",
			"configName", configName)
		return
	}

	// Validate the written config before restarting; roll back on failure so a bad
	// config never takes down rsyslog's existing forwarding.
	if validateErr := validateRsyslogConfig(ctx); validateErr != nil {
		logger.Warning(ctx, selfmonitor.ServiceSyslogRsyslogConfigAlarm,
			"rsyslog config validation failed, rolling back and skipping auto-configuration", validateErr,
			"configName", configName)
		if restoreErr := restoreRsyslogConfig(configName, oldContent); restoreErr != nil {
			logger.Warning(ctx, selfmonitor.ServiceSyslogRsyslogConfigAlarm,
				"failed to roll back rsyslog config after validation failure", restoreErr,
				"configName", configName)
		}
		return
	}

	logger.Info(ctx, "rsyslog config updated, restarting rsyslogd",
		"configName", configName,
		"configFile", rsyslogConfigFilePath(configName))
	if restartErr := restartRsyslog(ctx); restartErr != nil {
		logger.Warning(ctx, selfmonitor.ServiceSyslogRsyslogRestartAlarm,
			"failed to restart rsyslogd", restartErr)
	}
}

// cleanupRsyslogConfig removes the auto-generated rsyslog config on Stop and restarts
// rsyslogd so the forwarding action (and its disk-assisted retry queue) is torn down
// instead of lingering and endlessly retrying against a port nobody listens on. It only
// restarts when a file was actually removed, and skips silently when not running as root.
func (s *Syslog) cleanupRsyslogConfig() {
	ctx := s.context.GetRuntimeContext()

	if !isRoot() {
		logger.Warning(ctx, selfmonitor.ServiceSyslogRsyslogConfigAlarm,
			"AutoConfigRsyslog cleanup requires root privileges, skipping rsyslog config removal",
			"Address", s.Address)
		return
	}

	configName := s.context.GetConfigName()

	rsyslogOpMu.Lock()
	defer rsyslogOpMu.Unlock()

	removed, err := removeRsyslogConfig(configName)
	if err != nil {
		logger.Warning(ctx, selfmonitor.ServiceSyslogRsyslogConfigAlarm,
			"failed to remove rsyslog config on stop", err,
			"configName", configName)
		return
	}
	if !removed {
		return
	}

	logger.Info(ctx, "rsyslog config removed on stop, restarting rsyslogd",
		"configName", configName)
	if restartErr := restartRsyslog(ctx); restartErr != nil {
		logger.Warning(ctx, selfmonitor.ServiceSyslogRsyslogRestartAlarm,
			"failed to restart rsyslogd after config cleanup", restartErr)
	}
}

// parseTargetPort splits "host:port" into target and port strings.
func parseTargetPort(host string) (target, port string, err error) {
	target, port, err = net.SplitHostPort(host)
	if err != nil {
		return "", "", fmt.Errorf("split host port from %s: %w", host, err)
	}
	switch target {
	case "", "0.0.0.0":
		target = "127.0.0.1"
	case "::":
		target = "::1"
	}
	return target, port, nil
}

func getAddressParts(a string) (string, string, error) {
	parts := strings.SplitN(a, "://", 2)
	if len(parts) != 2 {
		return "", "", fmt.Errorf("missing protocol within address '%s'", a)
	}

	u, _ := url.Parse(a)
	switch u.Scheme {
	case "unix", "unixpacket", "unixgram":
		return parts[0], parts[1], nil
	}

	port := u.Port()
	if port == "" {
		port = "6514"
	}
	// net.JoinHostPort brackets IPv6 hosts (e.g. "[::1]:6514") so both net.Listen
	// and the downstream net.SplitHostPort in parseTargetPort accept the result.
	host := net.JoinHostPort(u.Hostname(), port)

	return u.Scheme, host, nil
}

func (s *Syslog) resetTimeout(c net.Conn) {
	if s.TimeoutSeconds > 0 {
		_ = c.SetReadDeadline(time.Now().Add(time.Duration(s.TimeoutSeconds) * time.Second))
	}
}

func (s *Syslog) setKeepAlive(c *net.TCPConn) error {
	if s.KeepAliveSeconds < 0 {
		return nil
	}

	if 0 == s.KeepAliveSeconds {
		return c.SetKeepAlive(false)
	}
	if err := c.SetKeepAlive(true); err != nil {
		return err
	}
	return c.SetKeepAlivePeriod(time.Duration(s.KeepAliveSeconds) * time.Second)
}

// sleepWithChan sleeps until timeout or syslog is done.
// It returns true if done is received.
func (s *Syslog) sleepWithChan(duration time.Duration) bool {
	select {
	case <-s.done:
		return true
	case <-time.After(duration):
		return false
	}
}

func (s *Syslog) listenStream(collector pipeline.Collector) {
	defer s.wg.Done()

	s.connections = map[string]net.Conn{}
	backoff := newSimpleBackoff()
Loop:
	for {
		conn, err := s.tcpListener.Accept()
		if err != nil {
			select {
			case <-s.done:
				logger.Info(s.context.GetRuntimeContext(), "Stop syslog because it is being stopped.")
				break Loop
			default:
			}

			// Alarm and sleep (with backoff).
			logger.Warning(s.context.GetRuntimeContext(), selfmonitor.ServiceSyslogStreamAlarm, "accept error", err)
			if s.sleepWithChan(backoff.Next()) {
				break
			}
			continue
		} else {
			backoff.Reset()
		}
		tcpConn, _ := conn.(*net.TCPConn)

		s.connectionsMu.Lock()
		if s.MaxConnections > 0 && len(s.connections) >= s.MaxConnections {
			s.connectionsMu.Unlock()
			_ = conn.Close()
			continue
		}
		s.connections[conn.RemoteAddr().String()] = conn
		s.connectionsMu.Unlock()

		if err := s.setKeepAlive(tcpConn); err != nil {
			logger.Warning(s.context.GetRuntimeContext(), selfmonitor.ServiceSyslogStreamAlarm, "setKeepAlive error", err)
		}

		s.connectionsWg.Add(1)
		go s.handle(conn, collector)
	}

	s.connectionsMu.Lock()
	for _, c := range s.connections {
		_ = c.Close()
	}
	s.connections = nil
	s.connectionsMu.Unlock()
}

func (s *Syslog) handle(conn net.Conn, collector pipeline.Collector) {
	defer func() {
		s.removeConnection(conn)
		_ = conn.Close()
		s.connectionsWg.Done()
	}()

	logger.Info(s.context.GetRuntimeContext(), "handle for connection", conn.RemoteAddr().String(), "begin")
	buf := bufio.NewReader(conn)
	scanner := bufio.NewScanner(buf)
	byteBuf := make([]byte, s.MaxMessageSize)
	scanner.Buffer(byteBuf, s.MaxMessageSize)
	s.resetTimeout(conn)
	backoff := newSimpleBackoff()
	// TODO: Scan panics if the split function returns too many empty tokens without advancing the input.
	// This is a common error mode for scanners.
	for scanner.Scan() {
		err := scanner.Err()
		if err != nil {
			if strings.HasSuffix(err.Error(), ": use of closed network connection") {
				logger.Info(s.context.GetRuntimeContext(), "Quit stream connection because of closed network connection")
				break
			}

			// I/O timeout.
			if strings.HasSuffix(err.Error(), ": i/o timeout") {
				logger.Warning(s.context.GetRuntimeContext(), selfmonitor.ServiceSyslogStreamAlarm, "connection i/o timeout")
				s.resetTimeout(conn)
				continue
			}

			logger.Warning(s.context.GetRuntimeContext(), selfmonitor.ServiceSyslogStreamAlarm, "scan error", err)
			if s.sleepWithChan(backoff.Next()) || backoff.CanQuit() {
				break
			}
			continue
		} else {
			backoff.Reset()
		}

		data := scanner.Bytes()
		if len(data) > 0 {
			s.parse(data, conn.RemoteAddr().String(), collector)
		}
		s.resetTimeout(conn)
	}
	if err := scanner.Err(); err != nil {
		logger.Warning(s.context.GetRuntimeContext(), selfmonitor.ServiceSyslogStreamAlarm, "quit stream connection because of scan error out of loop", err)
	}
	logger.Info(s.context.GetRuntimeContext(), "handle for connection", conn.RemoteAddr().String(), "quit")
}

func (s *Syslog) removeConnection(c net.Conn) {
	s.connectionsMu.Lock()
	delete(s.connections, c.RemoteAddr().String())
	s.connectionsMu.Unlock()
}

// TODO: There is a problem here, if for loop quit because of i/o timeout, the plugin quits
// without any notification.
func (s *Syslog) listenPacket(collector pipeline.Collector) {
	defer s.wg.Done()

	b := make([]byte, s.MaxMessageSize)
	backoff := newSimpleBackoff()
	for {
		if s.TimeoutSeconds > 0 {
			_ = s.udpListener.SetReadDeadline(time.Now().Add(time.Duration(s.TimeoutSeconds) * time.Second))
		}
		n, addr, err := s.udpListener.ReadFrom(b)
		if err != nil {
			if strings.HasSuffix(err.Error(), ": use of closed network connection") {
				logger.Info(s.context.GetRuntimeContext(), "Quit packet connection because of closed network connection")
				break
			}

			// I/O timeout, warn and retry.
			if strings.HasSuffix(err.Error(), ": i/o timeout") {
				logger.Warning(s.context.GetRuntimeContext(), selfmonitor.ServiceSyslogPacketAlarm, "connection i/o timeout")
				continue
			}

			logger.Warning(s.context.GetRuntimeContext(), selfmonitor.ServiceSyslogPacketAlarm, "read from error", err)
			if s.sleepWithChan(backoff.Next()) {
				break
			}
			continue
		} else {
			backoff.Reset()
		}

		data := b[:n]
		if len(data) > 0 {
			s.parse(data, fmt.Sprint(addr), collector)
		}
	}
}

func (s *Syslog) parse(b []byte, clientIP string, collector pipeline.Collector) {
	lines := bytes.Split(b, []byte("\n"))
	if '\n' == b[len(b)-1] {
		lines = lines[:len(lines)-1]
	}

	// Parse lines one by one, fill some fields of result if they are empty.
	for _, line := range lines {
		rst, err := s.parser.Parse(line)
		if err != nil {
			logger.Warning(s.context.GetRuntimeContext(), selfmonitor.ServiceSyslogParseAlarm, "Parse failed with protocol '", s.ParseProtocol,
				"error", err,
				"', drop line:", string(line))
			continue
		}

		fields := map[string]string{}
		fields["_program_"] = rst.program
		fields["_priority_"] = strconv.Itoa(rst.priority)
		fields["_facility_"] = strconv.Itoa(rst.facility)
		fields["_severity_"] = strconv.Itoa(rst.severity)
		// use nano timestamp because RFC5424's timestamp is [RFC3339]
		// eg: 2003-08-24T05:14:15.000003-07:00, 2003-10-11T22:14:15.003Z
		fields["_unixtimestamp_"] = strconv.FormatInt(rst.time.UnixNano(), 10)
		if rst.hostname == "" {
			fields["_hostname_"] = util.GetHostName()
		} else {
			fields["_hostname_"] = rst.hostname
		}
		if len(clientIP) > 0 {
			fields["_client_ip_"] = strings.Split(clientIP, ":")[0]
		} else {
			fields["_client_ip_"] = ""
		}

		fields["_ip_"] = util.GetIPAddress()
		fields["_content_"] = rst.content

		if rst.structuredData != nil {
			structuredData, _ := json.Marshal(*rst.structuredData)
			fields["_structured_data_"] = string(structuredData)
		}
		if rst.msgID != nil {
			fields["_message_id_"] = *rst.msgID
		}
		if rst.procID != nil {
			fields["_process_id_"] = *rst.procID
		}

		collector.AddData(nil, fields, rst.time)
	}
}

func newSyslog() *Syslog {
	return &Syslog{
		ParseProtocol:      "",
		Address:            "tcp://127.0.0.1:0",
		MaxConnections:     100,
		TimeoutSeconds:     0,
		KeepAliveSeconds:   300,
		MaxMessageSize:     64 * 1024,
		IgnoreParseFailure: true,
		AutoConfigRsyslog:  false,
		// RsyslogFilters defaults to nil (zero value for slices)
	}
}

func init() {
	pipeline.ServiceInputs["service_syslog"] = func() pipeline.ServiceInput {
		return newSyslog()
	}
}
