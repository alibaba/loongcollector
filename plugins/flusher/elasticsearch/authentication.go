package elasticsearch

import (
	"fmt"
	"io/ioutil"
	"net/http"
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
	if config.TLS != nil || httpcfg != nil {
		if err := configureTLSandHTTP(httpcfg, config.TLS, opts); err != nil {
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

func configureTLSandHTTP(httpcfg *HTTPConfig, config *tlscommon.TLSConfig, opts *elasticsearch.Config) error {
	tlsConfig, err := config.LoadTLSConfig()
	if err != nil {
		return fmt.Errorf("error loading tls config: %w", err)
	}
	transport := http.Transport{}
	if tlsConfig != nil && tlsConfig.InsecureSkipVerify {
		transport.TLSClientConfig = tlsConfig
	}
	if httpcfg.MaxIdleConnsPerHost != 0 {
		transport.MaxIdleConnsPerHost = httpcfg.MaxIdleConnsPerHost
	}
	if httpcfg.ResponseHeaderTimeout != "" {
		var unit time.Duration
		unit, err = convertTimeUnit(httpcfg.ResponseHeaderTimeout)
		if err == nil {
			transport.ResponseHeaderTimeout = unit
		} else {
			return err
		}
	}
	opts.Transport = &transport
	if config.CAFile != "" {
		opts.CACert, err = ioutil.ReadFile(config.CAFile)
		if err != nil {
			return err
		}
	}
	return nil
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
