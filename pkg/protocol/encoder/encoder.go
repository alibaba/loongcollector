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

package encoder

import (
	"fmt"
	"strings"

	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
	"github.com/alibaba/ilogtail/pkg/protocol/encoder/common"
	"github.com/alibaba/ilogtail/pkg/protocol/encoder/prometheus"
)

func NewEncoder(format string, options map[string]any) (extensions.Encoder, error) {
	switch strings.TrimSpace(strings.ToLower(format)) {
	case common.ProtocolPrometheus:
		var opt prometheus.Option
		if err := mapstructure.Decode(options, &opt); err != nil {
			return nil, err
		}
		return prometheus.NewPromEncoder(opt.SeriesLimit), nil

	default:
		return nil, fmt.Errorf("not supported encode format: %s", format)
	}
}
