package sls

const (
	LoggingURI = "logging"
)

type LoggingDetail struct {
	Type     string `json:"type"`
	Logstore string `json:"logstore"`
}

type Logging struct {
	Project        string           `json:"loggingProject"`
	LoggingDetails []*LoggingDetail `json:"loggingDetails"`
}
