// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package ratelimit

import (
	"fmt"
	"strconv"
	"strings"
)

const (
	unitPerSecond string = "s"
	unitPerMinute string = "m"
	unitPerHour   string = "h"
)

type algorithm interface {
	IsAllowed(string) bool
}

type rate struct {
	value          float64
	unit           string
	valuePerSecond float64
}

// Unpack creates a rate from the given string
func (r *rate) Unpack(str string) error {
	parts := strings.Split(str, "/")
	if len(parts) != 2 {
		return fmt.Errorf(`rate in invalid format: %v. Must be specified as "number/unit"`, str)
	}

	valueStr := strings.TrimSpace(parts[0])
	unitStr := strings.TrimSpace(parts[1])

	v, err := strconv.ParseFloat(valueStr, 32)
	if err != nil {
		return fmt.Errorf(`rate's value component is not numeric: %v`, valueStr)
	}

	if !IsValidUnit(unitStr) {
		return fmt.Errorf(`rate's unit component is not valid: %v`, unitStr)
	}

	r.value = v
	r.unit = unitStr
	r.valuePerSecond = r.getValuePerSecond()

	return nil
}

func (r *rate) getValuePerSecond() float64 {
	switch r.unit {
	case unitPerSecond:
		return r.value
	case unitPerMinute:
		return r.value / 60
	case unitPerHour:
		return r.value / (60 * 60)
	}

	return 0
}

func IsValidUnit(candidate string) bool {
	for _, a := range []string{unitPerSecond, unitPerMinute, unitPerHour} {
		if candidate == a {
			return true
		}
	}

	return false
}
