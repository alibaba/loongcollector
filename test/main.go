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

package main

import (
	"context"
	"os"
	"time"

	"github.com/urfave/cli/v2"

	"github.com/alibaba/ilogtail/pkg/logger"
)

func main() {
	app := cli.NewApp()
	app.Name = "ilogtail-e2e"
	app.Version = "latest"
	app.Compiled = time.Now()
	app.Description = "ilogtail-e2e is for ilogtail integration testing"
	app.Commands = []*cli.Command{
		&cmdDocs,
		&cmdStart,
	}
	app.Action = cli.ShowAppHelp
	if err := app.Run(os.Args); err != nil {
		logger.Error(context.Background(), "START_E2E_ALARM", "err", err)
	}
}
