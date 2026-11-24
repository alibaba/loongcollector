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
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"mime/multipart"
	"strconv"
	"strings"
	"time"

	"github.com/cespare/xxhash/v2"
	"github.com/pyroscope-io/pyroscope/pkg/convert/pprof"
	"github.com/pyroscope-io/pyroscope/pkg/storage/metadata"
	"github.com/pyroscope-io/pyroscope/pkg/storage/segment"
	"github.com/pyroscope-io/pyroscope/pkg/storage/tree"
	"github.com/pyroscope-io/pyroscope/pkg/util/form"

	"github.com/alibaba/ilogtail/pkg/helper/profile"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

const (
	formFieldProfile, formFieldSampleTypeConfig, formFieldPreviousProfile = "profile", "sample_type_config", "prev_profile"
)

var DefaultSampleTypeMapping = map[string]*tree.SampleTypeConfig{
	"samples": {
		Units:       metadata.SamplesUnits,
		DisplayName: "cpu",
		Sampled:     true,
	},
	"inuse_objects": {
		Units:       metadata.ObjectsUnits,
		Aggregation: "avg",
	},
	"alloc_objects": {
		Units:      metadata.ObjectsUnits,
		Cumulative: true,
	},
	"inuse_space": {
		Units:       metadata.BytesUnits,
		Aggregation: "avg",
	},
	"alloc_space": {
		Units:      metadata.BytesUnits,
		Cumulative: true,
	},
	"goroutine": {
		DisplayName: "goroutines",
		Units:       metadata.GoroutinesUnits,
		Aggregation: "avg",
	},
	"contentions": {
		DisplayName: "mutex_count",
		Units:       metadata.LockSamplesUnits,
		Cumulative:  true,
	},
	"delay": {
		DisplayName: "mutex_duration",
		Units:       metadata.LockNanosecondsUnits,
		Cumulative:  true,
	},
}

type RawProfile struct {
	rawData             []byte
	formDataContentType string
	profile             []byte
	previousProfile     []byte
	sampleTypeConfig    map[string]*tree.SampleTypeConfig
	parser              *Parser
	pushMode            bool
}

func NewRawProfileByPull(current, pre []byte, config map[string]*tree.SampleTypeConfig) *RawProfile {
	return &RawProfile{
		profile:         current,
		previousProfile: pre,
	}
}

func NewRawProfile(data []byte, format string) *RawProfile {
	return &RawProfile{
		rawData:             data,
		formDataContentType: format,
		pushMode:            true,
	}
}

func (r *RawProfile) Parse(ctx context.Context, meta *profile.Meta, tags map[string]string) ([]models.PipelineEvent, error) {
	var events []models.PipelineEvent
	if err := r.doParse(ctx, meta, r.profileLogCallback(meta, tags, &events)); err != nil {
		return nil, err
	}
	return events, nil
}

func (r *RawProfile) doParse(ctx context.Context, meta *profile.Meta, cb profile.CallbackFunc) error {
	if r.pushMode {
		if err := r.extractProfileRaw(); err != nil {
			return fmt.Errorf("cannot extract profile: %w", err)
		}
	}

	if len(r.profile) == 0 {
		return errors.New("empty profile")
	}

	if meta.SampleRate > 0 {
		meta.Tags["_sample_rate_"] = strconv.FormatUint(uint64(meta.SampleRate), 10)
	}
	if r.parser == nil {
		if r.sampleTypeConfig == nil {
			if logger.DebugFlag() {
				var keys []string
				for k := range r.sampleTypeConfig {
					keys = append(keys, k)
				}
				logger.Debug(ctx, "pprof default sampleTypeConfig: ", r.sampleTypeConfig == nil, "config:", strings.Join(keys, ","))
			}
			r.sampleTypeConfig = DefaultSampleTypeMapping
		}
		r.parser = &Parser{
			stackFrameFormatter: Formatter{},
			sampleTypesFilter:   filterKnownSamples(r.sampleTypeConfig),
			sampleTypes:         r.sampleTypeConfig,
		}
		if len(r.previousProfile) > 0 {
			filter := r.parser.sampleTypesFilter
			r.parser.sampleTypesFilter = func(s string) bool {
				if filter != nil {
					return filter(s) && r.parser.sampleTypes[s].Cumulative
				}
				return r.parser.sampleTypes[s].Cumulative
			}
			err := pprof.DecodePool(bytes.NewReader(r.previousProfile), func(tf *tree.Profile) error {
				if err := r.extractLogs(ctx, tf, meta, cb); err != nil {
					return err
				}
				return nil
			})
			if err != nil {
				return err

			}

		}
	}

	return pprof.DecodePool(bytes.NewReader(r.profile), func(tf *tree.Profile) error {

		if err := r.extractLogs(ctx, tf, meta, cb); err != nil {
			return err
		}
		return nil
	})
}

