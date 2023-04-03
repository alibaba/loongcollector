// Copyright 2023 iLogtail Authors
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

package profile

import (
	"github.com/stretchr/testify/require"

	"testing"
)

func TestFormatPositionAndName(t *testing.T) {
	str := "./node_modules/express/lib/router/index.js:process_params:338 /app/node_modules/express/lib/router/index.js"
	str = FormatPositionAndName(str, PyroscopeNodeJs)
	require.Equal(t, str, "./node_modules/express/lib/router/index.js:process_params:338 /app/node_modules/express/lib/router/index.js")
	str = "compress/flate.NewWriter /usr/local/go/src/compress/flate/deflate.go"
	str = FormatPositionAndName(str, PyroscopeGolang)
	require.Equal(t, str, "compress/flate.NewWriter /usr/local/go/src/compress/flate/deflate.go")
	str = "backtrace_rs.rs:23 - <pprof::backtrace::backtrace_rs::Trace as pprof::backtrace::Trace>::trace"
	str = FormatPositionAndName(str, PyroscopeRust)
	require.Equal(t, str, "pprof::backtrace::Trace>::trace backtrace_rs.rs:23")
	str = "System.Threading.Tasks!Task.InternalWaitCore System.Private.CoreLib"
	str = FormatPositionAndName(str, Unknown)
	require.Equal(t, str, "System.Threading.Tasks!Task.InternalWaitCore System.Private.CoreLib")
	str = "/usr/local/bundle/gems/pyroscope-0.3.0-x86_64-linux/lib/pyroscope.rb:63 - tag_wrapper"
	str = FormatPositionAndName(str, PyroscopeRuby)
	require.Equal(t, str, "tag_wrapper /usr/local/bundle/gems/pyroscope-0.3.0-x86_64-linux/lib/pyroscope.rb:63")
	str = "lib/utility/utility.py:38 - find_nearest_vehicle"
	str = FormatPositionAndName(str, PyroscopePython)
	require.Equal(t, str, "find_nearest_vehicle lib/utility/utility.py:38")
	str = "libjvm.so.AdvancedThresholdPolicy::method_back_branch_event"
	str = FormatPositionAndName(str, PyroscopeJava)
	require.Equal(t, str, "libjvm.so.AdvancedThresholdPolicy::method_back_branch_event")
	str = "/usr/lib/systemd/systemd+0x93242"
	str = FormatPositionAndName(str, PyroscopeEbpf)
	require.Equal(t, str, "/usr/lib/systemd/systemd+0x93242")
	str = "<internal> - sleep"
	str = FormatPositionAndName(str, PyroscopePhp)
	require.Equal(t, str, "sleep <internal>")
}
