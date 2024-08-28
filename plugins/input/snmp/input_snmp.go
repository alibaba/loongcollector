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
	"bufio"
	"bytes"
	"context"
	"errors"
	"fmt"
	"net"
	"os/exec"
	"runtime"
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	// third-party
	g "github.com/gosnmp/gosnmp"
)

const pluginType = "service_snmp"

// SNMP is a service input plugin to collect logs following SNMP protocol.
// It works with SNMP agents configured by users. It uses TCP or UDP
// to receive log from agents, and then translate them with MIB.

// Agent holds the configuration for a SNMP agent and will finally convert it into *g.GoSNMP agent.
type Agent struct {
	// Targets sets list of status target device ip
	Targets []string
	// Port sets local agent address
	Port string
	// Transport sets connection type TCP/UDP
	Transport string
	// Community sets authorize community for snmp v2
	Community string
	// Version sets SNMP version
	Version int
	// MaxRepetitions sets the GETBULK max-repetitions used by BulkWalk*
	MaxRepetitions uint32
	// Timeout is the timeout for one SNMP request/response.
	Timeout int
	// Retries sets the number of retries to attempt
	Retries int
	// ExponentialTimeout sets whether Double timeout in each retry
	ExponentialTimeout bool
	// Oids give the Object Identifier list for agent to collect
	Oids []string
	// Fields give the untranslated oids for agent to collect
	Fields []string
	// Tables give the table list for agent to collect
	Tables []string

	MaxTargetsLength int
	MaxOidsLength    int
	MaxFieldsLength  int
	MaxTablesLength  int
	MaxSearchLength  int

	// Authentication information for SNMP v3
	UserName                 string
	AuthoritativeEngineID    string
	AuthenticationProtocol   string
	AuthenticationPassphrase string
	PrivacyProtocol          string
	PrivacyPassphrase        string

	gs            []*g.GoSNMP
	fieldContents []Field
	context       pipeline.Context
}

// Field holds the configuration for a Field to look up.
type Field struct {
	Name       string
	Oid        string
	Conversion string
	Content    string
}

func (s *Agent) Description() string {
	return "get SNMP Agent input for logtail"
}

func (s *Agent) Init(context pipeline.Context) (int, error) {
	s.context = context

	if s.MaxTargetsLength < 1 {
		return 1, fmt.Errorf("invalid MaxTargetsLength, at least 1")
	}

	if s.MaxSearchLength < 1 {
		return 1, fmt.Errorf("invalid MaxSearchLength, at least 1")
	}

	if s.MaxOidsLength < 1 && s.MaxFieldsLength < 1 && s.MaxTablesLength < 1 {
		return 1, fmt.Errorf("unable to init input_snmp when MaxOidsLength and MaxFieldsLength and MaxTablesLength all smaller than 1")
	}

	if len(s.Targets) == 0 {
		return 1, fmt.Errorf("no targets, at least set one")
	}

	if s.Timeout <= 0 {
		return 1, fmt.Errorf("invalid Timeout, at larger than 0")
	}

	checkInputs := []string{"Targets", "Oids", "Fields", "Tables"}
	for _, checkPoints := range checkInputs {
		if s.checkInput(checkPoints) {
			return 1, fmt.Errorf("too many %v, exceed maximum length", checkPoints)
		}
	}

	// handle table inputs
	for _, table := range s.Tables {
		err := s.snmpTableCall(table)
		if err != nil {
			return 1, err
		}
	}

	// handle Fields and oids
	err := s.GetTranslated()
	if err != nil {
		return 1, err
	}

	if s.Transport != "tcp" && s.Transport != "udp" {
		return 1, fmt.Errorf("invalid transport %v, only support \"tcp\", \"udp\"", s.Transport)
	}

	if len(s.fieldContents) > s.MaxSearchLength {
		return 1, fmt.Errorf("search targets %v exceed maximum length %v", s.fieldContents, s.MaxSearchLength)
	} else if len(s.fieldContents) == 0 {
		return 1, fmt.Errorf("no search targets, make sure you've set oids or fields or tables to collect")
	}

	if s.PrivacyPassphrase == "" {
		s.PrivacyPassphrase = s.AuthenticationPassphrase
	}

	for _, target := range s.Targets {
		// use anonymous function to avoid resource leak
		ThisSNMPAgent, err := s.Wrapper(target)
		s.gs = append(s.gs, ThisSNMPAgent)
		if err != nil {
			return 1, fmt.Errorf("err %v accured while creating %v", err, target)
		}
	}

	return 0, nil
}

