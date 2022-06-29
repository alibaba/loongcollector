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

package boot

import (
	"io/ioutil"
	"net/http"
	"os"
	"path/filepath"
	"testing"

	"github.com/alibaba/ilogtail/test/config"

	"github.com/stretchr/testify/assert"
	"gopkg.in/yaml.v3"
)

const testComposeContent = `
version: '3'
services:
  nginx:
    image: nginx:stable-alpine
    ports:
     - "9080:80"
`

func createComposeFile() {
	_ = ioutil.WriteFile(config.CaseHome+config.DockerComposeFileName, []byte(testComposeContent), 0600)
}

func clean() {
	_ = os.Remove(config.CaseHome + config.DockerComposeFileName)
}

func TestBootCompose(t *testing.T) {
	defer os.Remove("./test.log")
	os.Create("test.log")
	config.FlusherFile = "./test.log"
	config.ConfigFile = "./test.log"
	createComposeFile()
	defer clean()
	path, _ := filepath.Abs(".")
	config.CaseHome = path + "/"
	config.CoverageFile = path + "/cov.file"
	defer os.Remove("./cov.file")
	var cfg config.Case
	booter := NewComposeBooter(&cfg)
	assert.NoError(t, booter.Start())
	resp, err := http.Get("http://localhost:9080")
	assert.NoError(t, err)
	defer resp.Body.Close()
	assert.NoError(t, booter.Stop())
}

func TestComposeBooter_createLogtailpluginComposeFile(t *testing.T) {
	defer func() {
		assert.NoError(t, os.Remove(config.CaseHome+finalFileName))
	}()
	var cfg config.Case
	path, _ := filepath.Abs(".")
	config.CaseHome = path + "/"

	cfg.Ilogtail.ENV = map[string]string{
		"ENV1": "val1",
		"ENV2": "val2",
	}
	cfg.Ilogtail.DependsOn = map[string]interface{}{
		"kafka": map[string]string{
			"condition": "service_healthy",
		},
	}
	booter := NewComposeBooter(&cfg)
	assert.NoError(t, booter.createComposeFile())
	bytes, err := ioutil.ReadFile(config.CaseHome + finalFileName)
	assert.NoError(t, err)
	res := make(map[string]interface{})
	assert.NoError(t, yaml.Unmarshal(bytes, &res))
}
