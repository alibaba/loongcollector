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
package generator

import (
	"crypto/rand"
	"fmt"
	"math/big"
	"os"
	"strconv"
	"text/template"

	"github.com/pkg/errors"
)

var Levels = []string{"ERROR", "INFO", "DEBUG", "WARNING"}

type GenerateFileLogConfig struct {
	GeneratedLogDir string
	TotalLog        int
	Interval        int
	FileName        string
	Custom          map[string]string
}

func getGenerateFileLogConfigFromEnv(customKeys ...string) (*GenerateFileLogConfig, error) {
	gneratedLogDir := getEnvOrDefault("GENERATED_LOG_DIR", "/tmp/loongcollector")
	totalLog, err := strconv.Atoi(getEnvOrDefault("TOTAL_LOG", "100"))
	if err != nil {
		return nil, errors.Wrap(err, "parse TOTAL_LOG failed")
	}
	interval, err := strconv.Atoi(getEnvOrDefault("INTERVAL", "1"))
	if err != nil {
		return nil, errors.Wrap(err, "parse INTERVAL failed")
	}
	fileName := getEnvOrDefault("FILENAME", "default.log")
	custom := make(map[string]string)
	for _, key := range customKeys {
		custom[key] = getEnvOrDefault(key, "")
	}
	return &GenerateFileLogConfig{
		GeneratedLogDir: gneratedLogDir,
		TotalLog:        totalLog,
		Interval:        interval,
		FileName:        fileName,
		Custom:          custom,
	}, nil
}

func string2Template(strings []string) []*template.Template {
	templates := make([]*template.Template, len(strings))
	for i, str := range strings {
		templates[i], _ = template.New(fmt.Sprintf("template_%d", i)).Parse(str)
	}
	return templates
}

func getRandomLogLevel() string {
	randInt, _ := rand.Int(rand.Reader, big.NewInt(int64(len(Levels))))
	return Levels[randInt.Int64()]
}

func getRandomMark() string {
	marks := []string{"-", "F"}
	randInt, _ := rand.Int(rand.Reader, big.NewInt(int64(len(marks))))
	return marks[randInt.Int64()]
}

func getEnvOrDefault(env, fallback string) string {
	if value, ok := os.LookupEnv(env); ok {
		return value
	}
	return fallback
}
