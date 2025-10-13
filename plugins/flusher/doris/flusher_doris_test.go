// Copyright 2024 iLogtail Authors
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

package doris

import (
	"strconv"
	"testing"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// TestNewFlusherDoris tests the creation of a new Doris flusher
func TestNewFlusherDoris(t *testing.T) {
	flusher := NewFlusherDoris()
	require.NotNil(t, flusher)
	assert.NotNil(t, flusher.Authentication.PlainText)
	assert.Empty(t, flusher.Addresses)
	assert.Empty(t, flusher.Table)
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

// BenchmarkFlusherDoris_MakeTestLogGroupList benchmarks log group creation
func BenchmarkFlusherDoris_MakeTestLogGroupList(b *testing.B) {
	for i := 0; i < b.N; i++ {
		_ = makeTestLogGroupList()
	}
}
