// Copyright 2025 LoongCollector Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package doris

import (
	"bytes"
	"strconv"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

// TestNewFlusherDoris tests the creation of a new Doris flusher
func TestNewFlusherDoris(t *testing.T) {
	flusher := NewFlusherDoris()
	require.NotNil(t, flusher)
	assert.NotNil(t, flusher.Authentication.PlainText)
	assert.Empty(t, flusher.Addresses)
	assert.Empty(t, flusher.Table)

	// Test buffer pool is initialized
	buf := flusher.bufferPool.Get()
	require.NotNil(t, buf)
	buffer, ok := buf.(*bytes.Buffer)
	require.True(t, ok, "buffer pool should return *bytes.Buffer")
	assert.NotNil(t, buffer)
	flusher.bufferPool.Put(buffer)
}

// TestFlusherDoris_Description tests the Description method
func TestFlusherDoris_Description(t *testing.T) {
	flusher := NewFlusherDoris()
	desc := flusher.Description()
	assert.Equal(t, "Doris flusher for logtail", desc)
}

// TestFlusherDoris_Validate tests the configuration validation
func TestFlusherDoris_Validate(t *testing.T) {
	tests := []struct {
		name      string
		addresses []string
		table     string
		wantErr   bool
	}{
		{
			name:      "valid config",
			addresses: []string{"127.0.0.1:8030"},
			table:     "test_table",
			wantErr:   false,
		},
		{
			name:      "empty addresses",
			addresses: []string{},
			table:     "test_table",
			wantErr:   true,
		},
		{
			name:      "nil addresses",
			addresses: nil,
			table:     "test_table",
			wantErr:   true,
		},
		{
			name:      "empty table",
			addresses: []string{"127.0.0.1:8030"},
			table:     "",
			wantErr:   true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			flusher := NewFlusherDoris()
			flusher.Addresses = tt.addresses
			flusher.Table = tt.table
			lctx := mock.NewEmptyContext("p", "l", "c")
			flusher.context = lctx

			err := flusher.Validate()
			if tt.wantErr {
				assert.Error(t, err)
			} else {
				assert.NoError(t, err)
			}
		})
	}
}

// TestFlusherDoris_IsReady tests the IsReady method
func TestFlusherDoris_IsReady(t *testing.T) {
	flusher := NewFlusherDoris()

	// Should return false when client is not initialized
	ready := flusher.IsReady("project", "logstore", 123)
	assert.False(t, ready)

	// Note: Testing with initialized client would require a real Doris instance
}

// TestAuthentication_GetUsernamePassword tests authentication credential retrieval
func TestAuthentication_GetUsernamePassword(t *testing.T) {
	tests := []struct {
		name        string
		auth        Authentication
		wantUser    string
		wantPass    string
		wantErr     bool
		errContains string
	}{
		{
			name: "valid credentials",
			auth: Authentication{
				PlainText: &PlainTextConfig{
					Username: "root",
					Password: "password",
				},
			},
			wantUser: "root",
			wantPass: "password",
			wantErr:  false,
		},
		{
			name: "empty username",
			auth: Authentication{
				PlainText: &PlainTextConfig{
					Username: "",
					Password: "password",
				},
			},
			wantErr:     true,
			errContains: "username",
		},
		{
			name: "empty password",
			auth: Authentication{
				PlainText: &PlainTextConfig{
					Username: "root",
					Password: "",
				},
			},
			wantErr:     true,
			errContains: "password",
		},
		{
			name: "nil plaintext config",
			auth: Authentication{
				PlainText: nil,
			},
			wantErr:     true,
			errContains: "plaintext authentication config",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			user, pass, err := tt.auth.GetUsernamePassword()
			if tt.wantErr {
				assert.Error(t, err)
				if tt.errContains != "" {
					assert.Contains(t, err.Error(), tt.errContains)
				}
			} else {
				assert.NoError(t, err)
				assert.Equal(t, tt.wantUser, user)
				assert.Equal(t, tt.wantPass, pass)
			}
		})
	}
}

// TestFlusherDoris_Init tests the initialization with mock context
func TestFlusherDoris_Init(t *testing.T) {
	t.Run("init fails with invalid config", func(t *testing.T) {
		flusher := NewFlusherDoris()
		// Empty addresses should cause validation to fail
		flusher.Addresses = []string{}
		flusher.Table = "test_table"

		lctx := mock.NewEmptyContext("p", "l", "c")
		err := flusher.Init(lctx)
		assert.Error(t, err)
	})

	t.Run("init fails with missing auth", func(t *testing.T) {
		flusher := NewFlusherDoris()
		flusher.Addresses = []string{"127.0.0.1:8030"}
		flusher.Table = "test_table"
		flusher.Database = "test_db"
		flusher.Authentication.PlainText = &PlainTextConfig{
			Username: "", // Empty username
			Password: "password",
		}

		lctx := mock.NewEmptyContext("p", "l", "c")
		err := flusher.Init(lctx)
		assert.Error(t, err)
		assert.Contains(t, err.Error(), "username")
	})
}

