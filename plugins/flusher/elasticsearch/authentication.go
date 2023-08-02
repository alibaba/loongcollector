// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package elasticsearch

import (
	"fmt"
	"io/ioutil"
	"net/http"
	"reflect"
	"time"

	"github.com/elastic/go-elasticsearch/v8"

	"github.com/alibaba/ilogtail/pkg/tlscommon"
)

type Authentication struct {
	// PlainTextConfig
	PlainText *PlainTextConfig
	// TLS authentication
	TLS *tlscommon.TLSConfig
}

type PlainTextConfig struct {
	// The username for connecting to clickhouse.
	Username string
	// The password for connecting to clickhouse.
	Password string
}

func (config *Authentication) ConfigureAuthenticationAndHTTP(httpcfg *HTTPConfig, opts *elasticsearch.Config) error {
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
	if httpcfg != nil {
		if err := configureHTTP(httpcfg, opts); err != nil {
			return err
		}
	}
	return nil
}

func (plainTextConfig *PlainTextConfig) ConfigurePlaintext(opts *elasticsearch.Config) error {
	// Validate Auth info
	if plainTextConfig.Username == "" || plainTextConfig.Password == "" {
		return fmt.Errorf("PlainTextConfig username or password cannot be null")
	}
	opts.Username = plainTextConfig.Username
	opts.Password = plainTextConfig.Password
	return nil
}

func configureTLS(config *tlscommon.TLSConfig, opts *elasticsearch.Config) error {
	tlsConfig, err := config.LoadTLSConfig()
	if err != nil {
		return fmt.Errorf("error loading tls config: %w", err)
	}
	transport := &http.Transport{}
	if tlsConfig != nil && tlsConfig.InsecureSkipVerify {
		transport.TLSClientConfig = tlsConfig
	}
	if config.CAFile != "" {
		opts.CACert, err = ioutil.ReadFile(config.CAFile)
		if err != nil {
			return err
		}
	}
	mergeTransports(opts, transport)
	return nil
}

func configureHTTP(httpcfg *HTTPConfig, opts *elasticsearch.Config) error {
	transport := &http.Transport{}
	if httpcfg.MaxIdleConnsPerHost != 0 {
		transport.MaxIdleConnsPerHost = httpcfg.MaxIdleConnsPerHost
	}
	if httpcfg.ResponseHeaderTimeout != "" {
		var unit time.Duration
		unit, err := convertTimeUnit(httpcfg.ResponseHeaderTimeout)
		if err != nil {
			return err
		}
		transport.ResponseHeaderTimeout = unit
	}
	mergeTransports(opts, transport)
	return nil
}

func mergeTransports(opts *elasticsearch.Config, transport *http.Transport) {
	if opts.Transport == nil {
		opts.Transport = transport
		return
	}

	existing, _ := opts.Transport.(*http.Transport)
	// Use reflection to copy fields from transport to existing Transport
	transportValue := reflect.ValueOf(transport).Elem()
	existingValue := reflect.ValueOf(existing).Elem()
	for i := 0; i < transportValue.NumField(); i++ {
		field := transportValue.Type().Field(i)
		fieldValue := transportValue.Field(i)
		existingField := existingValue.FieldByName(field.Name)
		if existingField.IsValid() && existingField.Type() == fieldValue.Type() {
			existingField.Set(fieldValue)
		}
	}
}

func convertTimeUnit(unit string) (time.Duration, error) {
	val, ok := timeUnitMap[unit]
	if !ok {
		return 0, fmt.Errorf("unsupported time unit: %q", unit)
	}
	return val, nil

}

var timeUnitMap = map[string]time.Duration{
	"Nanosecond":  time.Nanosecond,
	"Microsecond": time.Microsecond,
	"Millisecond": time.Millisecond,
	"Second":      time.Second,
	"Minute":      time.Minute,
	"Hour":        time.Hour,
}
