package kubernetesmetav2

import (
	"testing"

	"github.com/stretchr/testify/assert"
	app "k8s.io/api/apps/v1" //nolint:typecheck

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
)

func TestProcessEmptyDeployment(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &app.Deployment{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processDeploymentEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}

func TestProcessEmptyDaemonSet(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &app.DaemonSet{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processDaemonSetEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}

func TestProcessEmptyStatefulSet(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &app.StatefulSet{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processStatefulSetEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}

func TestProcessEmptyReplicaSet(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &app.ReplicaSet{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processReplicaSetEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}
