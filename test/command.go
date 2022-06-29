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
	"fmt"
	"os"
	"runtime"

	"github.com/urfave/cli/v2"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/controller"
	_ "github.com/alibaba/ilogtail/test/engine/subscriber"
	_ "github.com/alibaba/ilogtail/test/engine/validator"
	"github.com/alibaba/ilogtail/test/tools"
)

var (
	cmdStart = cli.Command{
		Name:  "start",
		Usage: "start ilogtail e2e",
		Flags: []cli.Flag{
			&cli.StringFlag{
				Name:    "config",
				Aliases: []string{"c"},
				Usage:   "Load configuration from `FILE`",
				Value:   "./example",
			},
			&cli.BoolFlag{
				Name:    "debug",
				Aliases: []string{"v"},
				Usage:   "Open debug flag",
				Value:   false,
			},
			&cli.BoolFlag{
				Name:    "profile",
				Aliases: []string{"p"},
				Usage:   "Open profile flag",
				Value:   false,
			},
		},
		Action: func(c *cli.Context) error {
			defer func() {
				if err := recover(); err != nil {
					trace := make([]byte, 2048)
					runtime.Stack(trace, true)
					logger.Error(context.Background(), "PLUGIN_RUNTIME_ALARM", "panicked", err, "stack", string(trace))
					logger.Flush()
					_ = os.Rename(util.GetCurrentBinaryPath()+"logtail_plugin.LOG", config.EngineLogFile)
					os.Exit(1)
				}
				_ = os.Rename(util.GetCurrentBinaryPath()+"logtail_plugin.LOG", config.EngineLogFile)
			}()
			configPath := c.String("config")
			debug := c.Bool("debug")
			profile := c.Bool("profile")
			loggerOptions := []logger.ConfigOption{
				logger.OptionAsyncLogger,
			}
			if debug {
				loggerOptions = append(loggerOptions, logger.OptionDebugLevel)
			} else {
				loggerOptions = append(loggerOptions, logger.OptionInfoLevel)
			}
			logger.InitTestLogger(loggerOptions...)
			defer logger.Flush()
			cfg, err := config.Load(configPath, profile)
			if err != nil {
				return err
			}
			if err := controller.Start(cfg); err != nil {
				logger.Error(context.Background(), "CONTROLLER_ALARM", "err", err)
				os.Exit(1)
			}
			return nil
		},
	}

	cmdDocs = cli.Command{
		Name:  "docs",
		Usage: "generate ilogtail e2e docs",
		Flags: []cli.Flag{
			&cli.StringFlag{
				Name:    "output",
				Aliases: []string{"o"},
				Usage:   "The document output root path",
				Value:   "docs",
			},
			&cli.StringFlag{
				Name:    "example",
				Aliases: []string{"e"},
				Usage:   "The menu file path",
				Value:   "./example/ilogtail-e2e.yaml",
			},
		},
		Action: func(c *cli.Context) error {
			examplePath := c.String("example")
			outputPath := c.String("output")
			err := tools.GenerateManual(examplePath, outputPath)
			fmt.Println("===============================")
			if err == nil {
				doc.Generate("./docs")
				fmt.Println("   GENERATE MANUAL SUCCESS")
			} else {
				fmt.Println("   GENERATE MANUAL FAIL")
			}
			fmt.Println("===============================")
			return err
		},
	}
)
