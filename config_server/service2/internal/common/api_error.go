package common

import (
	"fmt"
	"reflect"
)

type ApiError struct {
	Code    int
	Message string
}

func (apiError ApiError) Error() string {
	return fmt.Sprintf("Code:%d,Messgae:%s", apiError.Code, apiError.Message)
}

func ErrorWithMsg(code int, msg string) *ApiError {
	var apiErrorResult = ApiError{
		Code:    Failed.Code,
		Message: Failed.Message,
	}
	if code != 0 {
		apiErrorResult.Code = code
	}
	if msg != "" {
		apiErrorResult.Message = msg
	}
	PrintLog(apiErrorResult.Code, apiErrorResult.Message, 3)
	return &apiErrorResult
}

func ServerErrorWithMsg(msg string, a ...any) *ApiError {
	if a == nil || len(a) == 0 {
		return ErrorWithMsg(Failed.Code, msg)
	}
	return ErrorWithMsg(Failed.Code, fmt.Sprintf(msg, a...))
}

func ServerError() *ApiError {
	return ErrorWithMsg(Failed.Code, Failed.Message)
}

//这里切记不要返回APIError,否则会出现问题（因为(ApiError)(nil)!=nil）

func SystemError(err error) error {
	if err == nil {
		return err
	}
	//errors.Is work不了
	if reflect.TypeOf(err) == reflect.TypeOf(&ApiError{}) {
		return err
	}

	if err.Error() == "" {
		return ErrorWithMsg(SystemFailed.Code, SystemFailed.Message)
	}
	return ErrorWithMsg(SystemFailed.Code, err.Error())
}

func ValidateErrorWithMsg(msg string) *ApiError {
	return ErrorWithMsg(ValidateFailed.Code, msg)
}

func ValidateError() *ApiError {
	return ErrorWithMsg(ValidateFailed.Code, ValidateFailed.Message)
}
