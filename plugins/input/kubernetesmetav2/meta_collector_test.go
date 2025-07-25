package kubernetesmetav2

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/flags"
)

func TestGenEntityTypeKeyInfra(t *testing.T) {
	m := metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{},
	}
	*flags.ClusterType = "k8s"
	m.serviceK8sMeta.initDomain()
	assert.Equal(t, "k8s.pod", m.genEntityTypeKey("pod"))
	assert.Equal(t, "k8s.cluster", m.genEntityTypeKey("cluster"))
}

func TestGenEntityTypeKeyEmpty(t *testing.T) {
	m := metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{},
	}
	m.serviceK8sMeta.initDomain()
	assert.Equal(t, "k8s.pod", m.genEntityTypeKey("pod"))
	assert.Equal(t, "k8s.cluster", m.genEntityTypeKey("cluster"))
}
