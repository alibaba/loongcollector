// Copyright 2022 iLogtail Authors
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

package kafkav2

import (
	"fmt"

	"github.com/IBM/sarama"
)

// Version is a kafka version
type Version string

// Validate that a kafka version is among the possible options
func (v *Version) Validate() error {
	if _, ok := v.Get(); ok {
		return nil
	}
	return fmt.Errorf("unknown/unsupported kafka version '%v'", v)
}

// Get a sarama kafka version
func (v *Version) Get() (sarama.KafkaVersion, bool) {
	s := string(*v)
	version, err := sarama.ParseKafkaVersion(s)
	if err != nil {
		return sarama.KafkaVersion{}, false
	}
	for _, supp := range sarama.SupportedVersions {
		if version == supp {
			return version, true
		}
	}
	return sarama.KafkaVersion{}, false
}
