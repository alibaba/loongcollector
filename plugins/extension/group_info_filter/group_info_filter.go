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

package groupinfofilter

import (
	"regexp"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
)

// ensure ExtensionGroupInfoFilter implements the extensions.FlushInterceptor interface
var _ extensions.FlushInterceptor = (*ExtensionGroupInfoFilter)(nil)

type filterCondition struct {
	Pattern string
	Reverse bool
	Reg     *regexp.Regexp
}

type ExtensionGroupInfoFilter struct {
	Tags  map[string]*filterCondition // match config for tags, the map value support regex
	Metas map[string]*filterCondition // match config for metas, the map value support regex

	tagsPattern  map[string]*filterCondition
	metasPattern map[string]*filterCondition
	context      pipeline.Context
}

func (e *ExtensionGroupInfoFilter) Description() string {
	return "filter extension that test the GroupInfo if match the config"
}

func (e *ExtensionGroupInfoFilter) Init(context pipeline.Context) error {
	e.context = context
	e.tagsPattern = map[string]*filterCondition{}
	e.metasPattern = map[string]*filterCondition{}

	for k, v := range e.Tags {
		reg, err := regexp.Compile(v.Pattern)
		if err != nil {
			logger.Error(context.GetRuntimeContext(), "EXTENSION_FILTER_ALARM", "regex compile tags err, pattern", v, "error", err)
			return err
		}
		v.Reg = reg
		e.tagsPattern[k] = v
	}

	for k, v := range e.Metas {
		reg, err := regexp.Compile(v.Pattern)
		if err != nil {
			logger.Error(context.GetRuntimeContext(), "EXTENSION_FILTER_ALARM", "regex compile metas err, pattern", v, "error", err)
			return err
		}
		v.Reg = reg
		e.metasPattern[k] = v
	}

	return nil
}

// Intercept returns original event if matched, nil if not
func (e *ExtensionGroupInfoFilter) Intercept(group *models.PipelineGroupEvents) *models.PipelineGroupEvents {
	if group == nil {
		return nil
	}

	for k, r := range e.metasPattern {
		v := group.Group.GetMetadata().Get(k)
		isMatch := r.Reg.MatchString(v)
		if isMatch != !r.Reverse {
			return nil
		}
	}

	for k, r := range e.tagsPattern {
		v := group.Group.GetTags().Get(k)
		isMatch := r.Reg.MatchString(v)
		if isMatch != !r.Reverse {
			return nil
		}
	}

	return group
}

func (e *ExtensionGroupInfoFilter) Stop() error {
	return nil
}

func init() {
	pipeline.AddExtensionCreator("ext_groupinfo_filter", func() pipeline.Extension {
		return &ExtensionGroupInfoFilter{}
	})
}
