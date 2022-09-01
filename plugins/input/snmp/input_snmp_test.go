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

package snmp

import (
	// stdlib
	"fmt"
	"os"
	"os/exec"
	"strings"
	"testing"
	"time"

	// other packages of this project
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pluginmanager"

	// third party
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

type ContextTest struct {
	pluginmanager.ContextImp

	Logs []string
}

type mockLog struct {
	tags   map[string]string
	fields map[string]string
}

type mockCollector struct {
	logs    []mockLog
	rawLogs []protocol.Log
}

func ExecCmdWithOption(cmd string, panicErr bool) (string, error) {
	c := exec.Command("/bin/bash", "-c", cmd)
	c.Env = os.Environ()
	output, err := c.CombinedOutput()
	if err != nil {
		if panicErr {
			fmt.Println("Failed to execute command:", cmd, err)
			panic(err)
		}
	}
	result := strings.Trim(string(output), "\n")
	return result, err
}

func (c *mockCollector) AddData(
	tags map[string]string, fields map[string]string, t ...time.Time) {
	c.AddDataWithContext(tags, fields, nil, t...)
}

func (c *mockCollector) AddDataArray(
	tags map[string]string, columns []string, values []string, t ...time.Time) {
	c.AddDataArrayWithContext(tags, columns, values, nil, t...)
}

func (c *mockCollector) AddRawLog(log *protocol.Log) {
	c.AddRawLogWithContext(log, nil)
}

func (c *mockCollector) AddDataWithContext(
	tags map[string]string, fields map[string]string, ctx map[string]interface{}, t ...time.Time) {
	c.logs = append(c.logs, mockLog{tags, fields})
}

func (c *mockCollector) AddDataArrayWithContext(
	tags map[string]string, columns []string, values []string, ctx map[string]interface{}, t ...time.Time) {
}

func (c *mockCollector) AddRawLogWithContext(log *protocol.Log, ctx map[string]interface{}) {
	c.rawLogs = append(c.rawLogs, *log)
}

func (p *ContextTest) LogWarnf(alarmType string, format string, params ...interface{}) {
	p.Logs = append(p.Logs, alarmType+":"+fmt.Sprintf(format, params...))
}

func (p *ContextTest) LogErrorf(alarmType string, format string, params ...interface{}) {
	p.Logs = append(p.Logs, alarmType+":"+fmt.Sprintf(format, params...))
}

func (p *ContextTest) LogWarn(alarmType string, kvPairs ...interface{}) {
	fmt.Println(alarmType, kvPairs)
}
func defaultInput() (*pluginmanager.ContextImp, *Agent) {
	// to use this function, create "public" community first
	ctx := &pluginmanager.ContextImp{}
	input := &Agent{
		Targets:            []string{"127.0.0.1"},
		Port:               "161",
		Community:          "public",
		MaxRepetitions:     0,
		Timeout:            1,
		Retries:            1,
		ExponentialTimeout: false,
		MaxTargetsLength:   10,
		MaxOidsLength:      10,
		MaxFieldsLength:    10,
		MaxTablesLength:    10,
		MaxSearchLength:    30,
	}
	return ctx, input
}

func newUDPInputV1() (*pluginmanager.ContextImp, *Agent) {
	ctx, input := defaultInput()
	input.Version = 1
	input.Transport = "udp"
	return ctx, input
}

func newTCPInputV1() (*pluginmanager.ContextImp, *Agent) {
	ctx, input := defaultInput()
	input.Version = 1
	input.Transport = "tcp"
	return ctx, input
}

func newInputV2() (*pluginmanager.ContextImp, *Agent) {
	ctx, input := defaultInput()
	input.Version = 2
	input.Transport = "udp"
	return ctx, input
}

func newInputV3() (*pluginmanager.ContextImp, *Agent) {
	// to use this function, run `net-snmp-create-v3-user -ro -A SecUREDpass -a SHA -X StRongPASS -x AES snmpreadonly` first
	ctx, input := defaultInput()
	input.Version = 3
	input.Transport = "udp"
	input.UserName = "snmpreadonly"
	input.AuthenticationProtocol = "SHA"
	input.AuthenticationPassphrase = "SecUREDpass"
	input.PrivacyProtocol = "AES"
	input.PrivacyPassphrase = "StRongPASS"
	return ctx, input
}

func InitGoSNMP(ctx *pluginmanager.ContextImp, input *Agent) error {
	input.Oids = append(input.Oids, "1.3.6.1.2.1.1.4.0")
	_, err := input.Init(ctx)
	if err != nil {
		return err
	}
	return nil
}

func TestStartAndStop(t *testing.T) {
	// need open both udp and tcp transport in  snmpd.conf
	res, _ := ExecCmdWithOption("ps aux | awk '{print $11}' | grep ^snmp", false)
	n := strings.Count(res, "\n")
	if n < 1 {
		// snmpd is not running, skip the test
		return
	}

	ctx := &pluginmanager.ContextImp{}
	ctx.InitContext("test_project", "test_logstore", "test_configname")

	ctx, input := newUDPInputV1()
	_ = InitGoSNMP(ctx, input)

	collector := &mockCollector{}

	var err error
	go func() {
		err = input.Start(collector)
		require.NoError(t, err)
	}()

	time.Sleep(time.Duration(1) * time.Second)
	assert.Equal(t, len(collector.logs), 1)

	t1 := time.Now()
	err = input.Stop()
	require.NoError(t, err)
	dur := time.Since(t1)
	// dur := time.Now().Sub(t1) // for lint requirements
	require.True(t, dur/time.Microsecond < 2000, "dur: %v", dur)

	ctx, input = newTCPInputV1()
	_ = InitGoSNMP(ctx, input)

	collector = &mockCollector{}
	go func() {
		err = input.Start(collector)
		require.NoError(t, err)
	}()

	// time.Sleep(time.Duration(1) * time.Second)
	// assert.Equal(t, len(collector.logs), 1)

	t2 := time.Now()
	err = input.Stop()
	require.NoError(t, err)
	dur = time.Since(t2)
	// dur := time.Now().Sub(t1) // for lint requirements
	require.True(t, dur/time.Microsecond < 2000, "dur: %v", dur)

}

func TestGET(t *testing.T) {
	// "Content" equals to result of command `snmpget -v2c -c public 127.0.0.1 <Oid>`, may different on different machines
	res, _ := ExecCmdWithOption("ps aux | awk '{print $11}' | grep ^snmp", false)
	n := strings.Count(res, "\n")
	if n < 1 {
		// snmpd is not running, skip the test
		return
	}

	ctx, input := newInputV3()
	err := InitGoSNMP(ctx, input)
	require.NoError(t, err)
	err = input.gs[0].Connect()
	require.NoError(t, err)
	collector := &mockCollector{}
	go func() {
		err = input.Start(collector)
		require.NoError(t, err)
	}()
	time.Sleep(time.Duration(1) * time.Second)
	assert.Equal(t, len(collector.logs), 1)
	assert.Equal(
		t,
		collector.logs[0],
		mockLog{nil, map[string]string{
			"_target_":      "127.0.0.1",
			"_field_":       "SNMPv2-MIB::sysContact.0",
			"_oid_":         ".1.3.6.1.2.1.1.4.0",
			"_conversion_":  "",
			"_type_":        "OctetString",
			"_content_":     "Me <me@example.org>",
			"_targetindex_": "0"}},
	)
	ctx, input = newInputV2()
	err = InitGoSNMP(ctx, input)
	require.NoError(t, err)
	err = input.gs[0].Connect()
	require.NoError(t, err)
	collector = &mockCollector{}
	go func() {
		err = input.Start(collector)
		require.NoError(t, err)
	}()
	time.Sleep(time.Duration(1) * time.Second)
	assert.Equal(t, len(collector.logs), 1)
	assert.Equal(
		t,
		collector.logs[0],
		mockLog{nil, map[string]string{
			"_target_":      "127.0.0.1",
			"_field_":       "SNMPv2-MIB::sysContact.0",
			"_oid_":         ".1.3.6.1.2.1.1.4.0",
			"_conversion_":  "",
			"_type_":        "OctetString",
			"_content_":     "Me <me@example.org>",
			"_targetindex_": "0"}},
	)
	ctx, input = newTCPInputV1()
	err = InitGoSNMP(ctx, input)
	require.NoError(t, err)
	err = input.gs[0].Connect()
	require.NoError(t, err)
	collector = &mockCollector{}
	go func() {
		err = input.Start(collector)
		require.NoError(t, err)
	}()
	time.Sleep(time.Duration(1) * time.Second)
	assert.Equal(t, len(collector.logs), 1)
	assert.Equal(
		t,
		collector.logs[0],
		mockLog{nil, map[string]string{
			"_target_":      "127.0.0.1",
			"_field_":       "SNMPv2-MIB::sysContact.0",
			"_oid_":         ".1.3.6.1.2.1.1.4.0",
			"_conversion_":  "",
			"_type_":        "OctetString",
			"_content_":     "Me <me@example.org>",
			"_targetindex_": "0"}},
	)
	ctx, input = newUDPInputV1()
	err = InitGoSNMP(ctx, input)
	require.NoError(t, err)
	err = input.gs[0].Connect()
	require.NoError(t, err)
	collector = &mockCollector{}
	go func() {
		err = input.Start(collector)
		require.NoError(t, err)
	}()
	time.Sleep(time.Duration(1) * time.Second)
	assert.Equal(t, len(collector.logs), 1)
	assert.Equal(
		t,
		collector.logs[0],
		mockLog{nil, map[string]string{
			"_target_":      "127.0.0.1",
			"_field_":       "SNMPv2-MIB::sysContact.0",
			"_oid_":         ".1.3.6.1.2.1.1.4.0",
			"_conversion_":  "",
			"_type_":        "OctetString",
			"_content_":     "Me <me@example.org>",
			"_targetindex_": "0"}},
	)
}

func TestAuth(t *testing.T) {
	res, _ := ExecCmdWithOption("ps aux | awk '{print $11}' | grep ^snmp", false)
	n := strings.Count(res, "\n")
	if n < 1 {
		// snmpd is not running, skip the test
		return
	}

	ctx, input := newInputV3()
	input.UserName = "11111"
	err := InitGoSNMP(ctx, input)
	require.NoError(t, err)
	err = input.gs[0].Connect()
	require.NoError(t, err)
	_, err = input.gs[0].Get(input.Oids)
	assert.Equal(t, err, fmt.Errorf("unknown username"))
	ctx, input = newInputV3()
	input.AuthenticationPassphrase = "test1"
	err = InitGoSNMP(ctx, input)
	require.NoError(t, err)
	err = input.gs[0].Connect()
	require.NoError(t, err)
	_, err = input.gs[0].Get(input.Oids)
	assert.Equal(t, err, fmt.Errorf("wrong digest"))
	ctx, input = newInputV3()
	input.AuthenticationPassphrase = "test2"
	err = InitGoSNMP(ctx, input)
	require.NoError(t, err)
	err = input.gs[0].Connect()
	require.NoError(t, err)
	_, err = input.gs[0].Get(input.Oids)
	assert.Equal(t, err, fmt.Errorf("wrong digest"))
	ctx, input = newInputV3()
	input.PrivacyPassphrase = "test3"
	err = InitGoSNMP(ctx, input)
	require.NoError(t, err)
	err = input.gs[0].Connect()
	require.NoError(t, err)
	_, err = input.gs[0].Get(input.Oids)
	assert.Equal(t, err, fmt.Errorf("request timeout (after %v retries)", input.Retries))
	ctx, input = newInputV2()
	input.Community = "test4"
	err = InitGoSNMP(ctx, input)
	require.NoError(t, err)
	err = input.gs[0].Connect()
	require.NoError(t, err)
	_, err = input.gs[0].Get(input.Oids)
	assert.Equal(t, err, fmt.Errorf("request timeout (after %v retries)", input.Retries))
}

func TestOidsParser(t *testing.T) {
	// `Field` equals to results of command `snmptranslate -Td -Ob -m all <Oids>`, may different on different machines
	res, _ := ExecCmdWithOption("ps aux | awk '{print $11}' | grep ^snmp", false)
	n := strings.Count(res, "\n")
	if n < 1 {
		// snmpd is not running, skip the test
		return
	}

	ctx, input := newInputV2()
	input.Oids = append(input.Oids, "1.3.6.1.2.1.1.3.0")
	input.Oids = append(input.Oids, "1.3.6.1.2.1.1.4.0")
	_, err := input.Init(ctx)
	require.NoError(t, err)
	assert.Equal(t, input.fieldContents[0],
		Field{
			Name:       "DISMAN-EXPRESSION-MIB::sysUpTimeInstance",
			Oid:        ".1.3.6.1.2.1.1.3.0",
			Conversion: ""})
	require.NoError(t, err)
	assert.Equal(t, input.fieldContents[1],
		Field{
			Name:       "SNMPv2-MIB::sysContact.0",
			Oid:        ".1.3.6.1.2.1.1.4.0",
			Conversion: ""})
}

func TestFieldsParser(t *testing.T) {
	res, _ := ExecCmdWithOption("ps aux | awk '{print $11}' | grep ^snmp", false)
	n := strings.Count(res, "\n")
	if n < 1 {
		// snmpd is not running, skip the test
		return
	}

	ctx, input := newInputV2()
	input.Fields = append(input.Fields, "SNMPv2-MIB::system.sysUpTime.0")
	input.Fields = append(input.Fields, "SNMPv2-MIB::sysContact.0")
	_, err := input.Init(ctx)
	require.NoError(t, err)
	assert.Equal(t, len(input.fieldContents), 2)
	assert.Equal(t, input.fieldContents[0],
		Field{
			Name:       "DISMAN-EVENT-MIB::sysUpTimeInstance",
			Oid:        ".1.3.6.1.2.1.1.3.0",
			Conversion: ""})
	require.NoError(t, err)
	assert.Equal(t, input.fieldContents[1],
		Field{
			Name:       "SNMPv2-MIB::sysContact.0",
			Oid:        ".1.3.6.1.2.1.1.4.0",
			Conversion: ""})
}

func TestTablesParser(t *testing.T) {
	// `fieldContents` equals to results of command `snmptable -v 2c -c public -Ch 127.0.0.1 <Tables>`, may different on different machines
	res, _ := ExecCmdWithOption("ps aux | awk '{print $11}' | grep ^snmp", false)
	n := strings.Count(res, "\n")
	if n < 1 {
		// snmpd is not running, skip the test
		return
	}

	ctx, input := newInputV2()
	input.Tables = append(input.Tables, "SNMPv2-MIB::sysORTable")
	_, err := input.Init(ctx)
	require.NoError(t, err)
	assert.Equal(t, len(input.fieldContents), 3)
	assert.Equal(t, input.fieldContents[0],
		Field{
			Name:       "SNMPv2-MIB::sysORID",
			Oid:        ".1.3.6.1.2.1.1.9.1.2",
			Conversion: ""})
	assert.Equal(t, input.fieldContents[1],
		Field{
			Name:       "SNMPv2-MIB::sysORDescr",
			Oid:        ".1.3.6.1.2.1.1.9.1.3",
			Conversion: ""})
	assert.Equal(t, input.fieldContents[2],
		Field{
			Name:       "SNMPv2-MIB::sysORUpTime",
			Oid:        ".1.3.6.1.2.1.1.9.1.4",
			Conversion: ""})
}

func TestEmptyInputs(t *testing.T) {
	ctx, input := newInputV2()
	_, err := input.Init(ctx)
	assert.Equal(t, err, fmt.Errorf("no search targets, make sure you've set oids or fields or tables to collect"))
}
