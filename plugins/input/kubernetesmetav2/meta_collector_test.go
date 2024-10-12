package kubernetesmetav2

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/flags"
)

func TestGenEntityTypeKeyAcs(t *testing.T) {
	m := metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{},
	}
	*flags.ClusterType = ackCluster
	m.serviceK8sMeta.initDomain()
	assert.Equal(t, "acs.k8s.pod", m.genEntityTypeKey("pod"))
	assert.Equal(t, "acs.ack.cluster", m.genEntityTypeKey("cluster"))

	*flags.ClusterType = oneCluster
	m.serviceK8sMeta.initDomain()
	assert.Equal(t, "acs.k8s.pod", m.genEntityTypeKey("pod"))
	assert.Equal(t, "acs.one.cluster", m.genEntityTypeKey("cluster"))

	*flags.ClusterType = asiCluster
	m.serviceK8sMeta.initDomain()
	assert.Equal(t, "acs.k8s.pod", m.genEntityTypeKey("pod"))
	assert.Equal(t, "acs.asi.cluster", m.genEntityTypeKey("cluster"))
}

func TestGenEntityTypeKeyInfra(t *testing.T) {
	m := metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{},
	}
	*flags.ClusterType = "k8s"
	m.serviceK8sMeta.initDomain()
	assert.Equal(t, "infra.k8s.pod", m.genEntityTypeKey("pod"))
	assert.Equal(t, "infra.k8s.cluster", m.genEntityTypeKey("cluster"))
}

func TestGenEntityTypeKeyEmpty(t *testing.T) {
	m := metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{},
	}
	m.serviceK8sMeta.initDomain()
	assert.Equal(t, "infra.k8s.pod", m.genEntityTypeKey("pod"))
	assert.Equal(t, "infra.k8s.cluster", m.genEntityTypeKey("cluster"))
}
