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
	"os"
	"path/filepath"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestSanitizeConfigName(t *testing.T) {
	cases := []struct {
		in, want string
	}{
		{"simple", "simple"},
		{"with-dash_and_9", "with-dash_and_9"},
		{"has space", "has_space"},
		{"a/b.c:d", "a_b_c_d"},
		{"中文名", "___"},
		{"", ""},
	}
	for _, c := range cases {
		assert.Equal(t, c.want, sanitizeConfigName(c.in), "input=%q", c.in)
	}
}

func TestNormalizeRsyslogProtocol(t *testing.T) {
	cases := []struct {
		in, want string
	}{
		{"tcp", "tcp"},
		{"udp", "udp"},
		{"tcp4", "tcp"},
		{"tcp6", "tcp"},
		{"udp4", "udp"},
		{"udp6", "udp"},
		{"unixgram", "unixgram"}, // untouched; unixgram never reaches config generation
	}
	for _, c := range cases {
		assert.Equal(t, c.want, normalizeRsyslogProtocol(c.in), "input=%q", c.in)
	}
}

func TestRsyslogConfigFilePath(t *testing.T) {
	old := rsyslogConfigDir
	rsyslogConfigDir = "/etc/rsyslog.d"
	defer func() { rsyslogConfigDir = old }()

	assert.Equal(t, "/etc/rsyslog.d/10-loongcollector-my_config.conf", rsyslogConfigFilePath("my config"))
	assert.Equal(t, "/etc/rsyslog.d/10-loongcollector-abc.conf", rsyslogConfigFilePath("abc"))
}

func TestGenerateRsyslogConfig_DefaultFilter(t *testing.T) {
	cfg := &SyslogForwardingConfig{
		ConfigName: "cfg1",
		Protocol:   "tcp",
		Target:     "127.0.0.1",
		Port:       "9999",
		// Filters nil -> default *.*
	}
	got := generateRsyslogConfig(cfg)

	assert.Contains(t, got, "template(name=\"LC_TraditionalForwardFormat\"")
	assert.Contains(t, got, "*.* action(type=\"omfwd\"")
	assert.Contains(t, got, "template=\"LC_TraditionalForwardFormat\"")
	assert.Contains(t, got, "queue.type=\"LinkedList\"")
	assert.Contains(t, got, "queue.filename=\"omfwd-loongcollector-cfg1\"")
	assert.Contains(t, got, "queue.maxDiskSpace=\"1g\"")
	assert.Contains(t, got, "action.resumeRetryCount=\"-1\"")
	assert.Contains(t, got, "action.reportSuspension=\"on\"")
	assert.Contains(t, got, "target=\"127.0.0.1\" Port=\"9999\" Protocol=\"tcp\")")
}

func TestGenerateRsyslogConfig_CustomFilters(t *testing.T) {
	cfg := &SyslogForwardingConfig{
		ConfigName: "auth cfg",
		Protocol:   "udp",
		Target:     "10.0.0.2",
		Port:       "9514",
		Filters:    []string{"auth.warning", "kern.err", "daemon.info"},
	}
	got := generateRsyslogConfig(cfg)

	assert.Contains(t, got, "auth.warning;kern.err;daemon.info action(type=\"omfwd\"")
	assert.Contains(t, got, "target=\"10.0.0.2\" Port=\"9514\" Protocol=\"udp\")")
	// config name is sanitized in the queue filename
	assert.Contains(t, got, "queue.filename=\"omfwd-loongcollector-auth_cfg\"")
}

func TestGenerateRsyslogConfig_ConfigNameInjection(t *testing.T) {
	cfg := &SyslogForwardingConfig{
		ConfigName: "evil\n*.* action(type=\"omfwd\" target=\"attacker\" Port=\"514\" Protocol=\"tcp\")",
		Protocol:   "tcp",
		Target:     "127.0.0.1",
		Port:       "9999",
	}
	got := generateRsyslogConfig(cfg)

	// The crafted newline must be escaped, keeping the comment on a single line
	// so no extra rsyslog directive is injected.
	assert.NotContains(t, got, "# Config: evil\n")
	assert.Contains(t, got, `# Config: "evil\n*.* action`)
	// Only the legitimate action line should target 127.0.0.1; the injected
	// "attacker" target must remain inert inside the escaped comment.
	assert.Contains(t, got, "target=\"127.0.0.1\" Port=\"9999\" Protocol=\"tcp\")")
}

func TestWriteRsyslogConfig_ChangeDetection(t *testing.T) {
	dir := t.TempDir()
	old := rsyslogConfigDir
	rsyslogConfigDir = dir
	defer func() { rsyslogConfigDir = old }()

	const name = "testcfg"
	path := rsyslogConfigFilePath(name)

	// First write: changed, no previous file (nil oldContent).
	changed, oldContent, err := writeRsyslogConfig(name, "content-v1")
	require.NoError(t, err)
	assert.True(t, changed)
	assert.Nil(t, oldContent)
	data, err := os.ReadFile(path)
	require.NoError(t, err)
	assert.Equal(t, "content-v1", string(data))

	// Same content: unchanged, returns previous content.
	changed, oldContent, err = writeRsyslogConfig(name, "content-v1")
	require.NoError(t, err)
	assert.False(t, changed)
	require.NotNil(t, oldContent)
	assert.Equal(t, "content-v1", *oldContent)

	// Different content: changed, returns previous content.
	changed, oldContent, err = writeRsyslogConfig(name, "content-v2")
	require.NoError(t, err)
	assert.True(t, changed)
	require.NotNil(t, oldContent)
	assert.Equal(t, "content-v1", *oldContent)
	data, err = os.ReadFile(path)
	require.NoError(t, err)
	assert.Equal(t, "content-v2", string(data))
}

