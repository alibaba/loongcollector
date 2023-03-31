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

package fmtstr

import (
	"sort"
	"strconv"
	"strings"
	"time"
)

func newTimestampFormatMapping(goFormat, generalFormat string) *TimestampFormatMapping {
	return &TimestampFormatMapping{
		GoFormat:      goFormat,
		GeneralFormat: generalFormat,
	}
}

type TimestampFormatMapping struct {
	GoFormat      string
	GeneralFormat string
}

func newTimestampFormatMappings() []*TimestampFormatMapping {
	return []*TimestampFormatMapping{
		newTimestampFormatMapping("January", "MMMM"),
		newTimestampFormatMapping("Jan", "MMM"),
		newTimestampFormatMapping("1", "M"),
		newTimestampFormatMapping("01", "MM"),
		newTimestampFormatMapping("Monday", "EEEE"),
		newTimestampFormatMapping("Mon", "EEE"),
		newTimestampFormatMapping("2", "d"),
		newTimestampFormatMapping("02", "dd"),
		newTimestampFormatMapping("15", "HH"),
		newTimestampFormatMapping("3", "K"),
		newTimestampFormatMapping("03", "KK"),
		newTimestampFormatMapping("4", "m"),
		newTimestampFormatMapping("04", "mm"),
		newTimestampFormatMapping("5", "s"),
		newTimestampFormatMapping("05", "ss"),
		newTimestampFormatMapping("2006", "yyyy"),
		newTimestampFormatMapping("06", "yy"),
		newTimestampFormatMapping("PM", "aa"),
		newTimestampFormatMapping("MST", "Z"),
		newTimestampFormatMapping("Z0700", "'Z'XX"),
		newTimestampFormatMapping("Z070000", "'Z'XX"),
		newTimestampFormatMapping("Z07", "'Z'X"),
		newTimestampFormatMapping("Z07:00", "'Z'XXX"),
		newTimestampFormatMapping("Z07:00:00", "'Z'XXX"),
		newTimestampFormatMapping("-0700", "XX"),
		newTimestampFormatMapping("-07", "X"),
		newTimestampFormatMapping("-07:00", "XXX"),
		newTimestampFormatMapping("999999999", "SSS"),
	}
}

func FormatTimestamp(t *time.Time, format string) string {
	mappings := newTimestampFormatMappings()
	goFormat := GeneralToGoFormat(mappings, format)
	// format with week
	if strings.Contains(goFormat, "ww") {
		weekNumber := GetWeek(t)
		formattedTime := t.Format(goFormat)
		return strings.ReplaceAll(formattedTime, "ww", strconv.Itoa(weekNumber))
	}
	return t.Format(goFormat)
}

func GetWeek(t *time.Time) int {
	_, week := t.ISOWeek()
	return week
}

func GeneralToGoFormat(mappings []*TimestampFormatMapping, format string) string {
	// After sorting, avoid replacement errors when using replace later
	sort.Slice(mappings, func(i, j int) bool {
		return len(mappings[i].GeneralFormat) > len(mappings[j].GeneralFormat)
	})
	for _, m := range mappings {
		if m.GeneralFormat == "M" && (strings.Contains(format, "Mon") ||
			strings.Contains(format, "AM") || strings.Contains(format, "PM")) {
			continue
		}
		if m.GeneralFormat == "d" && strings.Contains(format, "Monday") {
			continue
		}
		format = strings.ReplaceAll(format, m.GeneralFormat, m.GoFormat)
	}
	return format
}
