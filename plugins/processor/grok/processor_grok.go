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

package grok

import (
	"bufio"
	"bytes"
	"errors"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/dlclark/regexp2"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type ProcessorGrok struct {
	CustomPatternDir    []string          // The paths of the folders where the custom GROK patterns are located. Processor_grok will read all the files in the folder. Doesn't support hot reload
	CustomPatterns      map[string]string // Custom GROK patterns, key is pattern's name and value is grok expression
	SourceKey           string            // Target field, default is "content"
	Match               []string          // Grok expressions to match logs
	TimeoutMilliSeconds int64             // Maximum attempt time in milliseconds to parse grok expressions, set to 0 to disable timeout, default is 0
	IgnoreParseFailure  bool              // Whether to keep the original field after parsing failure，default is true. Configured to false to discard the log when parsing fails.
	KeepSource          bool              // Whether to keep the original field after parsing success, default is true
	NoKeyError          bool              // Whether to report an error if there is no matching original field, default is false
	NoMatchError        bool              // Whether to report an error if all expressions in Match do not match, default is false
	TimeoutError        bool              // Whether to report an error if the match timeout, default is false

	context           pipeline.Context
	originalPatterns  map[string]string // The initial form of all imported grok patterns, e.g. {"SYSLOGPROG":`%{PROG:program}(?:\[%{POSINT:pid:int}\])?`},
	processedPatterns map[string]string // The parsed grok patterns in regular expression, e.g. {"SYSLOGPROG":{`(?P<program>[\x21-\x5a\x5c\x5e-\x7e]+)(?:\[(?P<pid>\b(?:[1-9][0-9]*)\b)\])?`,{"pid":"int"}}
	compiledPatterns  []*regexp2.Regexp // The compiled regexp object of the patterns in Match
	aliases           map[string]string // Correspondence between alias and original name, e.g. {"pid":"POSINT", "program":"PROG"}
}

const pluginType = "processor_grok"

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorGrok) Init(context pipeline.Context) error {
	p.context = context
	p.originalPatterns = map[string]string{}

	var err error

	p.addPatternsFromMap(defaultPatterns)

	if len(p.CustomPatternDir) > 0 {
		for _, path := range p.CustomPatternDir {
			err = p.addPatternsFromPath(path)
			if err != nil {
				logger.Error(p.context.GetRuntimeContext(), "PROCESSOR_INIT_ALARM", "init grok's custom pattern in dir error", err)
				return err
			}
		}
	}

	if len(p.CustomPatterns) > 0 {
		p.addPatternsFromMap(p.CustomPatterns)
	}

	err = p.buildPatterns()
	if err != nil {
		logger.Error(p.context.GetRuntimeContext(), "PROCESSOR_INIT_ALARM", "build grok's pattern error", err)
		return err
	}

	err = p.compileMatchs()
	if err != nil {
		logger.Error(p.context.GetRuntimeContext(), "PROCESSOR_INIT_ALARM", "compile grok's matchs error", err)
		return err
	}

	return nil
}

func (*ProcessorGrok) Description() string {
	return "grok processor for logtail"
}

func (p *ProcessorGrok) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	for _, log := range logArray {
		p.processLog(log)
	}
	return logArray
}

func (p *ProcessorGrok) processLog(log *protocol.Log) {
	findKey := false
	for i, cont := range log.Contents {
		if len(p.SourceKey) == 0 || p.SourceKey == cont.Key {
			findKey = true
			parseResult := p.processGrok(log, &cont.Value)

			// no match error
			if parseResult == matchFail && p.NoMatchError {
				logger.Warning(p.context.GetRuntimeContext(), "GROK_FIND_ALARM", "all match fail", p.SourceKey, cont.Value)
			}

			// tome out error
			if parseResult == matchTimeOut && p.TimeoutError {
				logger.Warning(p.context.GetRuntimeContext(), "GROK_FIND_ALARM", "match time out", p.SourceKey, cont.Value)
			}

			// keep source
			if (parseResult == matchSuccess && !p.KeepSource) || (parseResult != matchSuccess && !p.IgnoreParseFailure) {
				log.Contents = append(log.Contents[:i], log.Contents[i+1:]...)
			}
		}
	}

	// no key err
	if !findKey && p.NoKeyError {
		logger.Warning(p.context.GetRuntimeContext(), "GROK_FIND_ALARM", "anchor cannot find key", p.SourceKey)
	}
}

