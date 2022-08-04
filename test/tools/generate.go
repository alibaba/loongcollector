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

package tools

import (
	"os"
	"path/filepath"

	"github.com/alibaba/ilogtail/test/config"

	"gopkg.in/yaml.v3"
)

func GenerateManual(examplePath, docPath string) error {
	return genernateExample(examplePath)

}

func genernateExample(path string) error {
	abs, err := filepath.Abs(path)
	if err != nil {
		return err
	}
	_ = os.Remove(abs)
	bytes, err := yaml.Marshal(config.DemoCase)
	if err != nil {
		return err
	}
	return os.WriteFile(abs, bytes, 0600)
}
