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

package flags

import (
	"flag"
	"os"
	"testing"
)

func TestLoadEnvToFlags(t *testing.T) {
	tests := []struct {
		name     string
		envs     map[string]string
		flags    map[string]interface{}
		expected map[string]interface{}
	}{
		{
			name: "string flag",
			envs: map[string]string{
				LoongcollectorEnvPrefix + "CONFIG": "/etc/ilogtail/config.json",
			},
			flags: map[string]interface{}{
				"config": "",
			},
			expected: map[string]interface{}{
				"config": "/etc/ilogtail/config.json",
			},
		},
		{
			name: "bool flag",
			envs: map[string]string{
				LoongcollectorEnvPrefix + "DEBUG": "true",
			},
			flags: map[string]interface{}{
				"debug": false,
			},
			expected: map[string]interface{}{
				"debug": true,
			},
		},
		{
			name: "int flag",
			envs: map[string]string{
				LoongcollectorEnvPrefix + "PORT": "8080",
			},
			flags: map[string]interface{}{
				"port": 0,
			},
			expected: map[string]interface{}{
				"port": 8080,
			},
		},
		{
			name: "env not exist",
			envs: map[string]string{},
			flags: map[string]interface{}{
				"config": "default",
			},
			expected: map[string]interface{}{
				"config": "default",
			},
		},
		{
			name: "invalid bool value",
			envs: map[string]string{
				LoongcollectorEnvPrefix + "DEBUG": "invalid",
			},
			flags: map[string]interface{}{
				"debug": false,
			},
			expected: map[string]interface{}{
				"debug": false,
			},
		},
		{
			name: "invalid int value",
			envs: map[string]string{
				LoongcollectorEnvPrefix + "PORT": "invalid",
			},
			flags: map[string]interface{}{
				"port": 0,
			},
			expected: map[string]interface{}{
				"port": 0,
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Clean environment variables
			os.Clearenv()

			// Set environment variables
			for k, v := range tt.envs {
				os.Setenv(k, v)
			}

			// Create flags
			flag.CommandLine = flag.NewFlagSet(os.Args[0], flag.ExitOnError)
			for name, value := range tt.flags {
				switch v := value.(type) {
				case string:
					flag.String(name, v, "test flag")
				case bool:
					flag.Bool(name, v, "test flag")
				case int:
					flag.Int(name, v, "test flag")
				}
			}

			// Load environment variables to flags
			LoadEnvToFlags()

			found := false
			// Verify flag values
			flag.VisitAll(func(f *flag.Flag) {
				found = true
				expected := tt.expected[f.Name]
				switch v := expected.(type) {
				case string:
					if f.Value.String() != v {
						t.Errorf("flag %s = %s, want %s", f.Name, f.Value.String(), v)
					}
				case bool:
					if f.Value.String() != "true" && v || f.Value.String() != "false" && !v {
						t.Errorf("flag %s = %s, want %v", f.Name, f.Value.String(), v)
					}
				case int:
					if f.Value.String() != "0" && v == 0 || f.Value.String() != "8080" && v == 8080 {
						t.Errorf("flag %s = %s, want %d", f.Name, f.Value.String(), v)
					}
				}
			})
			if !found {
				t.Errorf("flag %s not found", tt)
			}
		})
	}
}
