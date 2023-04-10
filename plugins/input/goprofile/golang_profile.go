package goprofile

import (
	"errors"

	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type GoProfile struct {
	Mode            Mode
	Interval        int32 // unit second
	Timeout         int32 // unit second
	BodyLimitSize   int32 // unit KB
	EnabledProfiles []string
	Labels          map[string]string
	Config          map[string]interface{}

	ctx     pipeline.Context
	manager *Manager
}

func (g *GoProfile) Init(context pipeline.Context) (int, error) {
	g.ctx = context
	if g.Mode == "" {
		return 0, errors.New("mode is empty")
	}
	if len(g.Config) == 0 {
		return 0, errors.New("config is empty")
	}
	return 0, nil
}

func (g *GoProfile) Description() string {
	return "Go profile support to pull go pprof profile"
}

func (g *GoProfile) Stop() error {
	g.manager.Stop()
	return nil
}

func (g *GoProfile) Start(collector pipeline.Collector) error {
	g.manager = NewManager(collector)
	return g.manager.Start(g)
}

func init() {
	pipeline.ServiceInputs["service_go_profile"] = func() pipeline.ServiceInput {
		return &GoProfile{
			// here you could set default value.
			Interval:        10,
			Timeout:         15,
			BodyLimitSize:   10240,
			EnabledProfiles: []string{"cpu", "mem", "goroutines", "mutex", "block"},
			Labels:          map[string]string{},
		}
	}

}
