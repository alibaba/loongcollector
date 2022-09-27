package kafka

import (
	"fmt"
	"strings"

	"github.com/Shopify/sarama"
)

type SaslConfig struct {
	SaslMechanism string
}

const (
	saslTypePlaintext   = sarama.SASLTypePlaintext
	saslTypeSCRAMSHA256 = sarama.SASLTypeSCRAMSHA256
	saslTypeSCRAMSHA512 = sarama.SASLTypeSCRAMSHA512
)

func (c *SaslConfig) ConfigureSarama(config *sarama.Config) {
	switch strings.ToUpper(c.SaslMechanism) { // try not to force users to use all upper case
	case "":
		// SASL is not enabled
		return
	case saslTypePlaintext:
		config.Net.SASL.Mechanism = sarama.SASLTypePlaintext
	case saslTypeSCRAMSHA256:
		config.Net.SASL.Handshake = true
		config.Net.SASL.Mechanism = sarama.SASLTypeSCRAMSHA256
		config.Net.SASL.SCRAMClientGeneratorFunc = func() sarama.SCRAMClient {
			return &XDGSCRAMClient{HashGeneratorFcn: SHA256}
		}
	case saslTypeSCRAMSHA512:
		config.Net.SASL.Handshake = true
		config.Net.SASL.Mechanism = sarama.SASLTypeSCRAMSHA512
		config.Net.SASL.SCRAMClientGeneratorFunc = func() sarama.SCRAMClient {
			return &XDGSCRAMClient{HashGeneratorFcn: SHA512}
		}
	default:
		// This should never happen because `SaslMechanism` is checked on `Validate()`, keeping a panic to detect it earlier if it happens.
		panic(fmt.Sprintf("not valid SASL mechanism '%v', only supported with PLAIN|SCRAM-SHA-512|SCRAM-SHA-256", c.SaslMechanism))
	}
}

func (c *SaslConfig) Validate() error {
	switch strings.ToUpper(c.SaslMechanism) { // try not to force users to use all upper case
	case "", saslTypePlaintext, saslTypeSCRAMSHA256, saslTypeSCRAMSHA512:
	default:
		return fmt.Errorf("not valid SASL mechanism '%v', only supported with PLAIN|SCRAM-SHA-512|SCRAM-SHA-256", c.SaslMechanism)
	}
	return nil
}
