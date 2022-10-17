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

package kafkav2

import (
	"fmt"
	"strings"

	"github.com/Shopify/sarama"
)

const (
	saslTypePlaintext   = sarama.SASLTypePlaintext
	saslTypeSCRAMSHA256 = sarama.SASLTypeSCRAMSHA256
	saslTypeSCRAMSHA512 = sarama.SASLTypeSCRAMSHA512
)

type Authentication struct {
	// plaintext authentication
	PlainText *PlainTextConfig
	// SASL authentication
	SASL *SaslConfig
}

type SaslConfig struct {
	// SASL Mechanism to be used, possible values are: (PLAIN, SCRAM-SHA-256 or SCRAM-SHA-512)
	SaslMechanism string
	// The username for connecting to Kafka.
	Username string
	// The password for connecting to Kafka.
	Password string
}

type PlainTextConfig struct {
	// The username for connecting to Kafka.
	Username string
	// The password for connecting to Kafka.
	Password string
}

func (config *Authentication) ConfigureAuthentication(saramaConfig *sarama.Config) error {
	if config.PlainText != nil {
		if err := config.PlainText.ConfigurePlaintext(saramaConfig); err != nil {
			return err
		}
	}

	if config.SASL != nil {
		if err := config.SASL.ConfigureSasl(saramaConfig); err != nil {
			return err
		}
	}
	return nil
}

func (plainTextConfig *PlainTextConfig) ConfigurePlaintext(saramaConfig *sarama.Config) error {
	// Validate Auth info
	if plainTextConfig.Username != "" && plainTextConfig.Password == "" {
		return fmt.Errorf("PlainTextConfig password must be set when username is configured")
	}

	if plainTextConfig.Username != "" {
		saramaConfig.Net.SASL.Enable = true
		saramaConfig.Net.SASL.User = plainTextConfig.Username
		saramaConfig.Net.SASL.Password = plainTextConfig.Password
	}
	return nil
}

func (saslConfig *SaslConfig) ConfigureSasl(saramaConfig *sarama.Config) error {
	if saslConfig.Username == "" {
		return fmt.Errorf("username have to be provided")
	}

	if saslConfig.Password == "" {
		return fmt.Errorf("password have to be provided")
	}

	saramaConfig.Net.SASL.Enable = true
	saramaConfig.Net.SASL.User = saslConfig.Username
	saramaConfig.Net.SASL.Password = saslConfig.Password
	switch strings.ToUpper(saslConfig.SaslMechanism) { // try not to force users to use all upper case
	case "":
		// SASL is not enabled
	case saslTypePlaintext:
		saramaConfig.Net.SASL.Mechanism = sarama.SASLTypePlaintext
	case saslTypeSCRAMSHA256:
		saramaConfig.Net.SASL.Handshake = true
		saramaConfig.Net.SASL.Mechanism = sarama.SASLTypeSCRAMSHA256
		saramaConfig.Net.SASL.SCRAMClientGeneratorFunc = func() sarama.SCRAMClient {
			return &XDGSCRAMClient{HashGeneratorFcn: SHA256}
		}
	case saslTypeSCRAMSHA512:
		saramaConfig.Net.SASL.Handshake = true
		saramaConfig.Net.SASL.Mechanism = sarama.SASLTypeSCRAMSHA512
		saramaConfig.Net.SASL.SCRAMClientGeneratorFunc = func() sarama.SCRAMClient {
			return &XDGSCRAMClient{HashGeneratorFcn: SHA512}
		}
	default:
		// This should never happen because `SaslMechanism` is checked on `Validate()`, keeping a panic to detect it earlier if it happens.
		return fmt.Errorf("not valid SASL mechanism '%v', only supported with PLAIN|SCRAM-SHA-512|SCRAM-SHA-256", saslConfig.SaslMechanism)
	}
	return nil
}

func (saslConfig *SaslConfig) Validate() error {
	switch strings.ToUpper(saslConfig.SaslMechanism) { // try not to force users to use all upper case
	case "", saslTypePlaintext, saslTypeSCRAMSHA256, saslTypeSCRAMSHA512:
	default:
		return fmt.Errorf("not valid SASL mechanism '%v', only supported with PLAIN|SCRAM-SHA-512|SCRAM-SHA-256", saslConfig.SaslMechanism)
	}
	return nil
}
