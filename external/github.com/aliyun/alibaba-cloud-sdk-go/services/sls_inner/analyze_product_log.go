package sls_inner

//Licensed under the Apache License, Version 2.0 (the "License");
//you may not use this file except in compliance with the License.
//You may obtain a copy of the License at
//
//http://www.apache.org/licenses/LICENSE-2.0
//
//Unless required by applicable law or agreed to in writing, software
//distributed under the License is distributed on an "AS IS" BASIS,
//WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//See the License for the specific language governing permissions and
//limitations under the License.
//
// Code generated by Alibaba Cloud SDK Code Generator.
// Changes may cause incorrect behavior and will be lost if the code is regenerated.

import (
	"github.com/aliyun/alibaba-cloud-sdk-go/sdk/requests"
	"github.com/aliyun/alibaba-cloud-sdk-go/sdk/responses"
)

// AnalyzeProductLog invokes the sls_inner.AnalyzeProductLog API synchronously
// api document: https://help.aliyun.com/api/sls-inner/analyzeproductlog.html
func (client *Client) AnalyzeProductLog(request *AnalyzeProductLogRequest) (response *AnalyzeProductLogResponse, err error) {
	response = CreateAnalyzeProductLogResponse()
	err = client.DoAction(request, response)
	return
}

// AnalyzeProductLogWithChan invokes the sls_inner.AnalyzeProductLog API asynchronously
// api document: https://help.aliyun.com/api/sls-inner/analyzeproductlog.html
// asynchronous document: https://help.aliyun.com/document_detail/66220.html
func (client *Client) AnalyzeProductLogWithChan(request *AnalyzeProductLogRequest) (<-chan *AnalyzeProductLogResponse, <-chan error) {
	responseChan := make(chan *AnalyzeProductLogResponse, 1)
	errChan := make(chan error, 1)
	err := client.AddAsyncTask(func() {
		defer close(responseChan)
		defer close(errChan)
		response, err := client.AnalyzeProductLog(request)
		if err != nil {
			errChan <- err
		} else {
			responseChan <- response
		}
	})
	if err != nil {
		errChan <- err
		close(responseChan)
		close(errChan)
	}
	return responseChan, errChan
}

// AnalyzeProductLogWithCallback invokes the sls_inner.AnalyzeProductLog API asynchronously
// api document: https://help.aliyun.com/api/sls-inner/analyzeproductlog.html
// asynchronous document: https://help.aliyun.com/document_detail/66220.html
func (client *Client) AnalyzeProductLogWithCallback(request *AnalyzeProductLogRequest, callback func(response *AnalyzeProductLogResponse, err error)) <-chan int {
	result := make(chan int, 1)
	err := client.AddAsyncTask(func() {
		var response *AnalyzeProductLogResponse
		var err error
		defer close(result)
		response, err = client.AnalyzeProductLog(request)
		callback(response, err)
		result <- 1
	})
	if err != nil {
		defer close(result)
		callback(nil, err)
		result <- 0
	}
	return result
}

// AnalyzeProductLogRequest is the request struct for api AnalyzeProductLog
type AnalyzeProductLogRequest struct {
	*requests.RpcRequest
	Project       string           `position:"Query" name:"Project"`
	CloudProduct  string           `position:"Query" name:"CloudProduct"`
	ResourceQuota string           `position:"Query" name:"ResourceQuota"`
	Lang          string           `position:"Query" name:"Lang"`
	Region        string           `position:"Query" name:"Region"`
	Logstore      string           `position:"Query" name:"Logstore"`
	Overwrite     string           `position:"Query" name:"Overwrite"`
	VariableMap   string           `position:"Query" name:"VariableMap"`
	TTL           requests.Integer `position:"Query" name:"TTL"`
	HotTTL        requests.Integer `position:"Query" name:"HotTTL"`
}

// AnalyzeProductLogResponse is the response struct for api AnalyzeProductLog
type AnalyzeProductLogResponse struct {
	*responses.BaseResponse
	RequestId string `json:"RequestId" xml:"RequestId"`
	Code      string `json:"Code" xml:"Code"`
	Success   string `json:"Success" xml:"Success"`
	Message   string `json:"Message" xml:"Message"`
}

// CreateAnalyzeProductLogRequest creates a request to invoke AnalyzeProductLog API
func CreateAnalyzeProductLogRequest() (request *AnalyzeProductLogRequest) {
	request = &AnalyzeProductLogRequest{
		RpcRequest: &requests.RpcRequest{},
	}
	request.InitWithApiInfo("Sls", "2019-10-23", "AnalyzeProductLog", "", "")
	return
}

// CreateAnalyzeProductLogResponse creates a response to parse from AnalyzeProductLog response
func CreateAnalyzeProductLogResponse() (response *AnalyzeProductLogResponse) {
	response = &AnalyzeProductLogResponse{
		BaseResponse: &responses.BaseResponse{},
	}
	return
}
