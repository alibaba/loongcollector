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
	"context"
	"fmt"
	"os"
	"os/exec"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
)

// rsyslogOpMu serializes the "write + validate + restart" critical section (and the
// Stop-time cleanup) across pipelines. `rsyslogd -N1` validates the whole effective
// config, so a concurrent half-written drop-in from another pipeline could otherwise
// fail an unrelated pipeline's validation and trigger a spurious rollback; concurrent
// restarts could also stomp on each other. All host-global rsyslog operations run
// under this lock.
var rsyslogOpMu sync.Mutex

const (
	rsyslogFilePrefix = "10-loongcollector"

	// minRsyslogMajorVersion is the minimum rsyslog major version required for the
	// action(type="omfwd" ...) syntax used by the generated config.
	minRsyslogMajorVersion = 8

	// restartTimeout bounds how long we wait for rsyslog to restart, preventing a
	// stuck rsyslogd from blocking the plugin indefinitely.
	restartTimeout = 30 * time.Second
	// versionCheckTimeout / validateTimeout bound the auxiliary rsyslogd invocations.
	versionCheckTimeout = 5 * time.Second
	validateTimeout     = 15 * time.Second
)

// rsyslogConfigDir is the directory where rsyslog loads drop-in config files.
// It is a var (not const) so unit tests can redirect writes to a temp directory.
var rsyslogConfigDir = "/etc/rsyslog.d"

// systemdMarkerPath, when present, indicates the host uses systemd as its init system.
var systemdMarkerPath = "/run/systemd/system"

// rsyslogVersionRegex extracts the major version from `rsyslogd -v` output,
// e.g. "rsyslogd 8.2102.0-...".
var rsyslogVersionRegex = regexp.MustCompile(`rsyslogd\s+(\d+)\.`)

// SyslogForwardingConfig holds parsed info for rsyslog forwarding.
type SyslogForwardingConfig struct {
	ConfigName string   // pipeline config name, used for filename
	Protocol   string   // "tcp" or "udp", parsed from Address
	Target     string   // IP/hostname, parsed from Address
	Port       string   // port number, parsed from Address
	Filters    []string // rsyslog filter rules like "auth.warning", default ["*.*"]
}

// normalizeRsyslogProtocol maps a Go network scheme to the value accepted by the
// rsyslog omfwd "Protocol" parameter, which only understands "tcp" or "udp".
// The IPv4/IPv6 variants allowed by net.Listen (tcp4/tcp6/udp4/udp6) would fail
// `rsyslogd -N1` validation and trigger a rollback, so collapse them here.
func normalizeRsyslogProtocol(scheme string) string {
	switch scheme {
	case "tcp4", "tcp6":
		return "tcp"
	case "udp4", "udp6":
		return "udp"
	default:
		return scheme
	}
}

// rsyslogConfigFilePath returns the full path for the rsyslog config file.
func rsyslogConfigFilePath(configName string) string {
	return fmt.Sprintf("%s/%s-%s.conf", rsyslogConfigDir, rsyslogFilePrefix, sanitizeConfigName(configName))
}

// validateRsyslogFilters rejects filter entries that contain a carriage return or
// newline, which would break out of the selector line and inject extra rsyslog
// directives. Other syntax errors are still caught by `rsyslogd -N1`, but newlines
// are the structural hazard worth failing fast on with a clear alarm.
func validateRsyslogFilters(filters []string) error {
	for _, f := range filters {
		if strings.ContainsAny(f, "\r\n") {
			return fmt.Errorf("rsyslog filter %q contains a newline", f)
		}
	}
	return nil
}

// sanitizeConfigName replaces any character that is NOT [a-zA-Z0-9_-] with '_'.
func sanitizeConfigName(name string) string {
	var b strings.Builder
	b.Grow(len(name))
	for _, r := range name {
		if (r >= 'a' && r <= 'z') || (r >= 'A' && r <= 'Z') || (r >= '0' && r <= '9') || r == '_' || r == '-' {
			b.WriteRune(r)
		} else {
			b.WriteByte('_')
		}
	}
	return b.String()
}

