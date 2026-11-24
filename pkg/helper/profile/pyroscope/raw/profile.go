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
	"github.com/alibaba/ilogtail/pkg/models"
)

type Profile struct {
	RawData []byte
	Format  profile.Format
}

func NewRawProfile(data []byte, format profile.Format) *Profile {
	return &Profile{
		RawData: data,
		Format:  format,
	}
}

func (p *Profile) Parse(_ context.Context, meta *profile.Meta, tags map[string]string) ([]models.PipelineEvent, error) {
	var events []models.PipelineEvent
	if err := p.doParse(p.profileLogCallback(meta, tags, &events)); err != nil {
		return nil, err
	}
	return events, nil
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

func (p *Profile) profileLogCallback(meta *profile.Meta, tags map[string]string, events *[]models.PipelineEvent) func([]byte, int) {
	profileID := profile.GetProfileID(meta)
	for key, value := range tags {
		meta.Tags[key] = value
	}
	labels, _ := json.Marshal(meta.Tags)
	labelText := string(labels)
	valueType := meta.Units.DetectValueType()
	profileType := profile.DetectProfileType(valueType).Kind
	timestamp := profile.LogTimestamp(meta.StartTime.Unix(), int64(meta.StartTime.Nanosecond()))
	duration := strconv.FormatInt(meta.EndTime.Sub(meta.StartTime).Nanoseconds(), 10)
	return func(key []byte, value int) {
		name, stack := p.extractNameAndStacks(key, meta.SpyName)
		units := meta.Units
		if units == profile.SamplesUnits {
			units = profile.NanosecondsUnit
			value *= int(time.Second.Nanoseconds() / int64(meta.SampleRate))
		}
		log := models.NewLogWithOrderedContents(13)
		log.Timestamp = timestamp
		log.Contents.Add("name", name)
		log.Contents.Add("stack", strings.Join(stack, "\n"))
		log.Contents.Add("stackID", strconv.FormatUint(xxhash.Sum64(key), 16))
		log.Contents.Add("language", meta.SpyName)
		log.Contents.Add("type", profileType)
		log.Contents.Add("units", string(units))
		log.Contents.Add("valueTypes", valueType)
		log.Contents.Add("aggTypes", string(meta.AggregationType))
		log.Contents.Add("dataType", "CallStack")
		log.Contents.Add("durationNs", duration)
		log.Contents.Add("profileID", profileID)
		log.Contents.Add("labels", labelText)
		log.Contents.Add("val", strconv.FormatFloat(float64(value), 'f', 2, 64))
		*events = append(*events, log)
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