func TestWriteRsyslogConfig_UnreadableExistingFile(t *testing.T) {
	dir := t.TempDir()
	old := rsyslogConfigDir
	rsyslogConfigDir = dir
	defer func() { rsyslogConfigDir = old }()

	const name = "unreadablecfg"
	path := rsyslogConfigFilePath(name)

	// Make the config path a directory so os.ReadFile fails with a non-NotExist
	// error (EISDIR) regardless of the test user (root bypasses file perms).
	require.NoError(t, os.Mkdir(path, 0755))

	changed, oldContent, err := writeRsyslogConfig(name, "content-v1")
	require.Error(t, err)
	assert.False(t, changed)
	assert.Nil(t, oldContent)

	// The existing path must be left untouched, not overwritten.
	fi, statErr := os.Stat(path)
	require.NoError(t, statErr)
	assert.True(t, fi.IsDir())
}

func TestRestoreRsyslogConfig(t *testing.T) {
	dir := t.TempDir()
	old := rsyslogConfigDir
	rsyslogConfigDir = dir
	defer func() { rsyslogConfigDir = old }()

	const name = "restorecfg"
	path := rsyslogConfigFilePath(name)

	// Restore with nil oldContent (file did not exist before) removes the file.
	require.NoError(t, os.WriteFile(path, []byte("bad"), 0644))
	require.NoError(t, restoreRsyslogConfig(name, nil))
	_, err := os.Stat(path)
	assert.True(t, os.IsNotExist(err))

	// Restore with a non-nil empty oldContent (previously an empty file) keeps an
	// empty file rather than deleting it.
	require.NoError(t, os.WriteFile(path, []byte("new-bad"), 0644))
	empty := ""
	require.NoError(t, restoreRsyslogConfig(name, &empty))
	data, err := os.ReadFile(path)
	require.NoError(t, err)
	assert.Empty(t, string(data))

	// Restore with oldContent rewrites the previous content.
	require.NoError(t, os.WriteFile(path, []byte("new-bad"), 0644))
	good := "good-old"
	require.NoError(t, restoreRsyslogConfig(name, &good))
	data, err = os.ReadFile(path)
	require.NoError(t, err)
	assert.Equal(t, "good-old", string(data))
}

func TestRemoveRsyslogConfig(t *testing.T) {
	dir := t.TempDir()
	old := rsyslogConfigDir
	rsyslogConfigDir = dir
	defer func() { rsyslogConfigDir = old }()

	const name = "removecfg"
	path := rsyslogConfigFilePath(name)

	// Removing a non-existent file is a no-op (no error, removed=false).
	removed, err := removeRsyslogConfig(name)
	require.NoError(t, err)
	assert.False(t, removed)

	// Removing an existing file deletes it and reports removed=true.
	require.NoError(t, os.WriteFile(path, []byte("x"), 0644))
	removed, err = removeRsyslogConfig(name)
	require.NoError(t, err)
	assert.True(t, removed)
	_, statErr := os.Stat(path)
	assert.True(t, os.IsNotExist(statErr))
}

func TestValidateRsyslogFilters(t *testing.T) {
	// Valid: empty, nil, and normal facility.severity selectors.
	require.NoError(t, validateRsyslogFilters(nil))
	require.NoError(t, validateRsyslogFilters([]string{}))
	require.NoError(t, validateRsyslogFilters([]string{"*.*", "auth.warning", "kern.err"}))

	// Invalid: a newline would break out of the selector line.
	require.Error(t, validateRsyslogFilters([]string{"auth.warning\n*.* action(type=\"omfwd\")"}))
	require.Error(t, validateRsyslogFilters([]string{"ok.info", "bad\rentry"}))
}

func TestParseRsyslogMajorVersion(t *testing.T) {
	cases := []struct {
		name    string
		output  string
		want    int
		wantErr bool
	}{
		{"v8", "rsyslogd 8.2102.0-15.el8 (aka 2021.02)\n...", 8, false},
		{"v7", "rsyslogd 7.4.7, compiled with:\n...", 7, false},
		{"v10", "rsyslogd 10.0.1", 10, false},
		{"garbage", "some unrelated output", 0, true},
		{"empty", "", 0, true},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			got, err := parseRsyslogMajorVersion(c.output)
			if c.wantErr {
				assert.Error(t, err)
				return
			}
			require.NoError(t, err)
			assert.Equal(t, c.want, got)
		})
	}
}

func TestDetectInitSystem(t *testing.T) {
	old := systemdMarkerPath
	defer func() { systemdMarkerPath = old }()

	// Marker exists and is a directory -> systemd.
	dir := t.TempDir()
	systemdMarkerPath = filepath.Join(dir, "systemd-system")
	require.NoError(t, os.Mkdir(systemdMarkerPath, 0755))
	name, args := detectInitSystem()
	assert.Equal(t, "systemctl", name)
	assert.Equal(t, []string{"restart", "rsyslog"}, args)

	// Marker missing -> SysV.
	systemdMarkerPath = filepath.Join(dir, "does-not-exist")
	name, args = detectInitSystem()
	assert.Equal(t, "service", name)
	assert.Equal(t, []string{"rsyslog", "restart"}, args)
}
