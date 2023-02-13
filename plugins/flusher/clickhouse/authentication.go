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

package clickhouse

import (
	"fmt"

	"github.com/ClickHouse/clickhouse-go/v2"

	"github.com/alibaba/ilogtail/pkg/tlscommon"
)

type Authentication struct {
	// PlainText authentication
	PlainText *PlainTextConfig
	// TLS authentication
	TLS *tlscommon.TLSConfig
}

type PlainTextConfig struct {
	// The username for connecting to clickhouse.
	Username string
	// The password for connecting to clickhouse.
	Password string
	// The default database
	Database string
}

func (config *Authentication) ConfigureAuthentication(opts *clickhouse.Options) error {
	if config.PlainText != nil {
		if err := config.PlainText.ConfigurePlaintext(opts); err != nil {
			return err
		}
	}
	if config.TLS != nil {
		if err := configureTLS(config.TLS, opts); err != nil {
			return err
		}
	}
	return nil
}

func (plainTextConfig *PlainTextConfig) ConfigurePlaintext(opts *clickhouse.Options) error {
	// Validate Auth info
	if plainTextConfig.Username != "" && plainTextConfig.Password == "" {
		return fmt.Errorf("PlainTextConfig password must be set when username is configured")
	}
	if plainTextConfig.Database == "" {
		return fmt.Errorf("PlainTextConfig database must be set")
	}
	opts.Auth.Username = plainTextConfig.Username
	opts.Auth.Password = plainTextConfig.Password
	opts.Auth.Database = plainTextConfig.Database
	return nil
}

func configureTLS(config *tlscommon.TLSConfig, opts *clickhouse.Options) error {
	tlsConfig, err := config.LoadTLSConfig()
	if err != nil {
		return fmt.Errorf("error loading tls config: %w", err)
	}
	if tlsConfig != nil && tlsConfig.InsecureSkipVerify {
		opts.TLS = tlsConfig
	}
	return nil
}