func sampleRate(p *tree.Profile) int64 {
	if p.Period <= 0 || p.PeriodType == nil {
		return 0
	}
	sampleUnit := time.Nanosecond
	switch p.StringTable[p.PeriodType.Unit] {
	case "microseconds":
		sampleUnit = time.Microsecond
	case "milliseconds":
		sampleUnit = time.Millisecond
	case "seconds":
		sampleUnit = time.Second
	}
	return p.Period * sampleUnit.Nanoseconds()
}

func (r *RawProfile) extractLogs(ctx context.Context, tp *tree.Profile, meta *profile.Meta, cb profile.CallbackFunc) error {

	stackMap := make(map[uint64]*profile.Stack)
	valMap := make(map[uint64][]uint64)
	labelMap := make(map[uint64]map[string]string)
	typeMap := make(map[uint64][]string)
	unitMap := make(map[uint64][]string)
	aggtypeMap := make(map[uint64][]string)

	if len(tp.SampleType) > 0 {
		meta.Units = profile.Units(tp.StringTable[tp.SampleType[0].Type])
	}
	p := r.parser
	err := p.iterate(tp, func(vt *tree.ValueType, tl tree.Labels, t *tree.Tree) (keep bool, err error) {
		if len(tp.StringTable) <= int(vt.Type) || len(tp.StringTable) <= int(vt.Unit) {
			return true, errors.New("invalid type or unit")
		}
		stype := tp.StringTable[vt.Type]
		sunit := tp.StringTable[vt.Unit]
		sconfig, ok := p.sampleTypes[stype]
		if !ok {
			return false, errors.New("unknown type")
		}
		if sconfig.Cumulative {
			prev, found := p.load(vt.Type, tl)
			if !found {
				// Keep the current entry in cache.
				return true, nil
			}
			// Take diff with the previous tree.
			// The result is written to prev, t is not changed.
			t = prev.Diff(t)
		}
		var sampleDuration int64
		if sconfig.Sampled {
			sampleDuration = sampleRate(tp)
		}
		t.IterateStacks(func(name string, self uint64, stack []string) {
			if name == "" {
				return
			}
			id := xxhash.Sum64String(strings.Join(stack, ""))
			stackMap[id] = &profile.Stack{
				Name:  profile.FormatPositionAndName(name, profile.FormatType(meta.SpyName)),
				Stack: profile.FormatPostionAndNames(stack[1:], profile.FormatType(meta.SpyName)),
			}
			aggtypeMap[id] = append(aggtypeMap[id], p.getAggregationType(stype, string(meta.AggregationType)))
			typeMap[id] = append(typeMap[id], p.getDisplayName(stype))
			if sconfig.Sampled && sampleDuration != 0 && stype == string(profile.SamplesUnits) {
				sunit = string(profile.NanosecondsUnit)
				self *= uint64(sampleDuration)
			}
			unitMap[id] = append(unitMap[id], sunit)
			valMap[id] = append(valMap[id], self)
			labelMap[id] = buildKey(meta.Tags, tl, tp.StringTable).Labels()
		})
		return true, nil
	})
	if err != nil {
		return fmt.Errorf("iterate profile tree error: %w", err)
	}
	for id, fs := range stackMap {
		if len(valMap[id]) == 0 || len(typeMap[id]) == 0 || len(unitMap[id]) == 0 || len(aggtypeMap[id]) == 0 {
			logger.Warning(ctx, selfmonitor.PprofProfileAlarm, "stack don't have enough meta or values", fs)
			continue
		}
		if tp.GetTimeNanos() != 0 {
			cb(id, fs, valMap[id], typeMap[id], unitMap[id], aggtypeMap[id], tp.GetTimeNanos(), tp.GetTimeNanos()+tp.GetDurationNanos(), labelMap[id])
		} else {
			cb(id, fs, valMap[id], typeMap[id], unitMap[id], aggtypeMap[id], meta.StartTime.UnixNano(), meta.EndTime.UnixNano(), labelMap[id])
		}
	}
	return nil
}

