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

package envconfig

import (
	"context"

	"github.com/alibaba/ilogtail/pkg/logger"

	productAPI "github.com/aliyun/alibaba-cloud-sdk-go/services/sls_inner"
)

func createProductClient() (*productAPI.Client, error) {
	if *AliCloudECSFlag {
		accessKeyID, accessKeySecret, stsToken, _, err := UpdateTokenFunction()
		if err != nil {
			return nil, err
		}
		return productAPI.NewClientWithStsToken("cn-hangzhou", accessKeyID, accessKeySecret, stsToken)
	}
	if len(*DefaultSTSToken) > 0 {
		return productAPI.NewClientWithStsToken("cn-hangzhou", *DefaultAccessKeyID, *DefaultAccessKeySecret, *DefaultSTSToken)
	}
	return productAPI.NewClientWithAccessKey("cn-hangzhou", *DefaultAccessKeyID, *DefaultAccessKeySecret)
}

func recorrectRegion(region string) string {
	if region == "cn-shenzhen-finance-1" {
		return "cn-shenzhen-finance"
	}
	return region
}

// CreateProductLogstore create product logstore
//	"cn-hangzhou"
//	"test-hangzhou-project"
//	"audit-logstore"
//	"k8s-audit"
//	"cn"
func CreateProductLogstore(region, project, logstore, product, lang string) error {
	client, err := createProductClient()
	if err != nil {
		return err
	}

	// Create an API request and set parameters
	request := productAPI.CreateAnalyzeProductLogRequest()
	request.Domain = *ProductAPIDomain
	request.Region = recorrectRegion(region)
	request.Project = project
	request.Logstore = logstore
	request.CloudProduct = product
	// not refresh ttl
	request.Overwrite = "false"
	// not refresh index and dashboard
	request.VariableMap = "{\"overwriteIndex\":\"no_overwrite\", \"overwriteDashboard\":\"no_overwrite\"}"
	request.Lang = lang
	resp, err := client.AnalyzeProductLog(request)
	if err != nil {
		return err
	}
	logger.Info(context.Background(), "create product success, region", region, "project", project, "logstore", logstore, "product", product, "lang", lang, "response", *resp, "domain", *ProductAPIDomain)
	return nil
}