func (p *ProcessorGrok) processGrok(log *protocol.Log, val *string) MatchResult {
	findMatch := false
	for _, gr := range p.compiledPatterns {
		m, err := gr.FindStringMatch(*val)
		if err != nil {
			return matchTimeOut
		}

		names := []string{}
		captures := []string{}
		for m != nil {
			gps := m.Groups()
			for i := range gps {
				_, err = strconv.ParseInt(gps[i].Name, 10, 32)
				if err != nil && gps[i].Capture.String() != "" {
					name := p.nameToAlias(gps[i].Name)
					names = append(names, name)
					captures = append(captures, gps[i].Capture.String())
				}
			}
			m, err = gr.FindNextMatch(m)
			if err != nil {
				return matchTimeOut
			}
		}

		if len(captures) > 0 {
			for i := 0; i < len(captures); i++ {
				log.Contents = append(log.Contents, &protocol.Log_Content{Key: names[i], Value: captures[i]})
			}
			findMatch = true
			break
		}
	}
	if !findMatch {
		return matchFail
	}
	return matchSuccess
}

// Add patterns from path to processor_grok
func (p *ProcessorGrok) addPatternsFromPath(path string) error {
	if fi, err := os.Stat(path); err == nil {
		if fi.IsDir() {
			path += "/*"
		}
	} else {
		return errors.New("invalid path :" + path)
	}

	files, _ := filepath.Glob(path)

	var filePatterns = map[string]string{}
	for _, fileName := range files {
		file, err := os.Open(filepath.Clean(fileName))
		if err != nil {
			return errors.New("Cannot open file " + fileName + ", reason:" + err.Error())
		}

		scanner := bufio.NewScanner(bufio.NewReader(file))

		for scanner.Scan() {
			l := scanner.Text()
			if len(l) > 0 && l[0] != '"' {
				names := strings.SplitN(l, " ", 2)
				filePatterns[names[0]] = names[1]
			}
		}

		_ = file.Close()
	}
	p.addPatternsFromMap(filePatterns)
	return nil
}

// Add patterns from map to processor_grok
func (p *ProcessorGrok) addPatternsFromMap(m map[string]string) {
	for name, pattern := range m {
		p.originalPatterns[name] = pattern
	}
}

// trans original patterns to processed patterns
func (p *ProcessorGrok) buildPatterns() error {
	p.processedPatterns = map[string]string{}
	p.aliases = map[string]string{}
	patternGraph := graph{}

	for k, v := range p.originalPatterns {
		keys := []string{}
		for _, key := range normal.FindAllStringSubmatch(v, -1) {
			if !valid.MatchString(key[1]) {
				return errors.New("invalid pattern " + key[1])
			}
			names := strings.Split(key[1], ":")
			syntax := names[0]
			if p.processedPatterns[syntax] == "" {
				if _, ok := p.originalPatterns[syntax]; !ok {
					return errors.New("no pattern found for " + syntax)
				}
			}
			keys = append(keys, syntax)
		}
		patternGraph[k] = keys
	}

	order, cyclic := sortGraph(patternGraph)
	if cyclic != nil {
		cyc := ""
		for _, v := range cyclic {
			cyc += v + " "
		}
		return errors.New("cannot build patterns because cyclic exist" + cyc)
	}

	for _, key := range reverseList(order) {
		dnPattern, err := p.denormalizePattern(p.originalPatterns[key])
		if err != nil {
			return errors.New("cannot add pattern " + key + ": " + err.Error())
		}
		p.processedPatterns[key] = dnPattern
	}
	return nil
}