// generateRsyslogConfig generates rsyslog v8+ action() syntax config content.
// The template mirrors the Azure Monitor Agent (AMA) reference config: it defines a
// traditional forward format template (to preserve PRI/timestamp/hostname downstream)
// and a disk-assisted queue with infinite retry so logs survive LoongCollector restarts.
func generateRsyslogConfig(cfg *SyslogForwardingConfig) string {
	sanitized := sanitizeConfigName(cfg.ConfigName)

	// Build filter selector: default to all facilities/severities.
	filterSelector := "*.*"
	if len(cfg.Filters) > 0 {
		filterSelector = strings.Join(cfg.Filters, ";")
	}

	var b strings.Builder
	b.WriteString("# LoongCollector auto-generated rsyslog forwarding configuration\n")
	// %q escapes newlines/control chars so a crafted ConfigName cannot break out
	// of the comment and inject extra rsyslog directives.
	b.WriteString(fmt.Sprintf("# Config: %q\n", cfg.ConfigName))
	b.WriteString("# DO NOT EDIT - managed by LoongCollector service_syslog plugin\n")
	b.WriteString("\n")
	b.WriteString("template(name=\"LC_TraditionalForwardFormat\" type=\"string\"\n")
	b.WriteString("  string=\"<%PRI%>%TIMESTAMP% %HOSTNAME% %syslogtag%%msg:::sp-if-no-1st-sp%%msg%\")\n")
	b.WriteString("\n")
	b.WriteString(fmt.Sprintf("%s action(type=\"omfwd\"\n", filterSelector))
	b.WriteString("  template=\"LC_TraditionalForwardFormat\"\n")
	b.WriteString("  queue.type=\"LinkedList\"\n")
	b.WriteString(fmt.Sprintf("  queue.filename=\"omfwd-loongcollector-%s\"\n", sanitized))
	b.WriteString("  queue.maxFileSize=\"32m\"\n")
	b.WriteString("  queue.maxDiskSpace=\"1g\"\n")
	b.WriteString("  queue.size=\"25000\"\n")
	b.WriteString("  queue.workerThreads=\"100\"\n")
	b.WriteString("  queue.dequeueBatchSize=\"2048\"\n")
	b.WriteString("  queue.saveonshutdown=\"on\"\n")
	b.WriteString("  action.resumeRetryCount=\"-1\"\n")
	b.WriteString("  action.resumeInterval=\"5\"\n")
	b.WriteString("  action.reportSuspension=\"on\"\n")
	b.WriteString("  action.reportSuspensionContinuation=\"on\"\n")
	b.WriteString(fmt.Sprintf("  target=\"%s\" Port=\"%s\" Protocol=\"%s\")\n", cfg.Target, cfg.Port, cfg.Protocol))

	return b.String()
}

// writeRsyslogConfig writes the rsyslog config file if content has changed.
// It returns whether the file was written (changed) and the previous content:
// a nil oldContent means the file did not exist, while a non-nil pointer (even
// to "") means it existed, so the caller can roll back correctly on validation
// failure — distinguishing "delete the file" from "restore an empty file".
func writeRsyslogConfig(configName string, content string) (changed bool, oldContent *string, err error) {
	path := rsyslogConfigFilePath(configName)

	// Read existing content (if any) so we can compare and roll back.
	existing, readErr := os.ReadFile(path)
	switch {
	case readErr == nil:
		prev := string(existing)
		oldContent = &prev
		if prev == content {
			return false, oldContent, nil
		}
	case !os.IsNotExist(readErr):
		// The file exists but cannot be read. Overwriting now would destroy the
		// original, and on a later rollback a nil oldContent would be mistaken
		// for "no previous file" and delete the config. Abort instead so existing
		// rsyslog forwarding is never broken.
		return false, nil, fmt.Errorf("read existing rsyslog config %s: %w", path, readErr)
	}

	if writeErr := os.WriteFile(path, []byte(content), 0644); writeErr != nil {
		return false, oldContent, fmt.Errorf("write rsyslog config %s: %w", path, writeErr)
	}

	return true, oldContent, nil
}

