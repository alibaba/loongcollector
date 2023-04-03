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

package pprof

import (
	"fmt"

	"github.com/pyroscope-io/pyroscope/pkg/storage/metadata"
	"github.com/pyroscope-io/pyroscope/pkg/storage/segment"
	"github.com/pyroscope-io/pyroscope/pkg/storage/tree"

	"github.com/alibaba/ilogtail/pkg/helper/profile"
)

type StackFrameFormatter interface {
	format(x *tree.Profile, fn *tree.Function, line *tree.Line) []byte
}

type Formatter struct {
}

func (Formatter) format(x *tree.Profile, fn *tree.Function, _ *tree.Line) []byte {
	return []byte(fmt.Sprintf("%s %s",
		x.StringTable[fn.Name],
		x.StringTable[fn.Filename],
	))
}

func filterKnownSamples(sampleTypes map[string]*tree.SampleTypeConfig) func(string) bool {
	return func(s string) bool {
		_, ok := sampleTypes[s]
		return ok
	}
}

type Parser struct {
	stackFrameFormatter StackFrameFormatter
	sampleTypesFilter   func(string) bool
	sampleTypes         map[string]*tree.SampleTypeConfig
	cache               tree.LabelsCache
}

func (p *Parser) getDisplayName(defaultName string) string {
	config, ok := p.sampleTypes[defaultName]
	if !ok || config.DisplayName == "" {
		return defaultName
	}
	return config.DisplayName
}

func (p *Parser) getAggregationType(name string, defaultval string) string {
	config, ok := p.sampleTypes[name]
	if !ok {
		return defaultval
	}
	switch config.Aggregation {
	case metadata.AverageAggregationType:
		return string(profile.AvgAggType)
	case metadata.SumAggregationType:
		return string(profile.SumAggType)
	default:
		return defaultval
	}
}

func (p *Parser) iterate(x *tree.Profile, fn func(vt *tree.ValueType, l tree.Labels, t *tree.Tree) (keep bool, err error)) error {
	c := make(tree.LabelsCache)
	p.readTrees(x, c, tree.NewFinder(x))
	for sampleType, entries := range c {
		if t, ok := x.ResolveSampleType(sampleType); ok {
			for h, e := range entries {
				keep, err := fn(t, e.Labels, e.Tree)
				if err != nil {
					return err
				}
				if !keep {
					c.Remove(sampleType, h)
				}
			}
		}
	}
	p.cache = c
	return nil
}

func (p *Parser) load(sampleType int64, labels tree.Labels) (*tree.Tree, bool) {
	e, ok := p.cache.Get(sampleType, labels.Hash())
	if !ok {
		return nil, false
	}
	return e.Tree, true
}

// readTrees generates trees from the profile populating c.
func (p *Parser) readTrees(x *tree.Profile, c tree.LabelsCache, f tree.Finder) {
	// SampleType value indexes.
	indexes := make([]int, 0, len(x.SampleType))
	// Corresponding type IDs used as the main cache keys.
	types := make([]int64, 0, len(x.SampleType))
	for i, s := range x.SampleType {
		if p.sampleTypesFilter != nil && p.sampleTypesFilter(x.StringTable[s.Type]) {
			indexes = append(indexes, i)
			types = append(types, s.Type)
		}
	}
	if len(indexes) == 0 {
		return
	}
	stack := make([][]byte, 0, 16)
	for _, s := range x.Sample {
		for i := len(s.LocationId) - 1; i >= 0; i-- {
			// Resolve stack.
			loc, ok := f.FindLocation(s.LocationId[i])
			if !ok {
				continue
			}
			// Multiple line indicates this location has inlined functions,
			// where the last entry represents the caller into which the
			// preceding entries were inlined.
			//
			// E.g., if memcpy() is inlined into printf:
			//    line[0].function_name == "memcpy"
			//    line[1].function_name == "printf"
			//
			// Therefore iteration goes in reverse order.
			for j := len(loc.Line) - 1; j >= 0; j-- {
				fn, ok := f.FindFunction(loc.Line[j].FunctionId)
				if !ok || x.StringTable[fn.Name] == "" {
					continue
				}
				sf := p.stackFrameFormatter.format(x, fn, loc.Line[j])
				stack = append(stack, sf)
			}
		}
		// Insert tree nodes.
		for i, vi := range indexes {
			v := uint64(s.Value[vi])
			if v == 0 {
				continue
			}
			// If the sample has ProfileID label, it belongs to an exemplar.
			if j := labelIndex(x, s.Label, segment.ProfileIDLabelName); j >= 0 {
				// Regardless of whether we should skip exemplars or not, the value
				// should be appended to the exemplar baseline profile (w/o ProfileID label).
				c.GetOrCreateTree(types[i], tree.CutLabel(s.Label, j)).InsertStack(stack, v)
			}
			c.GetOrCreateTree(types[i], s.Label).InsertStack(stack, v)
		}
		stack = stack[:0]
	}
}

func labelIndex(p *tree.Profile, labels tree.Labels, key string) int {
	for i, label := range labels {
		if n, ok := p.ResolveLabelName(label); ok && n == key {
			return i
		}
	}
	return -1
}
