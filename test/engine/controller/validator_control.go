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

package controller

import (
	"context"
	"encoding/json"
	"io/ioutil"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/boot"
	"github.com/alibaba/ilogtail/test/engine/validator"
)

type ValidatorController struct {
	chain         *CancelChain
	logValidators []validator.LogValidator
	sysValidators []validator.SystemValidator
	report        *Report
}

type Report struct {
	Pass               bool                                 `json:"pass"`
	RowLogCount        int                                  `json:"row_log_count"`
	ProcessedLogCount  int                                  `json:"processed_log_count"`
	FlushLogCount      int                                  `json:"flush_log_count"`
	FlushLogGroupCount int                                  `json:"flush_log_group"`
	AlarmLogs          map[string]map[string]map[string]int `json:"alarm_logs"`
	LogReports         map[string]*Reason                   `json:"log_reports"`
	TotalReports       map[string]*Reason                   `json:"total_reports"`
}

type Reason struct {
	Pass        bool           `json:"pass"`
	FailReasons map[string]int `json:"fail_reasons"`
}

func (c *ValidatorController) Init(parent *CancelChain, cfg *config.Case) error {
	logger.Info(context.Background(), "validator controller is initializing....")
	validator.InitCounter()
	c.chain = WithCancelChain(parent)
	for _, rule := range cfg.Verify.LogRules {
		logger.Debug(context.Background(), "stage", "add", "rule", rule.Name)
		v, err := validator.NewLogValidator(rule.Validator, rule.Spec)
		if err != nil {
			return err
		}
		c.logValidators = append(c.logValidators, v)
	}
	for _, rule := range cfg.Verify.SystemRules {
		logger.Debug(context.Background(), "stage", "add", "rule", rule.Name)
		v, err := validator.NewSystemValidator(rule.Validator, rule.Spec)
		if err != nil {
			return err
		}
		c.sysValidators = append(c.sysValidators, v)
	}
	c.report = &Report{
		Pass:         true,
		LogReports:   make(map[string]*Reason),
		TotalReports: make(map[string]*Reason),
	}
	return nil
}

func (c *ValidatorController) Start() error {
	logger.Info(context.Background(), "validator controller is starting....")
	for _, sysValidator := range c.sysValidators {
		if err := sysValidator.Start(); err != nil {
			return err
		}
	}
	go func() {
		<-c.chain.Done()
		logger.Info(context.Background(), "validator controller is closing....")
		boot.CopyCoreLogs()
		c.Clean()
		c.flushSummaryReport()
		c.chain.CancelChild()
		validator.CloseCounter()
	}()
	go func() {
		for group := range globalSubscriberChan {
			projectMatch, staticMatch := staticLogCheck(group.Logs[0])

			switch {
			case staticMatch:
				if !projectMatch {
					continue
				}
				validator.GetCounterChan() <- group
			case alarmLogCheck(group.Logs[0]):
				validator.GetAlarmLogChan() <- group
			default:
				for _, log := range group.Logs {
					logger.Debugf(context.Background(), "%s", log.String())
				}
				for _, v := range c.logValidators {
					c.addLogReport(v.Name(), v.Valid(group))
				}
				for _, v := range c.sysValidators {
					v.Valid(group)
				}
			}
		}
	}()
	return nil
}

func (c *ValidatorController) Clean() {
	logger.Info(context.Background(), "validator controller is cleaning....")
}

func (c *ValidatorController) CancelChain() *CancelChain {
	return c.chain
}

// flushSummaryReport write the whole validator report to the `report/{case_home}.json`.
func (c *ValidatorController) flushSummaryReport() {
	c.report.RowLogCount = validator.RawLogCounter
	c.report.ProcessedLogCount = validator.ProcessedLogCounter
	c.report.FlushLogCount = validator.FlushLogCounter
	c.report.FlushLogGroupCount = validator.FlushLogGroupCounter
	c.report.AlarmLogs = validator.AlarmLogs
	for _, sysValidator := range c.sysValidators {
		result := sysValidator.FetchResult()
		c.addSysReport(sysValidator.Name(), result)
	}
	bytes, _ := json.MarshalIndent(c.report, "", "\t")
	_ = ioutil.WriteFile(config.ReportFile, bytes, 0600)
}

// staticLogCheck checks the log contents to find the static logs of the e2e test case.
func staticLogCheck(log *protocol.Log) (projectMatch bool, typeMatch bool) {
	for _, content := range log.Contents {
		if content.Key == "raw_log" {
			typeMatch = true
		} else if content.Key == "project" && content.Value == E2EProjectName {
			projectMatch = true
		}
	}
	return
}

// alarmLogCheck checks the log contents to find the alarm log.
func alarmLogCheck(log *protocol.Log) bool {
	for _, content := range log.Contents {
		if content.Key == "alarm_count" {
			return true
		}
	}
	return false
}

// addLogReport add the log validator result to the final report.
func (c *ValidatorController) addLogReport(name string, failReports []*validator.Report) {
	_, ok := c.report.LogReports[name]
	if !ok {
		c.report.LogReports[name] = &Reason{
			Pass:        true,
			FailReasons: make(map[string]int),
		}
	}
	if len(failReports) > 0 {
		c.report.Pass = false
		reason := c.report.LogReports[name]
		reason.Pass = false
		for _, report := range failReports {
			reason.FailReasons[report.String()] = reason.FailReasons[report.String()] + 1
		}
	}
}

// addSysReport add the sys validator result to the final report.
func (c *ValidatorController) addSysReport(name string, failReports []*validator.Report) {
	if _, ok := c.report.TotalReports[name]; !ok {
		c.report.TotalReports[name] = &Reason{
			Pass:        true,
			FailReasons: make(map[string]int),
		}
	}
	if len(failReports) > 0 {
		c.report.Pass = false
		reason := c.report.TotalReports[name]
		reason.Pass = false
		for _, report := range failReports {
			reason.FailReasons[report.String()] = reason.FailReasons[report.String()] + 1
		}
	}
}