func (s *Agent) checkInput(checkpoint string) bool {
	flag := false
	switch checkpoint {
	case "Targets":
		flag = len(s.Targets) > s.MaxTargetsLength
	case "Oids":
		flag = len(s.Oids) > s.MaxOidsLength
	case "Fields":
		flag = len(s.Fields) > s.MaxFieldsLength
	case "Tables":
		flag = len(s.Tables) > s.MaxTablesLength
	default:
		flag = true
	}
	return flag
}

// Wrapper convert SNMPAgent configs into gosnmp.GoSNMP configs
func (s *Agent) Wrapper(envTarget string) (*g.GoSNMP, error) {
	envPort := s.Port
	port, _ := strconv.ParseUint(envPort, 10, 16)
	thisGoSNMP := &g.GoSNMP{
		Target:             envTarget,
		Port:               uint16(port),
		Community:          s.Community,
		Transport:          s.Transport,
		MaxRepetitions:     s.MaxRepetitions,
		Retries:            s.Retries,
		ExponentialTimeout: s.ExponentialTimeout,
		MaxOids:            s.MaxSearchLength,
		Timeout:            time.Duration(s.Timeout) * time.Second,
	}

	switch s.Version {
	case 3:
		var authenticationProtocol g.SnmpV3AuthProtocol
		switch s.AuthenticationProtocol {
		case "", "NoAuth":
			authenticationProtocol = g.NoAuth
		case "MD5":
			authenticationProtocol = g.MD5
		case "SHA":
			authenticationProtocol = g.SHA
		case "SHA224":
			authenticationProtocol = g.SHA224
		case "SHA256":
			authenticationProtocol = g.SHA256
		case "SHA384":
			authenticationProtocol = g.SHA384
		case "SHA512":
			authenticationProtocol = g.SHA512
		default:
			return nil, fmt.Errorf("unrecognized authenticationProtocol %v,"+
				" only support \"NoAuth\" \"MD5\" \"SHA\" \"SHA224\" \"SHA256\" \"SHA384\" \"SHA512\"",
				s.AuthenticationProtocol)
		}

		var privacyProtocol g.SnmpV3PrivProtocol
		switch s.PrivacyProtocol {
		case "", "NoPriv":
			privacyProtocol = g.NoPriv
		case "DES":
			privacyProtocol = g.DES
		case "AES":
			privacyProtocol = g.AES
		case "AES192":
			privacyProtocol = g.AES192
		case "AES256":
			privacyProtocol = g.AES256
		case "AES192C":
			privacyProtocol = g.AES192C
		case "AES256C":
			privacyProtocol = g.AES256C
		default:
			return nil, fmt.Errorf("unrecognized privacyProtocol %v,"+
				" only support \"NoPriv\" \"DES\" \"AES\" \"AES192\" \"AES256\" \"AES192C\" \"AES256C\"",
				s.PrivacyProtocol)
		}

		thisGoSNMP.Version = g.Version3
		thisGoSNMP.SecurityModel = g.UserSecurityModel
		thisGoSNMP.MsgFlags = g.AuthPriv
		thisGoSNMP.Transport = s.Transport
		thisGoSNMP.Timeout = time.Duration(s.Timeout) * time.Second
		thisGoSNMP.SecurityParameters = &g.UsmSecurityParameters{UserName: s.UserName,
			AuthenticationProtocol:   authenticationProtocol,
			AuthenticationPassphrase: s.AuthenticationPassphrase,
			PrivacyProtocol:          privacyProtocol,
			PrivacyPassphrase:        s.PrivacyPassphrase,
			AuthoritativeEngineID:    s.AuthoritativeEngineID,
		}
	case 2:
		thisGoSNMP.Version = g.Version2c

	case 1:
		thisGoSNMP.Version = g.Version1
	default:
		return nil, fmt.Errorf("unrecognized snmp version %v, only support 1,2,3", s.Version)
	}

	return thisGoSNMP, nil
}

func Asn1BER2String(source g.Asn1BER) (res string) {
	switch source {
	case 0x00:
		return "UnknownType"
	case 0x01:
		return "Boolean "
	case 0x02:
		return "Integer"
	case 0x03:
		return "BitString"
	case 0x04:
		return "OctetString"
	case 0x05:
		return "Null"
	case 0x06:
		return "ObjectIdentifier"
	case 0x07:
		return "ObjectDescription"
	case 0x40:
		return "IPAddress"
	case 0x41:
		return "Counter32"
	case 0x42:
		return "Gauge32"
	case 0x43:
		return "TimeTicks"
	case 0x44:
		return "Opaque"
	case 0x45:
		return "NsapAddress"
	case 0x46:
		return "Counter64"
	case 0x47:
		return "Uinteger32"
	case 0x78:
		return "OpaqueFloat"
	case 0x79:
		return "OpaqueDouble"
	case 0x80:
		return "NoSuchObject"
	case 0x81:
		return "NoSuchInstance"
	case 0x82:
		return "EndOfMibView"
	default:
		return ""
	}
}

