package kubernetesmetav2

import (
	"testing"

	"github.com/stretchr/testify/assert"
	networking "k8s.io/api/networking/v1" //nolint:typecheck

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
)

func TestProcessEmptyIngress(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &networking.Ingress{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processIngressEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}