// trans a pattern to regex expression
func (p *ProcessorGrok) denormalizePattern(pattern string) (string, error) {
	for _, values := range normal.FindAllStringSubmatch(pattern, -1) {
		if !valid.MatchString(values[1]) {
			return "", errors.New("invalid pattern " + values[1])
		}
		names := strings.Split(values[1], ":")

		syntax, alias := names[0], names[0]
		if len(names) > 1 {
			alias = p.aliasizePatternName(names[1])
		}

		storedPattern, ok := p.processedPatterns[syntax]
		if !ok {
			return "", errors.New("no pattern found for " + syntax)
		}

		var buffer bytes.Buffer
		if len(names) > 1 {
			buffer.WriteString("(?P<")
			buffer.WriteString(alias)
			buffer.WriteString(">")
			buffer.WriteString(storedPattern)
			buffer.WriteString(")")
		} else {
			buffer.WriteString("(")
			buffer.WriteString(storedPattern)
			buffer.WriteString(")")
		}

		pattern = strings.ReplaceAll(pattern, values[0], buffer.String())
	}

	return pattern, nil
}

// create alias for pattern's name, because regexg capture group name cannot contain punctuation marks except '_'
func (p *ProcessorGrok) aliasizePatternName(name string) string {
	alias := symbolic.ReplaceAllString(name, "_")
	p.aliases[alias] = name
	return alias
}

// get alias
func (p *ProcessorGrok) nameToAlias(name string) string {
	alias, ok := p.aliases[name]
	if ok {
		return alias
	}
	return name
}

// compile all patterns in match
func (p *ProcessorGrok) compileMatchs() error {
	p.compiledPatterns = []*regexp2.Regexp{}
	for _, pattern := range p.Match {
		newPattern, err := p.denormalizePattern(pattern)
		if err != nil {
			return err
		}

		compiledRegex, err := regexp2.Compile(newPattern, regexp2.RE2)
		if err != nil {
			return err
		}

		gr := compiledRegex

		if p.TimeoutMilliSeconds != 0 {
			gr.MatchTimeout = time.Millisecond * time.Duration(p.TimeoutMilliSeconds)
		}

		p.compiledPatterns = append(p.compiledPatterns, gr)
	}
	return nil
}

func init() {
	pipeline.Processors[pluginType] = func() pipeline.Processor {
		return &ProcessorGrok{
			CustomPatternDir:    []string{},
			CustomPatterns:      map[string]string{},
			SourceKey:           "content",
			Match:               []string{},
			TimeoutMilliSeconds: 0,
			IgnoreParseFailure:  true,
			KeepSource:          true,
			NoKeyError:          false,
			NoMatchError:        true,
			TimeoutError:        true,
		}
	}
}

var (
	valid    = regexp.MustCompile(`^\w+([-.]\w+)*(:([-.\w]+)(:(string|float|int))?)?$`)
	normal   = regexp.MustCompile(`%{([\w-.]+(?::[\w-.]+(?::[\w-.]+)?)?)}`)
	symbolic = regexp.MustCompile(`\W`)
)

type MatchResult int64

const (
	matchSuccess MatchResult = iota
	matchFail
	matchTimeOut
)

type graph map[string][]string

func reverseList(s []string) (r []string) {
	for _, i := range s {
		i := i
		defer func() { r = append(r, i) }()
	}
	return
}

// topological sorting
func sortGraph(g graph) ([]string, []string) {
	order := make([]string, len(g))
	cyclic := make([]string, len(g))
	temp := map[string]bool{}
	perm := map[string]bool{}
	var cycleFound bool
	var cycleStart string

	var visit func(string)
	index := len(order)
	visit = func(node string) {
		switch {
		case temp[node]:
			cycleFound = true
			cycleStart = node
			return
		case perm[node]:
			return
		}
		temp[node] = true
		for _, m := range g[node] {
			visit(m)
			if cycleFound {
				if cycleStart > "" {
					cyclic = append(cyclic, node)
					if node == cycleStart {
						cycleStart = ""
					}
				}
				return
			}
		}
		delete(temp, node)
		perm[node] = true
		index--
		order[index] = node
	}
	for node := range g {
		if perm[node] {
			continue
		}
		visit(node)
		if cycleFound {
			return nil, cyclic
		}
	}
	return order, nil
}
