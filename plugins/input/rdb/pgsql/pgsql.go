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

package pgsql

import (
	"fmt"
	"net/url"
	"strconv"
	"strings"

	_ "github.com/jackc/pgx/v4/stdlib" //

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/input/rdb"
)

type Pgsql struct {
	rdb.Rdb
}

func (m *Pgsql) Init(context pipeline.Context) (int, error) {
	return m.Rdb.Init(context, func() error {
		if m.Rdb.Limit && m.Rdb.PageSize > 0 {
			if m.Rdb.CheckPoint {
				m.Rdb.StateMent += fmt.Sprintf(" LIMIT %d OFFSET $2", m.Rdb.PageSize)
			} else {
				m.Rdb.StateMent += fmt.Sprintf(" LIMIT %d OFFSET $1", m.Rdb.PageSize)
			}
		}
		return nil
	})
}

func (m *Pgsql) Description() string {
	return "pgsql input plugin for logtail"
}

// Address               string
// DataBase              string
// User                  string
// Password              string
// PasswordPath          string
// Location              string
// DialTimeOutMs         int
// ReadTimeOutMs         int
func (m *Pgsql) dsnConfig() string {
	if len(m.Password) == 0 {
		if len(m.PasswordPath) != 0 {
			lines, err := util.ReadLines(m.PasswordPath)
			if err != nil && len(lines) > 0 {
				m.Password = strings.TrimSpace(lines[0])
			}
		}
	}

	// address can be one of the two below
	// 127.0.0.1:5432
	// 127.0.0.1
	addressIPPort := strings.Split(m.Address, ":")
	if len(addressIPPort) == 2 && m.Port == 0 {
		m.Port, _ = strconv.Atoi(addressIPPort[1])
	}
	if m.Port == 0 {
		m.Port = 5432
	}

	conn := fmt.Sprintf("postgresql://%s:%s@%s:%d/%s?connect_timeout=%d",
		url.QueryEscape(m.User),
		url.QueryEscape(m.Password),
		url.QueryEscape(m.Address),
		m.Port,
		url.QueryEscape(m.DataBase),
		m.DialTimeOutMs/1000,
	)
	return conn
}

// Start starts the ServiceInput's service, whatever that may be
func (m *Pgsql) Start(collector pipeline.Collector) error {
	connStr := m.dsnConfig()
	return m.Rdb.Start(collector, connStr, func() error {
		return nil
	}, nil)
}

func (m *Pgsql) Collect(collector pipeline.Collector) error {
	return m.Rdb.Collect(collector, nil)
}

// Stop stops the services and closes any necessary channels and connections
func (m *Pgsql) Stop() error {
	return m.Rdb.Stop()
}

func init() {
	pipeline.ServiceInputs["service_pgsql"] = func() pipeline.ServiceInput {
		return &Pgsql{
			Rdb: rdb.Rdb{
				ConnectionRetryTime:   3,
				ConnectionRetryWaitMs: 5000,
				Driver:                "pgx",
				Address:               "127.0.0.1",
				User:                  "root",
				MaxSyncSize:           0,
				DialTimeOutMs:         5000,
				ReadTimeOutMs:         5000,
				Port:                  5432,
				Shutdown:              make(chan struct{}, 2),
			},
		}
	}
}