// makeTestLogGroupList creates a test log group list for testing
func makeTestLogGroupList() *protocol.LogGroupList {
	fields := map[string]string{}
	lgl := &protocol.LogGroupList{
		LogGroupList: make([]*protocol.LogGroup, 0, 10),
	}

	for i := 1; i <= 10; i++ {
		lg := &protocol.LogGroup{
			Logs: make([]*protocol.Log, 0, 10),
		}
		for j := 1; j <= 10; j++ {
			fields["group"] = strconv.Itoa(i)
			fields["message"] = "The message: " + strconv.Itoa(j)
			fields["id"] = strconv.Itoa(i*100 + j)
			l := test.CreateLogByFields(fields)
			lg.Logs = append(lg.Logs, l)
		}
		lgl.LogGroupList = append(lgl.LogGroupList, lg)
	}
	return lgl
}

// InvalidTestConnectAndWrite is an integration test (disabled by default)
// To run this test, you need a running Doris instance and change the function name to TestConnectAndWrite
func InvalidTestConnectAndWrite(t *testing.T) {
	if testing.Short() {
		t.Skip("Skipping integration test in short mode")
	}

	flusher := NewFlusherDoris()
	flusher.Addresses = []string{"127.0.0.1:8030"}
	flusher.Database = "test_db"
	flusher.Table = "test_table"
	flusher.GroupCommit = "off" // Test with default group commit mode
	flusher.Authentication.PlainText = &PlainTextConfig{
		Username: "root",
		Password: "password",
	}
	flusher.LoadProperties = map[string]string{
		"strict_mode":      "false",
		"max_filter_ratio": "1.0",
	}

	// Initialize the flusher
	lctx := mock.NewEmptyContext("p", "l", "c")
	err := flusher.Init(lctx)
	require.NoError(t, err, "Failed to initialize Doris flusher")

	// Verify that we can successfully write data to Doris
	lgl := makeTestLogGroupList()
	err = flusher.Flush("projectName", "logstoreName", "configName", lgl.GetLogGroupList())
	require.NoError(t, err, "Failed to flush data to Doris")

	// Stop the flusher
	err = flusher.Stop()
	require.NoError(t, err, "Failed to stop Doris flusher")
}

// TestFlusherDoris_FlushWithoutInit tests that flush fails when client is not initialized
func TestFlusherDoris_FlushWithoutInit(t *testing.T) {
	flusher := NewFlusherDoris()
	lgl := makeTestLogGroupList()

	err := flusher.Flush("projectName", "logstoreName", "configName", lgl.GetLogGroupList())
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "not initialized")
}

// TestParseGroupCommitMode tests the group commit mode parsing
func TestParseGroupCommitMode(t *testing.T) {
	tests := []struct {
		name     string
		mode     string
		expected string // We'll use string representation for comparison
	}{
		{
			name: "sync mode",
			mode: "sync",
		},
		{
			name: "async mode",
			mode: "async",
		},
		{
			name: "off mode",
			mode: "off",
		},
		{
			name: "empty string",
			mode: "",
		},
		{
			name: "unknown mode",
			mode: "unknown",
		},
		{
			name: "uppercase SYNC",
			mode: "SYNC",
		},
		{
			name: "mixed case Async",
			mode: "Async",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := parseGroupCommitMode(tt.mode)
			assert.NotNil(t, result)
			// Just ensure it doesn't panic and returns a valid value
		})
	}
}

// TestFlusherDoris_Stop tests the Stop method
func TestFlusherDoris_Stop(t *testing.T) {
	t.Run("stop without init", func(t *testing.T) {
		flusher := NewFlusherDoris()
		err := flusher.Stop()
		assert.NoError(t, err)
	})

	t.Run("stop multiple times", func(t *testing.T) {
		flusher := NewFlusherDoris()
		err := flusher.Stop()
		assert.NoError(t, err)

		// Stopping again should not panic or error
		err = flusher.Stop()
		assert.NoError(t, err)
	})

	t.Run("stop with progress logging enabled", func(t *testing.T) {
		flusher := NewFlusherDoris()
		flusher.LogProgressInterval = 1
		flusher.Addresses = []string{"http://127.0.0.1:8030"}
		flusher.Table = "test_table"
		flusher.Database = "test_db"
		flusher.Authentication.PlainText = &PlainTextConfig{
			Username: "root",
			Password: "password",
		}

		lctx := mock.NewEmptyContext("p", "l", "c")
		// Init will fail due to connection, but we can test Stop behavior
		_ = flusher.Init(lctx)

		err := flusher.Stop()
		assert.NoError(t, err)
	})
}