// execCmd helps execute snmp
func execCmd(arg0 string, args ...string) ([]byte, error) {
	/* #nosec */
	cmd := exec.Command(arg0, args...)
	out, err := cmd.CombinedOutput()
	if err != nil && !strings.Contains(err.Error(), "no child process") {
		/* #nosec */
		if err, ok := err.(*exec.ExitError); ok {
			return nil, fmt.Errorf("%s: %w", bytes.TrimRight(err.Stderr, "\r\n"), err)
		}
		return nil, err
	}
	return out, nil
}

// snmpTranslateCall search local MIBs to translate oid information, needs command `snmptraslate`
func snmpTranslateCall(oid string) (oidNum string, oidText string, conversion string, mibName string, oidSuffix string, err error) {
	var out []byte
	if strings.ContainsAny(oid, ":abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") {
		out, err = execCmd("snmptranslate", "-Td", "-Ob", oid)
	} else {
		out, err = execCmd("snmptranslate", "-Td", "-Ob", "-m", "all", oid)
		if err, ok := err.(*exec.Error); ok && err.Err == exec.ErrNotFound {
			return oid, oid, "", "", "", nil
		}
	}
	if err != nil {
		return "", "", "", "", "", err
	}

	scanner := bufio.NewScanner(bytes.NewBuffer(out))
	ok := scanner.Scan()
	if !ok && scanner.Err() != nil {
		return "", "", "", "", "", fmt.Errorf("getting OID text: %w", scanner.Err())
	}

	oidText = scanner.Text()
	i := strings.Index(oidText, "::")
	if i == -1 {
		if bytes.Contains(out, []byte("[TRUNCATED]")) {
			return oid, oid, "", "", "", nil
		}
		oidText = oid
		oidSuffix = ""
	} else {
		mibName = oidText[:i]
		oidSuffix = oidText[i+2:]
	}

	for scanner.Scan() {
		line := scanner.Text()
		if strings.HasPrefix(line, "  -- TEXTUAL CONVENTION ") {
			tc := strings.TrimPrefix(line, "  -- TEXTUAL CONVENTION ")
			switch tc {
			case "MacAddress", "PhysAddress":
				conversion = "hwaddr"
			case "InetAddressIPv4", "InetAddressIPv6", "InetAddress", "IPSIpAddress":
				conversion = "ipaddr"
			}
		} else if strings.HasPrefix(line, "::= { ") {
			objs := strings.TrimPrefix(line, "::= { ")
			objs = strings.TrimSuffix(objs, " }")

			for _, obj := range strings.Split(objs, " ") {
				if len(obj) == 0 {
					continue
				}
				if i := strings.Index(obj, "("); i != -1 {
					obj = obj[i+1:]
					oidNum += "." + obj[:strings.Index(obj, ")")] //nolint:gocritic
				} else {
					oidNum += "." + obj
				}
			}
			break
		}
	}
	return oidNum, oidText, conversion, mibName, oidSuffix, nil
}

// snmpTableCall search local MIBs to translate table oid information, append table information to r.Field, needs command `snmptable`
func (s *Agent) snmpTableCall(oid string) error {
	// return oidNum, oidText, conversion, mibName , oidSuffix,nil
	_, oidText, _, mibName, oidSuffix, err := snmpTranslateCall(oid)
	mibPrefix := mibName + "::"
	if err != nil {
		return fmt.Errorf("translating error: %w, oidSuffix %v", err, oidSuffix)
	}
	out, err := execCmd("snmptable", "-Ch", "-Cl", "-c", "public", "127.0.0.1", oidText)
	if err != nil {
		return fmt.Errorf("getting table columns: %w", err)
	}
	scanner := bufio.NewScanner(bytes.NewBuffer(out))
	scanner.Scan()
	cols := scanner.Text()
	if len(cols) == 0 {
		return fmt.Errorf("could not find any columns in table")
	}
	for _, col := range strings.Split(cols, " ") {
		if len(col) == 0 {
			continue
		}
		s.Fields = append(s.Fields, mibPrefix+col)
	}
	return err
}

// appendField helps translate oids and append information to fieldContents
func (s *Agent) appendField(oid string) (err error) {
	oidNum, oidText, conversion, _, _, err := snmpTranslateCall(oid)
	if err != nil {
		return err
	}
	thisField := &Field{
		Name:       oidText,
		Oid:        oidNum,
		Conversion: conversion,
	}
	s.fieldContents = append(s.fieldContents, *thisField)
	return nil
}

