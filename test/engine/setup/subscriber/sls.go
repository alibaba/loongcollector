package subscriber

import (
	"fmt"
	"strings"
	"sync"
	"text/template"
	"time"

	openapi "github.com/alibabacloud-go/darabonba-openapi/v2/client"
	sls "github.com/alibabacloud-go/sls-20201230/v5/client"
	"github.com/alibabacloud-go/tea/tea"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"
)

const slsName = "sls"
const SLSFlusherConfigTemplate = `
flushers:
  - Type: flusher_sls
    Aliuid: "{{.Aliuid}}"
    TelemetryType: "logs"
    Region: {{.Region}}
    Endpoint: {{.Endpoint}}
    Project: {{.Project}}
    Logstore: {{.Logstore}}`

var SLSFlusherConfig string
var SLSFlusherConfigOnce sync.Once

const queryCountSQL = "* | SELECT * FROM log WHERE from_unixtime(__time__) >= from_unixtime(%v) AND from_unixtime(__time__) < now()"

type SLSSubscriber struct {
	client *sls.Client
}

func (s *SLSSubscriber) Name() string {
	return "sls"
}

func (s *SLSSubscriber) Description() string {
	return "this a sls subscriber"
}

func (s *SLSSubscriber) GetData(startTime int32) ([]*protocol.LogGroup, error) {
	resp, err := s.getLogFromSLS(fmt.Sprintf(queryCountSQL, startTime), startTime)
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
	SLSFlusherConfigOnce.Do(func() {
		tpl := template.Must(template.New("slsFlusherConfig").Parse(SLSFlusherConfigTemplate))
		var builder strings.Builder
		_ = tpl.Execute(&builder, map[string]interface{}{
			"Aliuid":   config.TestConfig.Aliuid,
			"Region":   config.TestConfig.Region,
			"Endpoint": config.TestConfig.Endpoint,
			"Project":  config.TestConfig.Project,
			"Logstore": config.TestConfig.Logstore,
		})
		SLSFlusherConfig = builder.String()
	})
	return SLSFlusherConfig
}

func (s *SLSSubscriber) Stop() error {
	return nil
}

func (s *SLSSubscriber) getLogFromSLS(sql string, from int32) (*sls.GetLogsResponse, error) {
	now := int32(time.Now().Unix())
	if now == from {
		now++
	}
	req := &sls.GetLogsRequest{
		Query: tea.String(sql),
		From:  tea.Int32(from),
		To:    tea.Int32(now),
	}
	resp, err := s.client.GetLogs(tea.String(config.TestConfig.Project), tea.String(config.TestConfig.Logstore), req)
	if err != nil {
		return nil, err
	}
	if len(resp.Body) == 0 {
		return nil, fmt.Errorf("failed to get logs with sql %s, no log", sql)
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
		l := &SLSSubscriber{
			client: createSLSClient(config.TestConfig.AccessKeyID, config.TestConfig.AccessKeySecret, config.TestConfig.QueryEndpoint),
		}
		return l, nil
	})
	doc.Register("subscriber", lokiName, new(LokiSubscriber))
}
