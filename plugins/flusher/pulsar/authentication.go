// Copyright 2023 iLogtail Authors
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

package pulsar

import "github.com/apache/pulsar-client-go/pulsar"

type Authentication struct {
	TLS    *TLS
	Token  *Token
	Athenz *Athenz
	OAuth2 *OAuth2
}

// TLS is the interface used to configure a tcp client or server from a `Config`
type TLS struct {
	// Path to the TLS cert to use for TLS required connections. (optional)
	CertFile string
	// Path to the TLS key to use for TLS required connections. (optional)
	KeyFile string
}

type Token struct {
	Token string
}

type Athenz struct {
	ProviderDomain  string
	TenantDomain    string
	TenantService   string
	PrivateKey      string
	KeyID           string
	PrincipalHeader string
	ZtsURL          string
}

type OAuth2 struct {
	Enabled    bool
	IssuerURL  string
	Audience   string
	PrivateKey string
	Scope      string
}

func (authentication *Authentication) Auth() pulsar.Authentication {
	if authentication.TLS != nil {
		return pulsar.NewAuthenticationTLS(authentication.TLS.CertFile, authentication.TLS.KeyFile)
	}

	if authentication.Token != nil {
		return pulsar.NewAuthenticationToken(authentication.Token.Token)
	}

	if authentication.OAuth2 != nil {
		oauth2Map := map[string]string{
			"type":       "client_credentials",
			"issuerUrl":  authentication.OAuth2.IssuerURL,
			"privateKey": authentication.OAuth2.PrivateKey,
		}
		if len(authentication.OAuth2.Audience) > 0 {
			oauth2Map["audience"] = authentication.OAuth2.Audience
		}
		if len(authentication.OAuth2.Scope) > 0 {
			oauth2Map["scope"] = authentication.OAuth2.Scope
		}
		return pulsar.NewAuthenticationOAuth2(oauth2Map)
	}

	if authentication.Athenz != nil {
		return pulsar.NewAuthenticationAthenz(map[string]string{
			"providerDomain":  authentication.Athenz.ProviderDomain,
			"tenantDomain":    authentication.Athenz.TenantDomain,
			"tenantService":   authentication.Athenz.TenantService,
			"privateKey":      authentication.Athenz.PrivateKey,
			"keyId":           authentication.Athenz.KeyID,
			"principalHeader": authentication.Athenz.PrincipalHeader,
			"ztsUrl":          authentication.Athenz.ZtsURL,
		})
	}
	return nil
}
