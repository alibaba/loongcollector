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

package inputsyslog

import (
	"errors"
	"time"

	"github.com/alibaba/ilogtail/pkg/util"

	"github.com/influxdata/go-syslog/rfc5424"
	"github.com/jeromer/syslogparser/rfc3164"
)

type parseResult struct {
	hostname string
	program  string
	priority int
	facility int
	severity int
	time     time.Time
	content  string

	// RFC5424
	procID         *string
	msgID          *string
	structuredData *map[string]map[string]string
}

func rawParseResult(data []byte) parseResult {
	return parseResult{
		priority: -1,
		facility: -1,
		severity: -1,
		time:     time.Now(),
		content:  string(data),
	}
}

type parser interface {
	// Parse returns the parsed fields or the raw message on an ignored parse error.
	Parse(data []byte) (parseResult, error)
}

type parserConfig struct {
	ignoreParseFailure bool
	addHostname        bool
}

type parserCreator func(config *parserConfig) parser

var syslogParsers = map[string]parserCreator{}

// defaultParser does not parse.
type defaultParser struct{}

func newDefaultParser(config *parserConfig) parser {
	return &defaultParser{}
}

func (p *defaultParser) Parse(data []byte) (parseResult, error) {
	return rawParseResult(data), nil
}

type rfc3164Parser struct {
	config *parserConfig
}

func newRfc3164Parser(config *parserConfig) parser {
	return &rfc3164Parser{config: config}
}

func (p *rfc3164Parser) Parse(data []byte) (parseResult, error) {
	pp := rfc3164.NewParser(data)
	pp.Location(time.Local)
	// if hostname field is not included in log, add os.Hostname for parser default hostname
	if p.config.addHostname {
		pp.Hostname(util.GetHostName())
	}
	err := pp.Parse()
	if err != nil {
		if !p.config.ignoreParseFailure {
			return parseResult{}, err
		}

		return rawParseResult(data), nil
	}

	result := pp.DumpParseResult()
	return parseResult{
		hostname: result.Hostname,
		program:  result.Tag,
		priority: result.Priority,
		facility: result.Facility,
		severity: result.Severity,
		time:     result.Timestamp,
		content:  result.Content,
	}, nil
}

type rfc5424Parser struct {
	config *parserConfig
	parser *rfc5424.Parser
}

func newRfc5424Parser(config *parserConfig) parser {
	return &rfc5424Parser{
		config: config,
		parser: rfc5424.NewParser(),
	}
}

func (p *rfc5424Parser) Parse(data []byte) (parseResult, error) {
	m, err := p.parser.Parse(data, nil)
	if err != nil {
		if !p.config.ignoreParseFailure {
			return parseResult{}, err
		}

		return rawParseResult(data), nil
	}

	pr := parseResult{
		priority: -1,
		facility: -1,
		severity: -1,
	}
	if priority := m.Priority(); priority != nil {
		pr.priority = int(*priority)
	}
	if severity := m.Severity(); severity != nil {
		pr.severity = int(*severity)
	}
	if facility := m.Facility(); facility != nil {
		pr.facility = int(*facility)
	}
	if hostname := m.Hostname(); hostname != nil {
		pr.hostname = *hostname
	}
	if appname := m.Appname(); appname != nil {
		pr.program = *appname
	}
	if message := m.Message(); message != nil {
		pr.content = *message
	}
	if timestamp := m.Timestamp(); timestamp != nil {
		pr.time = *timestamp
	} else {
		pr.time = time.Now()
	}
	pr.procID = m.ProcID()
	pr.msgID = m.MsgID()
	pr.structuredData = m.StructuredData()

	return pr, nil
}

// autoParser use all available parsers to try to parse data.
type autoParser struct {
	parsers []parser
	config  *parserConfig
}

func newAutoParser(config *parserConfig) parser {
	p := &autoParser{config: config}
	p.parsers = append(p.parsers, newRfc3164Parser(config))
	p.parsers = append(p.parsers, newRfc5424Parser(config))
	return p
}

func (p *autoParser) Parse(data []byte) (parseResult, error) {
	for _, realParser := range p.parsers {
		pr, err := realParser.Parse(data)
		if err == nil {
			return pr, nil
		}
	}
	if !p.config.ignoreParseFailure {
		return parseResult{}, errors.New("No available parser can parse: " + string(data))
	}
	return rawParseResult(data), nil
}

func init() {
	syslogParsers[""] = newDefaultParser
	syslogParsers["rfc3164"] = newRfc3164Parser
	syslogParsers["rfc5424"] = newRfc5424Parser
	syslogParsers["auto"] = newAutoParser
}
