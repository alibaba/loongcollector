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

package raw

import (
	"bufio"
	"bytes"
	"context"
	"encoding/json"
	"strconv"
	"strings"
	"time"

	"github.com/cespare/xxhash/v2"
	"github.com/pyroscope-io/pyroscope/pkg/structs/transporttrie"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/helper/profile"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type Profile struct {
	RawData []byte
	Format  profile.Format

	logs []*protocol.Log // v1 result
}

func NewRawProfile(data []byte, format profile.Format) *Profile {
	return &Profile{
		RawData: data,
		Format:  format,
	}
}

func (p *Profile) Parse(ctx context.Context, meta *profile.Meta, tags map[string]string) (logs []*protocol.Log, err error) {
	cb := p.extractProfileV1(meta, tags)
	if err := p.doParse(cb); err != nil {
		return nil, err
	}
	return p.logs, nil
}

func (p *Profile) doParse(cb func([]byte, int)) error {
	r := bytes.NewReader(p.RawData)
	switch p.Format {
	case profile.FormatTrie:
		err := transporttrie.IterateRaw(r, make([]byte, 0, 256), cb)
		if err != nil {
			return err
		}
	case profile.FormatGroups:
		scanner := bufio.NewScanner(r)
		for scanner.Scan() {
			if err := scanner.Err(); err != nil {
				return err
			}
			line := scanner.Bytes()
			index := bytes.LastIndexByte(line, byte(' '))
			if index == -1 {
				continue
			}
			stacktrace := line[:index]
			count := line[index+1:]
			i, err := strconv.Atoi(string(count))
			if err != nil {
				return err
			}
			cb(stacktrace, i)
		}
	}
	return nil
}

func (p *Profile) extractProfileV1(meta *profile.Meta, tags map[string]string) func([]byte, int) {
	profileID := profile.GetProfileID(meta)
	for k, v := range tags {
		meta.Tags[k] = v
	}
	labels, _ := json.Marshal(meta.Tags)
	return func(k []byte, v int) {
		name, stack := p.extractNameAndStacks(k, meta.SpyName)
		stackID := strconv.FormatUint(xxhash.Sum64(k), 16)
		var content []*protocol.Log_Content
		u := meta.Units
		if meta.Units == profile.SamplesUnits {
			u = profile.NanosecondsUnit
			v *= int(time.Second.Nanoseconds() / int64(meta.SampleRate))
		}
		content = append(content,
			&protocol.Log_Content{
				Key:   "name",
				Value: name,
			},
			&protocol.Log_Content{
				Key:   "stack",
				Value: strings.Join(stack, "\n"),
			},
			&protocol.Log_Content{
				Key:   "stackID",
				Value: stackID,
			},
			&protocol.Log_Content{
				Key:   "language",
				Value: meta.SpyName,
			},
			&protocol.Log_Content{
				Key:   "type",
				Value: profile.DetectProfileType(meta.Units.DetectValueType()).Kind,
			},
			&protocol.Log_Content{
				Key:   "units",
				Value: string(u),
			},
			&protocol.Log_Content{
				Key:   "valueTypes",
				Value: meta.Units.DetectValueType(),
			},
			&protocol.Log_Content{
				Key:   "aggTypes",
				Value: string(meta.AggregationType),
			},
			&protocol.Log_Content{
				Key:   "dataType",
				Value: "CallStack",
			},
			&protocol.Log_Content{
				Key:   "durationNs",
				Value: strconv.FormatInt(meta.EndTime.Sub(meta.StartTime).Nanoseconds(), 10),
			},
			&protocol.Log_Content{
				Key:   "profileID",
				Value: profileID,
			},
			&protocol.Log_Content{
				Key:   "labels",
				Value: string(labels),
			},
			&protocol.Log_Content{
				Key:   "val",
				Value: strconv.FormatFloat(float64(v), 'f', 2, 64),
			},
		)

		log := &protocol.Log{
			Contents: content,
		}
		protocol.SetLogTimeWithNano(log, uint32(meta.StartTime.Unix()), uint32(meta.StartTime.Nanosecond()))
		p.logs = append(p.logs, log)
	}

}

func (p *Profile) extractNameAndStacks(k []byte, spyName string) (name string, stack []string) {
	slice := strings.Split(string(k), ";")
	if len(slice) > 0 && slice[len(slice)-1] == "" {
		slice = slice[:len(slice)-1]
	}
	if len(slice) == 1 {
		return profile.FormatPositionAndName(slice[len(slice)-1], profile.FormatType(spyName)), []string{}
	}
	name = profile.FormatPositionAndName(slice[len(slice)-1], profile.FormatType(spyName))
	slice = profile.FormatPostionAndNames(slice[:len(slice)-1], profile.FormatType(spyName))
	helper.ReverseStringSlice(slice)
	return name, slice
}
