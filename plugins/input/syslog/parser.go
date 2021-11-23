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

func newParseResult() *parseResult {
	return &parseResult{
		hostname: "",
		program:  "",
		priority: -1,
		facility: -1,
		severity: -1,
		time:     time.Now(),
		content:  "",
	}
}

type parser interface {
	// Parse parses @data to parseResult according to the protocol of parser.
	// If it parses failed but failField in parserConfig is set, it will set the
	// content of @data to this field and returns no error.
	// It returns parseResult and any error encountered.
	Parse(data []byte) (*parseResult, error)
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

func (p *defaultParser) Parse(data []byte) (*parseResult, error) {
	pr := newParseResult()
	pr.content = string(data)
	return pr, nil
}

type rfc3164Parser struct {
	config *parserConfig
}

func newRfc3164Parser(config *parserConfig) parser {
	return &rfc3164Parser{config: config}
}

func (p *rfc3164Parser) Parse(data []byte) (*parseResult, error) {
	pp := rfc3164.NewParser(data)
	pp.Location(time.Local)
	// if hostname field is not included in log, add os.Hostname for parser default hostname
	if p.config.addHostname {
		pp.Hostname(util.GetHostName())
	}
	err := pp.Parse()
	if err != nil {
		if !p.config.ignoreParseFailure {
			return nil, err
		}

		pr := newParseResult()
		pr.content = string(data)
		return pr, nil
	}

	rfc3164Pr := pp.DumpParseResult()
	pr := newParseResult()
	pr.hostname = rfc3164Pr.Hostname
	pr.program = rfc3164Pr.Tag
	pr.priority = rfc3164Pr.Priority
	pr.facility = rfc3164Pr.Facility
	pr.severity = rfc3164Pr.Severity
	pr.time = rfc3164Pr.Timestamp
	pr.content = rfc3164Pr.Content
	return pr, nil
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

func (p *rfc5424Parser) Parse(data []byte) (*parseResult, error) {
	m, err := p.parser.Parse(data, nil)
	if err != nil {
		if !p.config.ignoreParseFailure {
			return nil, err
		}

		pr := newParseResult()
		pr.content = string(data)
		return pr, nil
	}

	pr := newParseResult()
	if m.Priority() != nil {
		pr.priority = int(*m.Priority())
	}
	if m.Severity() != nil {
		pr.severity = int(*m.Severity())
	}
	if m.Facility() != nil {
		pr.facility = int(*m.Facility())
	}
	if m.Hostname() != nil {
		pr.hostname = *m.Hostname()
	}
	if m.Appname() != nil {
		pr.program = *m.Appname()
	}
	if m.Message() != nil {
		pr.content = *m.Message()
	}
	if m.Timestamp() != nil {
		pr.time = *m.Timestamp()
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

func (p *autoParser) Parse(data []byte) (pr *parseResult, err error) {
	for _, realParser := range p.parsers {
		pr, err = realParser.Parse(data)
		if err == nil {
			return pr, nil
		}
	}
	if !p.config.ignoreParseFailure {
		return nil, errors.New("No available parser can parse: " + string(data))
	}
	pr = newParseResult()
	pr.content = string(data)
	return pr, nil
}

func init() {
	syslogParsers[""] = newDefaultParser
	syslogParsers["rfc3164"] = newRfc3164Parser
	syslogParsers["rfc5424"] = newRfc5424Parser
	syslogParsers["auto"] = newAutoParser
}