// GetTranslated translate all oids and fields into fieldContents
func (s *Agent) GetTranslated() error {
	for _, oids := range s.Oids {
		err := s.appendField(oids)
		if err != nil {
			return err
		}
	}

	for _, fields := range s.Fields {
		err := s.appendField(fields)
		if err != nil {
			return err
		}
	}
	return nil
}

func (s *Agent) Start(collector pipeline.Collector) error {
	runtime.GOMAXPROCS(len(s.gs))
	for index, GsAgent := range s.gs {
		// use anonymous function to avoid resource leak
		thisGsAgent := GsAgent
		thisIndex := index
		go func() {
			err := func() error {
				err := thisGsAgent.Connect()
				if err != nil {
					logger.Errorf(context.Background(), "INPUT_SNMP_CONNECTION_ERROR", "%v", err)
					return err
				}

				var sent time.Time
				thisGsAgent.OnSent = func(x *g.GoSNMP) {
					sent = time.Now()
				}
				thisGsAgent.OnRecv = func(x *g.GoSNMP) {
					logger.Debugf(context.Background(), "Query latency in seconds:", time.Since(sent).Seconds())
				}

				var oids []string
				for _, fieldContents := range s.fieldContents {
					oids = append(oids, fieldContents.Oid)
				}

				result, err := thisGsAgent.Get(oids)

				switch {
				case errors.Is(err, g.ErrUnknownSecurityLevel):
					return fmt.Errorf("unknown security level")
				case errors.Is(err, g.ErrUnknownUsername):
					return fmt.Errorf("unknown username")
				case errors.Is(err, g.ErrWrongDigest):
					return fmt.Errorf("wrong digest")
				case errors.Is(err, g.ErrDecryption):
					return fmt.Errorf("decryption error")
				case err != nil:
					return err
				}

				for i, variable := range result.Variables {
					if s.fieldContents[i].Conversion == "hwaddr" {
						switch vt := variable.Value.(type) {
						case string:
							variable.Value = net.HardwareAddr(vt).String()
						case []byte:
							variable.Value = net.HardwareAddr(vt).String()
						default:
							return fmt.Errorf("invalid type (%T) for hwaddr conversion", variable.Value)
						}

					}

					if s.fieldContents[i].Conversion == "ipaddr" {
						var ipbs []byte

						switch vt := variable.Value.(type) {
						case string:
							ipbs = []byte(vt)
						case []byte:
							ipbs = vt
						default:
							return fmt.Errorf("invalid type (%T) for ipaddr conversion", variable.Value)
						}

						switch len(ipbs) {
						case 4, 16:
							variable.Value = net.IP(ipbs).String()
						default:
							return fmt.Errorf("invalid length (%d) for ipaddr conversion", len(ipbs))
						}
					}

					var res string
					switch variable.Type {
					case g.OctetString:
						res = string(variable.Value.([]byte))
					default:
						res = g.ToBigInt(variable.Value).String()
					}

					translatedType := Asn1BER2String(variable.Type)

					if translatedType == "Null" || translatedType == "" {
						logger.Warning(context.Background(), "result of %v is probably empty", s.fieldContents[i].Name)
						if s.Version == 1 {
							logger.Warning(context.Background(), "snmp v1 will not return any `GET` result while one of them is empty, please check `Oids`, `Fields`, `Tables` or change `Version` into `2`")
						}
					}

					collector.AddData(nil, map[string]string{
						"_targetindex_": strconv.Itoa(thisIndex),
						"_target_":      thisGsAgent.Target,
						"_field_":       s.fieldContents[i].Name,
						"_oid_":         variable.Name,
						"_conversion_":  s.fieldContents[i].Conversion,
						"_type_":        translatedType,
						"_content_":     res})
				}
				return nil
			}()
			if err != nil {
				logger.Errorf(context.Background(), "INPUT_SNMP_CONNECTION_ERROR", fmt.Sprintf("%v", err))
			}
		}()
	}
	return nil
}

func (s *Agent) Stop() error {
	for _, GsAgent := range s.gs {
		func() {
			defer GsAgent.Conn.Close()
		}()
	}
	return nil
}

func (s *Agent) Collect(_ pipeline.Collector) error {
	return nil
}

func init() {
	pipeline.ServiceInputs[pluginType] = func() pipeline.ServiceInput {
		return &Agent{
			Port:               "161",
			Transport:          "udp",
			Community:          "public",
			Version:            2,
			MaxRepetitions:     0,
			Timeout:            2,
			Retries:            2,
			ExponentialTimeout: false,
			MaxTargetsLength:   10,
			MaxOidsLength:      10,
			MaxFieldsLength:    10,
			MaxTablesLength:    10,
			MaxSearchLength:    30,
		}
	}
}
