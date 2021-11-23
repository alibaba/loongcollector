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
	"testing"

	"github.com/stretchr/testify/require"
)

func TestSimpleBackoff(t *testing.T) {
	backoff := newSimpleBackoff()
	b := backoff.(*simpleBackoff)
	require.NotEmpty(t, b)

	vals := []struct {
		cur     uint
		canQuit bool
	}{
		{1, false}, {2, false}, {4, false}, {8, false}, {16, false},
		{32, false}, {64, false}, {120, true}, {120, true}, {120, true},
	}
	for idx, val := range vals {
		require.Equal(t, val.cur, b.cur)
		b.Next()
		require.Equal(t, val.canQuit, b.CanQuit(), "%v:%v", idx, val)
	}

	b.Reset()
	require.Equal(t, b.cur, b.initial)
}