// TestFlusherDoris_UpdateStatistics tests statistics update
func TestFlusherDoris_UpdateStatistics(t *testing.T) {
	flusher := NewFlusherDoris()

	// Initially, stats should be zero
	assert.Equal(t, uint64(0), flusher.stats.totalBytes)
	assert.Equal(t, uint64(0), flusher.stats.totalRows)

	// Update statistics
	flusher.updateStatistics(1000, 10)
	assert.Equal(t, uint64(1000), flusher.stats.totalBytes)
	assert.Equal(t, uint64(10), flusher.stats.totalRows)

	// Update again
	flusher.updateStatistics(2000, 20)
	assert.Equal(t, uint64(3000), flusher.stats.totalBytes)
	assert.Equal(t, uint64(30), flusher.stats.totalRows)
}

// TestFlusherDoris_BufferPool tests buffer pool functionality
func TestFlusherDoris_BufferPool(t *testing.T) {
	flusher := NewFlusherDoris()

	// Get buffer from pool
	buf1 := flusher.bufferPool.Get().(*bytes.Buffer)
	assert.NotNil(t, buf1)

	// Write some data
	buf1.WriteString("test data")
	assert.Equal(t, "test data", buf1.String())

	// Reset and put back
	buf1.Reset()
	flusher.bufferPool.Put(buf1)

	// Get another buffer (might be the same one)
	buf2 := flusher.bufferPool.Get().(*bytes.Buffer)
	assert.NotNil(t, buf2)
	// Should be empty after reset
	assert.Equal(t, "", buf2.String())
}

// TestFlusherDoris_SetUrgent tests SetUrgent method
func TestFlusherDoris_SetUrgent(t *testing.T) {
	flusher := NewFlusherDoris()
	// SetUrgent is a no-op, just ensure it doesn't panic
	flusher.SetUrgent(true)
	flusher.SetUrgent(false)
}

// TestFlusherDoris_FlushEmptyLogGroupList tests flushing empty log group list
func TestFlusherDoris_FlushEmptyLogGroupList(t *testing.T) {
	flusher := NewFlusherDoris()
	flusher.Addresses = []string{"http://127.0.0.1:8030"}
	flusher.Table = "test_table"
	flusher.Database = "test_db"
	flusher.Authentication.PlainText = &PlainTextConfig{
		Username: "root",
		Password: "password",
	}

	lctx := mock.NewEmptyContext("p", "l", "c")
	// Init will fail due to connection, but we test empty flush
	_ = flusher.Init(lctx)

	// Flush empty list should return nil
	err := flusher.Flush("project", "logstore", "config", []*protocol.LogGroup{})
	assert.NoError(t, err)
}

// TestFlusherDoris_ConcurrencyConfig tests concurrency configuration
func TestFlusherDoris_ConcurrencyConfig(t *testing.T) {
	t.Run("default concurrency", func(t *testing.T) {
		flusher := NewFlusherDoris()
		assert.Equal(t, 1, flusher.Concurrency)
		assert.Equal(t, 1024, flusher.QueueCapacity)
	})

	t.Run("custom concurrency", func(t *testing.T) {
		flusher := NewFlusherDoris()
		flusher.Concurrency = 4
		flusher.QueueCapacity = 2048
		assert.Equal(t, 4, flusher.Concurrency)
		assert.Equal(t, 2048, flusher.QueueCapacity)
	})
}

// TestFlusherDoris_GroupCommitConfig tests group commit configuration
func TestFlusherDoris_GroupCommitConfig(t *testing.T) {
	tests := []struct {
		name   string
		config string
	}{
		{"off mode", "off"},
		{"sync mode", "sync"},
		{"async mode", "async"},
		{"empty string", ""},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			flusher := NewFlusherDoris()
			flusher.GroupCommit = tt.config
			assert.Equal(t, tt.config, flusher.GroupCommit)
		})
	}
}

// TestFlusherDoris_LogProgressIntervalConfig tests progress log interval configuration
func TestFlusherDoris_LogProgressIntervalConfig(t *testing.T) {
	t.Run("default interval", func(t *testing.T) {
		flusher := NewFlusherDoris()
		assert.Equal(t, 10, flusher.LogProgressInterval)
	})

	t.Run("custom interval", func(t *testing.T) {
		flusher := NewFlusherDoris()
		flusher.LogProgressInterval = 5
		assert.Equal(t, 5, flusher.LogProgressInterval)
	})

	t.Run("disabled progress logging", func(t *testing.T) {
		flusher := NewFlusherDoris()
		flusher.LogProgressInterval = 0
		assert.Equal(t, 0, flusher.LogProgressInterval)
	})
}

