package skywalkingv3

import (
	"github.com/alibaba/ilogtail"
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	agent "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
)

type perfDataHandler interface {
	collectorPerfData(logs *agent.BrowserPerfData) (*v3.Commands, error)
}

type perfDataHandlerImpl struct {
	context   ilogtail.Context
	collector ilogtail.Collector
}

func (p perfDataHandlerImpl) collectorPerfData(perfData *agent.BrowserPerfData) (*v3.Commands, error) {
	return &v3.Commands{}, nil
}
