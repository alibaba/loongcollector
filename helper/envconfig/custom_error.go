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
	"encoding/json"
	"regexp"

	"github.com/alibabacloud-go/tea/tea"

	"github.com/alibaba/ilogtail/pkg/logger"
)

type CustomError struct {
	Code      string
	RequestID string
	Message   string
	RawError  error
}

var popErrorRegex = `[\s\S]*ErrorCode: (.*)[\s\S]*RequestId: (.*)[\s\S]*Message: (.*)`

func (e *CustomError) CustomError() string {
	err, _ := json.Marshal(e)
	return string(err)
}

func CustomErrorNew(code string, requestID string, msg string) *CustomError {
	return &CustomError{
		Code:      code,
		RequestID: requestID,
		Message:   msg,
	}
}

type SDKErrorMsg struct {
	HTTPCode   int    `json:"httpCode"`
	RequestID  string `json:"requestId"`
	StatusCode int    `json:"statusCode"`
}

func CustomErrorFromSlsSDKError(err error) *CustomError {
	var sdkError = &tea.SDKError{}
	// 判断是否为SDKError
	if _t, ok := err.(*tea.SDKError); ok {
		sdkError = _t
	} else {
		return &CustomError{
			Code:      "SDKError",
			RequestID: "",
			Message:   err.Error(),
			RawError:  err,
		}
	}

	// 解析Data获取RequestId
	var sdkErrorMsg SDKErrorMsg
	err = json.Unmarshal([]byte(tea.StringValue(sdkError.Data)), &sdkErrorMsg)
	if err != nil {
		logger.Error(context.Background(), "unmarshal json fail : ", err.Error())
		return &CustomError{
			Code:      tea.StringValue(sdkError.Code),
			RequestID: "",
			Message:   tea.StringValue(sdkError.Message),
			RawError:  err,
		}
	}

	return &CustomError{
		Code:      tea.StringValue(sdkError.Code),
		RequestID: sdkErrorMsg.RequestID,
		Message:   tea.StringValue(sdkError.Message),
		RawError:  err,
	}
}

func CustomErrorFromPopError(popError error) *CustomError {
	compileRegex := regexp.MustCompile(popErrorRegex)
	matchArr := compileRegex.FindStringSubmatch(popError.Error())
	if len(matchArr) == 0 {
		return &CustomError{
			Code:      "PopError",
			RequestID: "",
			Message:   popError.Error(),
			RawError:  popError,
		}
	}

	return &CustomError{
		Code:      matchArr[1],
		RequestID: matchArr[2],
		Message:   matchArr[3],
		RawError:  popError,
	}
}

func GetAnnotationByError(projectInfo map[string]string, customError *CustomError) map[string]string {
	if len(customError.Code) > 0 {
		projectInfo["code"] = customError.Code
	}
	if len(customError.Message) > 0 {
		projectInfo["message"] = customError.Message
	}
	if len(customError.RequestID) > 0 {
		projectInfo["requestId"] = customError.RequestID
	}
	return projectInfo
}

func GetAnnotationByObject(config *AliyunLogConfigSpec, project, logstore, product, configName string, rawConfig bool) map[string]string {
	annotations := make(map[string]string)
	if len(project) > 0 {
		annotations["project"] = project
	}
	if len(logstore) > 0 {
		annotations["logstore"] = logstore
	}
	if len(product) > 0 {
		annotations["productCode"] = product
	}
	if len(configName) > 0 {
		annotations["configName"] = configName
	}

	if rawConfig {
		jsonStr, err := json.Marshal(config)
		if err == nil {
			annotations["logtailConfig"] = string(jsonStr)
		}
	}
	return annotations
}
