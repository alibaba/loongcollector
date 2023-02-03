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

package mssql

import (
	"fmt"
	"strconv"
	"strings"

	mssql "github.com/denisenkom/go-mssqldb"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/input/rdb"
)

var msColumnResolverFuncMap map[string]rdb.ColumnResolverFunc = map[string]rdb.ColumnResolverFunc{
	"UNIQUEIDENTIFIER": func(val string) (string, error) {
		// for mssql uniqueidentifier
		uniVal := new(mssql.UniqueIdentifier)
		if err := uniVal.Scan([]byte(val)); err != nil {
			return "", err
		}
		return uniVal.String(), nil
	},
}

type Mssql struct {
	rdb.Rdb
}

func (m *Mssql) Init(context pipeline.Context) (int, error) {
	return m.Rdb.Init(context, func() error {
		if m.Rdb.Limit && m.Rdb.PageSize > 0 {
			m.Rdb.StateMent += fmt.Sprintf(" OFFSET ? ROW FETCH NEXT %d ROW ONLY", m.Rdb.PageSize)
		}
		return nil
	})
}

func (m *Mssql) Description() string {
	return "mssql input plugin for logtail"
}

// Address               string
// DataBase              string
// User                  string
// Password              string
// PasswordPath          string
// Location              string
// DialTimeOutMs         int
// ReadTimeOutMs         int
func (m *Mssql) dsnConfig() string {
	if len(m.Password) == 0 {
		if len(m.PasswordPath) != 0 {
			lines, err := util.ReadLines(m.PasswordPath)
			if err != nil && len(lines) > 0 {
				m.Password = strings.TrimSpace(lines[0])
			}
		}
	}

	// address can be one of the two below
	// 127.0.0.1:1433
	// 127.0.0.1
	addressIPPort := strings.Split(m.Address, ":")
	if len(addressIPPort) == 2 && m.Port == 0 {
		m.Port, _ = strconv.Atoi(addressIPPort[1])
	}
	if m.Port == 0 {
		m.Port = 1433
	}

	conn := fmt.Sprintf("server=%s;port=%d;database=%s;user id=%s;password=%s;dial timeout=%s;connection timeout=%s;encrypt=disable",
		m.Address,
		m.Port,
		m.DataBase,
		m.User,
		m.Password,
		fmt.Sprintf("%d", m.DialTimeOutMs/1000),
		fmt.Sprintf("%d", m.ReadTimeOutMs/1000),
	)
	return conn
}

// Start starts the ServiceInput's service, whatever that may be
func (m *Mssql) Start(collector pipeline.Collector) error {
	connStr := m.dsnConfig()
	return m.Rdb.Start(collector, connStr, func() error {
		return nil
	}, msColumnResolverFuncMap)
}

func (m *Mssql) Collect(collector pipeline.Collector) error {
	return m.Rdb.Collect(collector, msColumnResolverFuncMap)
}

// Stop stops the services and closes any necessary channels and connections
func (m *Mssql) Stop() error {
	return m.Rdb.Stop()
}

func init() {
	pipeline.ServiceInputs["service_mssql"] = func() pipeline.ServiceInput {
		return &Mssql{
			Rdb: rdb.Rdb{
				ConnectionRetryTime:   3,
				ConnectionRetryWaitMs: 5000,
				Driver:                "mssql",
				Address:               "127.0.0.1",
				User:                  "root",
				MaxSyncSize:           0,
				DialTimeOutMs:         5000,
				ReadTimeOutMs:         5000,
				Port:                  1433,
				Shutdown:              make(chan struct{}, 2),
			},
		}
	}
}