func (r *RawProfile) profileLogCallback(meta *profile.Meta, tags map[string]string, events *[]models.PipelineEvent) profile.CallbackFunc {
	profileID := profile.GetProfileID(meta)
	return func(id uint64, stack *profile.Stack, vals []uint64, types, units, aggs []string, startTime, endTime int64, labels map[string]string) {
		for key, value := range tags {
			labels[key] = value
		}
		encodedLabels, _ := json.Marshal(labels)
		labelText := string(encodedLabels)
		seconds := startTime / int64(time.Second)
		timestamp := profile.LogTimestamp(seconds, startTime%int64(time.Second))
		stackTrace := strings.Join(stack.Stack, "\n")
		stackID := strconv.FormatUint(id, 16)
		duration := strconv.FormatInt(endTime-startTime, 10)
		for i, value := range vals {
			log := models.NewLogWithOrderedContents(13)
			log.Timestamp = timestamp
			if !r.pushMode && uint32(seconds) == 0 {
				// The pull input defaults timestamps with zero seconds to collection time.
				log.Timestamp = uint64(time.Now().UnixNano())
			}
			log.Contents.Add("name", stack.Name)
			log.Contents.Add("stack", stackTrace)
			log.Contents.Add("stackID", stackID)
			log.Contents.Add("language", meta.SpyName)
			log.Contents.Add("dataType", "CallStack")
			log.Contents.Add("durationNs", duration)
			log.Contents.Add("profileID", profileID)
			log.Contents.Add("labels", labelText)
			log.Contents.Add("units", units[i])
			log.Contents.Add("valueTypes", types[i])
			log.Contents.Add("aggTypes", aggs[i])
			log.Contents.Add("type", profile.DetectProfileType(types[i]).Kind)
			log.Contents.Add("val", strconv.FormatFloat(float64(value), 'f', 2, 64))
			*events = append(*events, log)
		}
	}
}

func buildKey(appLabels map[string]string, labels tree.Labels, table []string) *segment.Key {
	finalLabels := map[string]string{}
	for k, v := range appLabels {
		finalLabels[k] = v
	}
	for _, v := range labels {
		ks := table[v.Key]
		if ks == "" {
			continue
		}
		vs := table[v.Str]
		if vs == "" {
			continue
		}
		finalLabels[ks] = vs
	}
	return segment.NewKey(finalLabels)
}

func (r *RawProfile) extractProfileRaw() error {
	if r.formDataContentType == "" {
		r.profile = r.rawData
		return nil
	}
	boundary, err := form.ParseBoundary(r.formDataContentType)
	if err != nil {
		return err
	}
	f, err := multipart.NewReader(bytes.NewReader(r.rawData), boundary).ReadForm(32 << 20)
	if err != nil {
		return err
	}
	defer func() {
		_ = f.RemoveAll()
	}()

	if r.profile, err = form.ReadField(f, formFieldProfile); err != nil {
		return err
	}
	r.previousProfile, err = form.ReadField(f, formFieldPreviousProfile)
	if err != nil {
		return err
	}
	if c, err := form.ReadField(f, formFieldSampleTypeConfig); err != nil {
		return err
	} else if c != nil {
		var config map[string]*tree.SampleTypeConfig
		if err = json.Unmarshal(c, &config); err != nil {
			return err
		}
		r.sampleTypeConfig = config
	}
	return nil
}
