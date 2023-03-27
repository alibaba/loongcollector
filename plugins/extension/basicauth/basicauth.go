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

package basicauth

import (
	"net/http"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
)

// ensure ExtensionBasicAuth implements the extensions.ClientAuthenticator interface
var _ extensions.ClientAuthenticator = (*ExtensionBasicAuth)(nil)

type ExtensionBasicAuth struct {
	Username string
	Password string
}

func (b *ExtensionBasicAuth) Description() string {
	return "basic auth extension to add auth info to client request"
}

func (b *ExtensionBasicAuth) Init(context pipeline.Context) error {
	return nil
}

func (b *ExtensionBasicAuth) Stop() error {
	return nil
}

func (b *ExtensionBasicAuth) RoundTripper(base http.RoundTripper) (http.RoundTripper, error) {
	return &basicAuthRoundTripper{base: base, auth: b}, nil
}

type basicAuthRoundTripper struct {
	base http.RoundTripper
	auth *ExtensionBasicAuth
}

func (b *basicAuthRoundTripper) RoundTrip(request *http.Request) (*http.Response, error) {
	request.SetBasicAuth(b.auth.Username, b.auth.Password)
	return b.base.RoundTrip(request)
}

func init() {
	pipeline.AddExtensionCreator("ext_basicauth", func() pipeline.Extension {
		return &ExtensionBasicAuth{}
	})
}