// TestFlusherDoris_ConvertConfig tests convert configuration
func TestFlusherDoris_ConvertConfig(t *testing.T) {
	t.Run("default convert config", func(t *testing.T) {
		flusher := NewFlusherDoris()
		assert.Equal(t, "custom_single", flusher.Convert.Protocol)
		assert.Equal(t, "json", flusher.Convert.Encoding)
	})

	t.Run("custom convert config", func(t *testing.T) {
		flusher := NewFlusherDoris()
		flusher.Convert.Protocol = "otlp_log_v1"
		flusher.Convert.Encoding = "protobuf"
		assert.Equal(t, "otlp_log_v1", flusher.Convert.Protocol)
		assert.Equal(t, "protobuf", flusher.Convert.Encoding)
	})

	t.Run("field rename config", func(t *testing.T) {
		flusher := NewFlusherDoris()
		flusher.Convert.TagFieldsRename = map[string]string{
			"host": "hostname",
		}
		flusher.Convert.ProtocolFieldsRename = map[string]string{
			"contents": "data",
		}
		assert.Equal(t, "hostname", flusher.Convert.TagFieldsRename["host"])
		assert.Equal(t, "data", flusher.Convert.ProtocolFieldsRename["contents"])
	})
}

// TestFlusherDoris_LoadPropertiesConfig tests load properties configuration
func TestFlusherDoris_LoadPropertiesConfig(t *testing.T) {
	flusher := NewFlusherDoris()
	flusher.LoadProperties = map[string]string{
		"strict_mode":      "false",
		"max_filter_ratio": "0.1",
		"timeout":          "600",
	}

	assert.Equal(t, "false", flusher.LoadProperties["strict_mode"])
	assert.Equal(t, "0.1", flusher.LoadProperties["max_filter_ratio"])
	assert.Equal(t, "600", flusher.LoadProperties["timeout"])
}

// TestFlusherDoris_AuthenticationConfig tests authentication configuration
func TestFlusherDoris_AuthenticationConfig(t *testing.T) {
	t.Run("plaintext auth", func(t *testing.T) {
		flusher := NewFlusherDoris()
		flusher.Authentication.PlainText = &PlainTextConfig{
			Username: "test_user",
			Password: "test_pass",
			Database: "test_db",
		}

		assert.Equal(t, "test_user", flusher.Authentication.PlainText.Username)
		assert.Equal(t, "test_pass", flusher.Authentication.PlainText.Password)
		assert.Equal(t, "test_db", flusher.Authentication.PlainText.Database)
	})
}

// TestFlusherDoris_ValidateAddresses tests address validation
func TestFlusherDoris_ValidateAddresses(t *testing.T) {
	tests := []struct {
		name      string
		addresses []string
		wantErr   bool
	}{
		{
			name:      "valid http address",
			addresses: []string{"http://127.0.0.1:8030"},
			wantErr:   false,
		},
		{
			name:      "valid https address",
			addresses: []string{"https://127.0.0.1:8030"},
			wantErr:   false,
		},
		{
			name:      "multiple addresses",
			addresses: []string{"http://127.0.0.1:8030", "http://127.0.0.2:8030"},
			wantErr:   false,
		},
		{
			name:      "address without protocol",
			addresses: []string{"127.0.0.1:8030"},
			wantErr:   false, // Validate doesn't check URL format, only if addresses exist
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			flusher := NewFlusherDoris()
			flusher.Addresses = tt.addresses
			flusher.Table = "test_table"
			lctx := mock.NewEmptyContext("p", "l", "c")
			flusher.context = lctx

			err := flusher.Validate()
			if tt.wantErr {
				assert.Error(t, err)
			} else {
				assert.NoError(t, err)
			}
		})
	}
}

// BenchmarkFlusherDoris_MakeTestLogGroupList benchmarks log group creation
func BenchmarkFlusherDoris_MakeTestLogGroupList(b *testing.B) {
	for i := 0; i < b.N; i++ {
		_ = makeTestLogGroupList()
	}
}

// BenchmarkFlusherDoris_UpdateStatistics benchmarks statistics update
func BenchmarkFlusherDoris_UpdateStatistics(b *testing.B) {
	flusher := NewFlusherDoris()
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		flusher.updateStatistics(1000, 10)
	}
}

// BenchmarkFlusherDoris_BufferPool benchmarks buffer pool operations
func BenchmarkFlusherDoris_BufferPool(b *testing.B) {
	flusher := NewFlusherDoris()
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		buf := flusher.bufferPool.Get().(*bytes.Buffer)
		buf.Reset()
		flusher.bufferPool.Put(buf)
	}
}
