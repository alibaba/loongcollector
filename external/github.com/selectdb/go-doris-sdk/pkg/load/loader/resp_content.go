package load

import (
	jsoniter "github.com/json-iterator/go"
)

type LoadResponse struct {
	Status       LoadStatus
	Resp         RespContent
	ErrorMessage string
}

type LoadStatus int

const (
	FAILURE LoadStatus = iota
	SUCCESS
)

// String returns the string representation of LoadStatus
func (s LoadStatus) String() string {
	switch s {
	case SUCCESS:
		return "SUCCESS"
	case FAILURE:
		return "FAILURE"
	default:
		return "UNKNOWN"
	}
}

// RespContent represents the response from a stream load operation
type RespContent struct {
	TxnID                  int64  `json:"TxnId"`
	Label                  string `json:"Label"`
	Status                 string `json:"Status"`
	TwoPhaseCommit         string `json:"TwoPhaseCommit"`
	ExistingJobStatus      string `json:"ExistingJobStatus"`
	Message                string `json:"Message"`
	NumberTotalRows        int64  `json:"NumberTotalRows"`
	NumberLoadedRows       int64  `json:"NumberLoadedRows"`
	NumberFilteredRows     int    `json:"NumberFilteredRows"`
	NumberUnselectedRows   int    `json:"NumberUnselectedRows"`
	LoadBytes              int64  `json:"LoadBytes"`
	LoadTimeMs             int    `json:"LoadTimeMs"`
	BeginTxnTimeMs         int    `json:"BeginTxnTimeMs"`
	StreamLoadPutTimeMs    int    `json:"StreamLoadPutTimeMs"`
	ReadDataTimeMs         int    `json:"ReadDataTimeMs"`
	WriteDataTimeMs        int    `json:"WriteDataTimeMs"`
	CommitAndPublishTimeMs int    `json:"CommitAndPublishTimeMs"`
	ErrorURL               string `json:"ErrorURL"`
}

// String returns a JSON representation of the response content
func (r *RespContent) String() string {
	json := jsoniter.ConfigCompatibleWithStandardLibrary
	bytes, err := json.Marshal(r)
	if err != nil {
		return ""
	}
	return string(bytes)
}
