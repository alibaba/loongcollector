package odps

import (
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

func DumpLog(context pipeline.Context, log *protocol.Log) {
	for _, content := range log.Contents {
		logger.Info(context.GetRuntimeContext(), "log Content:", content.Key, content.Value)
	}
	logger.Info(context.GetRuntimeContext(), "log Time:", log.Time)
	for _, val := range log.Values {
		logger.Info(context.GetRuntimeContext(), "log Value:", val)
	}
	logger.Info(context.GetRuntimeContext(), "log TimeNs:", log.TimeNs)
}
