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

package helper

import (
	"regexp"
	"strconv"
)

var deploymentPodReg = regexp.MustCompile(`^([\w\-]+)\-([0-9a-z]{9,10})\-([0-9a-z]{5})$`)
var setPodReg = regexp.MustCompile(`^([\w\-]+)\-([0-9a-z]{5})$`)
var statefulSetPodReg = regexp.MustCompile(`^([\w\-]+)\-(\d+)$`)

func ExtractPodWorkload(name string) string {
	switch {
	case deploymentPodReg.MatchString(name):
		index := deploymentPodReg.FindStringSubmatchIndex(name)
		return name[index[2]:index[3]]
	case setPodReg.MatchString(name):
		index := setPodReg.FindStringSubmatchIndex(name)
		return name[index[2]:index[3]]
	case statefulSetPodReg.MatchString(name):
		index := statefulSetPodReg.FindStringSubmatchIndex(name)
		return name[index[2]:index[3]]
	}
	return name
}

func ExtractStatefulSetNum(pod string) int {
	if statefulSetPodReg.MatchString(pod) {
		index := statefulSetPodReg.FindStringSubmatchIndex(pod)
		num := pod[index[4]:index[5]]
		n, _ := strconv.Atoi(num)
		return n
	}
	return -1
}
