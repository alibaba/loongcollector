package kubernetesmetav2

import (
	"testing"

	"github.com/stretchr/testify/assert"
	batch "k8s.io/api/batch/v1" //nolint:typecheck

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
)

func TestProcessEmptyJob(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &batch.Job{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processJobEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}

func TestProcessEmptyCronJob(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &batch.CronJob{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processCronJobEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}
