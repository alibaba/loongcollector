package subscriber

import (
	"fmt"
	"strings"
	"text/template"
	"time"

	openapi "github.com/alibabacloud-go/darabonba-openapi/v2/client"
	sls "github.com/alibabacloud-go/sls-20201230/v5/client"
	"github.com/alibabacloud-go/tea/tea"
	"gopkg.in/yaml.v3"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"
)

const slsName = "sls"
const SLSFlusherConfigTemplate = `
flushers:
  - Type: flusher_sls
    Aliuid: "{{.Aliuid}}"
    TelemetryType: "{{.TelemetryType}}"
    Region: {{.Region}}
    Endpoint: {{.Endpoint}}
    Project: {{.Project}}
    Logstore: {{.Logstore}}`

type SLSSubscriber struct {
	client        *sls.Client
	TelemetryType string
	Aliuid        string
	Region        string
	Endpoint      string
	QueryEndpoint string
	Project       string
	Logstore      string
}

func (s *SLSSubscriber) Name() string {
	return "sls"
}

func (s *SLSSubscriber) Description() string {
	return "this a sls subscriber"
}

func (s *SLSSubscriber) GetData(query string, startTime int32) ([]*protocol.LogGroup, error) {
	query = s.getCompleteQuery(query)
	resp, err := s.getLogFromSLS(query, startTime)
	if err != nil {
		return nil, err
	}
	var groups []*protocol.LogGroup
	group := &protocol.LogGroup{}
	for _, log := range resp.Body {
		logPb := &protocol.Log{}
		for key, value := range log {
			logPb.Contents = append(logPb.Contents, &protocol.Log_Content{
				Key:   key,
				Value: value.(string),
			})
		}
		group.Logs = append(group.Logs, logPb)
	}
	groups = append(groups, group)
	return groups, nil
}

func (s *SLSSubscriber) FlusherConfig() string {
	tpl := template.Must(template.New("slsFlusherConfig").Parse(SLSFlusherConfigTemplate))
	var builder strings.Builder
	_ = tpl.Execute(&builder, map[string]interface{}{
		"Aliuid":        s.Aliuid,
		"Region":        s.Region,
		"Endpoint":      s.Endpoint,
		"Project":       s.Project,
		"Logstore":      s.Logstore,
		"TelemetryType": s.TelemetryType,
	})
	config := builder.String()
	return config
}

func (s *SLSSubscriber) Stop() error {
	return nil
}

func (s *SLSSubscriber) UpdateConfig(configName, configYaml string) error {
	if !strings.Contains(configYaml, "flushers") {
		configYaml += s.FlusherConfig()
	}
	// Get old config first
	response, err := s.client.GetLogtailPipelineConfig(tea.String(s.Project), tea.String(configName))
	if err != nil {
		return err
	}
	if *response.StatusCode != 200 {
		return fmt.Errorf("get config %s failed, status code %d, message %s", configName, *response.StatusCode, response.Body.GoString())
	}
	config := response.Body
	// Merge config
	newConfig := make(map[string]interface{})
	err = yaml.Unmarshal([]byte(configYaml), newConfig)
	if err != nil {
		return err
	}
	if config == nil {
		return fmt.Errorf("config %s not found", configName)
	}
	// Update config
	for k, v := range newConfig {
		switch k {
		case "inputs":
			newInput := make([]map[string]interface{}, 0)
			if vArray, ok := v.([]interface{}); ok {
				for _, vMap := range vArray {
					if vMap, ok := vMap.(map[string]interface{}); ok {
						newInput = append(newInput, vMap)
					} else {
						return fmt.Errorf("invalid input type")
					}
				}
			} else {
				return fmt.Errorf("invalid input type")
			}
			config.Inputs = newInput
		case "processors":
			newProcessor := make([]map[string]interface{}, 0)
			if vArray, ok := v.([]interface{}); ok {
				for _, vMap := range vArray {
					if vMap, ok := vMap.(map[string]interface{}); ok {
						newProcessor = append(newProcessor, vMap)
					} else {
						return fmt.Errorf("invalid processor type")
					}
				}
			} else {
				return fmt.Errorf("invalid processor type")
			}
			config.Processors = newProcessor
		case "flushers":
			newFlusher := make([]map[string]interface{}, 0)
			if vArray, ok := v.([]interface{}); ok {
				for _, vMap := range vArray {
					if vMap, ok := vMap.(map[string]interface{}); ok {
						newFlusher = append(newFlusher, vMap)
					} else {
						return fmt.Errorf("invalid flusher type")
					}
				}
			} else {
				return fmt.Errorf("invalid flusher type")
			}
			config.Flushers = newFlusher
		case "global":
			if vMap, ok := v.(map[string]interface{}); ok {
				config.Global = vMap
			} else {
				return fmt.Errorf("invalid global type")
			}
		}
	}
	request := &sls.UpdateLogtailPipelineConfigRequest{
		ConfigName:  tea.String(configName),
		Inputs:      config.Inputs,
		Processors:  config.Processors,
		Flushers:    config.Flushers,
		Aggregators: config.Aggregators,
		Global:      config.Global,
	}
	fmt.Println("update config", configName, "with", request.GoString())
	updateResponse, err := s.client.UpdateLogtailPipelineConfig(tea.String(s.Project), tea.String(configName), request)
	if err != nil {
		return err
	}
	if *updateResponse.StatusCode != 200 {
		return fmt.Errorf("update config %s failed, status code %d, message %s", configName, *updateResponse.StatusCode, updateResponse.GoString())
	}
	return nil
}