// restoreRsyslogConfig rolls back a config file to its previous state: it restores
// the previous content if the file existed (oldContent non-nil, including an empty
// file), otherwise removes the file (oldContent nil).
func restoreRsyslogConfig(configName string, oldContent *string) error {
	path := rsyslogConfigFilePath(configName)
	if oldContent == nil {
		_, err := removeRsyslogConfig(configName)
		return err
	}
	if err := os.WriteFile(path, []byte(*oldContent), 0644); err != nil {
		return fmt.Errorf("restore rsyslog config %s: %w", path, err)
	}
	return nil
}

// removeRsyslogConfig removes the rsyslog config file for the given config name.
// It returns removed=true only when a file actually existed and was deleted, so the
// caller can decide whether a rsyslog restart is warranted. A missing file is a no-op.
func removeRsyslogConfig(configName string) (removed bool, err error) {
	path := rsyslogConfigFilePath(configName)
	if err := os.Remove(path); err != nil {
		if os.IsNotExist(err) {
			return false, nil
		}
		return false, fmt.Errorf("remove rsyslog config %s: %w", path, err)
	}
	return true, nil
}

// runCommand runs an external command bounded by timeout and returns its combined output.
// It tolerates the "no child process" error that exec throws under the c-shared build mode.
func runCommand(ctx context.Context, timeout time.Duration, name string, args ...string) (string, error) {
	cctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()

	out, err := exec.CommandContext(cctx, name, args...).CombinedOutput() //nolint:gosec
	if err != nil && strings.Contains(err.Error(), "no child process") {
		// Workaround: exec throws "wait: no child process" under c-shared buildmode
		// even though the command succeeded.
		err = nil
	}
	return string(out), err
}

// checkRsyslogVersion returns the rsyslog major version by parsing `rsyslogd -v`.
func checkRsyslogVersion(ctx context.Context) (int, error) {
	out, err := runCommand(ctx, versionCheckTimeout, "rsyslogd", "-v")
	if err != nil {
		return 0, fmt.Errorf("run rsyslogd -v: %w", err)
	}
	return parseRsyslogMajorVersion(out)
}

// parseRsyslogMajorVersion extracts the major version number from `rsyslogd -v` output.
func parseRsyslogMajorVersion(output string) (int, error) {
	m := rsyslogVersionRegex.FindStringSubmatch(output)
	if len(m) < 2 {
		return 0, fmt.Errorf("cannot parse rsyslog version from output: %q", strings.TrimSpace(output))
	}
	major, err := strconv.Atoi(m[1])
	if err != nil {
		return 0, fmt.Errorf("parse rsyslog major version %q: %w", m[1], err)
	}
	return major, nil
}

// validateRsyslogConfig runs `rsyslogd -N1` to check the whole effective config
// (including the file we just wrote via $IncludeConfig) for syntax errors.
func validateRsyslogConfig(ctx context.Context) error {
	out, err := runCommand(ctx, validateTimeout, "rsyslogd", "-N1")
	if err != nil {
		return fmt.Errorf("rsyslogd -N1 validation failed: %w, output: %s", err, strings.TrimSpace(out))
	}
	return nil
}

// detectInitSystem returns the command and args to restart rsyslog based on the
// host's init system: systemd if the systemd runtime marker exists, otherwise SysV.
func detectInitSystem() (name string, args []string) {
	if fi, err := os.Stat(systemdMarkerPath); err == nil && fi.IsDir() {
		return "systemctl", []string{"restart", "rsyslog"}
	}
	return "service", []string{"rsyslog", "restart"}
}

// restartRsyslog restarts the rsyslog service, bounded by restartTimeout.
func restartRsyslog(ctx context.Context) error {
	name, args := detectInitSystem()
	if _, err := runCommand(ctx, restartTimeout, name, args...); err != nil {
		return fmt.Errorf("restart rsyslog via %s: %w", name, err)
	}
	logger.Info(ctx, "rsyslog restarted successfully", "via", name)
	return nil
}

// isRoot returns true if the current process is running as root.
func isRoot() bool {
	return os.Getuid() == 0
}
