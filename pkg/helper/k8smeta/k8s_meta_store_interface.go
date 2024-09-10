package k8smeta

import (
	"context"
	"runtime"

	"github.com/alibaba/ilogtail/pkg/logger"
)

//revive:disable:exported
type K8sMetaEvent struct {
	//revive:enable:exported
	EventType string
	Object    *ObjectWrapper
}

type ObjectWrapper struct {
	ResourceType      string
	Raw               interface{}
	FirstObservedTime int64
	LastObservedTime  int64
	Deleted           bool
}

type IdxFunc func(obj interface{}) ([]string, error)

type SendFunc func(events []*K8sMetaEvent)

func panicRecover() {
	if err := recover(); err != nil {
		trace := make([]byte, 2048)
		runtime.Stack(trace, true)
		logger.Error(context.Background(), "PLUGIN_RUNTIME_ALARM", "skywalking v2 runtime panic error", err, "stack", string(trace))
	}
}