func (s *SLSSubscriber) ApplyConfig(configName, machineGroup string) error {
	response, err := s.client.ApplyConfigToMachineGroup(tea.String(s.Project), tea.String(machineGroup), tea.String(configName))
	if err != nil {
		return err
	}
	if *response.StatusCode != 200 {
		return fmt.Errorf("apply config %s to machine group %s failed, status code %d, message %s", configName, machineGroup, *response.StatusCode, response.GoString())
	}
	return nil
}

func (s *SLSSubscriber) RemoveConfig(configName, machineGroup string) error {
	response, err := s.client.RemoveConfigFromMachineGroup(tea.String(s.Project), tea.String(machineGroup), tea.String(configName))
	if err != nil {
		return err
	}
	if *response.StatusCode != 200 {
		return fmt.Errorf("remove config %s from machine group %s failed, status code %d, message %s", configName, machineGroup, *response.StatusCode, response.GoString())
	}
	return nil
}

func (s *SLSSubscriber) getCompleteQuery(query string) string {
	if query == "" {
		return "*"
	}
	switch s.TelemetryType {
	case "logs":
		return query
	case "metrics":
		return fmt.Sprintf("* | select promql_query_range('%s') from metrics limit 10000", query)
	case "traces":
		return query
	default:
		return query
	}
}

func (s *SLSSubscriber) getLogFromSLS(sql string, from int32) (*sls.GetLogsResponse, error) {
	now := int32(time.Now().Unix())
	if now == from {
		now++
	}
	fmt.Println("get logs from sls with sql", sql, "from", from, "to", now, "in", config.TestConfig.GetLogstore(s.TelemetryType))
	req := &sls.GetLogsRequest{
		Query: tea.String(sql),
		From:  tea.Int32(from),
		To:    tea.Int32(now),
	}
	resp, err := s.client.GetLogs(tea.String(s.Project), tea.String(s.Logstore), req)
	if err != nil {
		return nil, err
	}
	if len(resp.Body) == 0 {
		return nil, fmt.Errorf("failed to get logs with sql %s from %v, no log", sql, from)
	}
	return resp, nil
}

func createSLSClient(accessKeyID, accessKeySecret, endpoint string) *sls.Client {
	config := &openapi.Config{
		AccessKeyId:     tea.String(accessKeyID),
		AccessKeySecret: tea.String(accessKeySecret),
		Endpoint:        tea.String(endpoint),
	}
	client, _ := sls.NewClient(config)
	return client
}

func init() {
	RegisterCreator(slsName, func(spec map[string]interface{}) (Subscriber, error) {
		l := &SLSSubscriber{}
		if v, ok := spec["aliuid"]; ok {
			l.Aliuid = v.(string)
		} else {
			l.Aliuid = config.TestConfig.Aliuid
		}
		if v, ok := spec["region"]; ok {
			l.Region = v.(string)
		} else {
			l.Region = config.TestConfig.Region
		}
		if v, ok := spec["endpoint"]; ok {
			l.Endpoint = v.(string)
		} else {
			l.Endpoint = config.TestConfig.Endpoint
		}
		if v, ok := spec["project"]; ok {
			l.Project = v.(string)
		} else {
			l.Project = config.TestConfig.Project
		}
		if v, ok := spec["logstore"]; ok {
			l.Logstore = v.(string)
		} else {
			l.Logstore = config.TestConfig.Logstore
		}
		if v, ok := spec["query_endpoint"]; ok {
			l.QueryEndpoint = v.(string)
		} else {
			l.QueryEndpoint = config.TestConfig.QueryEndpoint
		}
		if v, ok := spec["telemetry_type"]; ok {
			l.TelemetryType = v.(string)
		} else {
			l.TelemetryType = "logs"
		}
		l.client = createSLSClient(config.TestConfig.AccessKeyID, config.TestConfig.AccessKeySecret, l.QueryEndpoint)
		return l, nil
	})
	doc.Register("subscriber", slsName, new(SLSSubscriber))
}
