package kubernetesmetav2

import (
	"testing"

	"github.com/stretchr/testify/assert"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
)

func TestProcessServiceEntity(t *testing.T) {
	obj := &v1.Service{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "pod2",
			Namespace: "ns2",
			UID:       "uid2",
		},
		Spec: v1.ServiceSpec{
			Ports: []v1.ServicePort{
				{
					Name: "port1",
					Port: 80,
				},
				{
					Name: "port2",
					Port: 8080,
				},
			},
		},
	}
	objWrapper := &k8smeta.ObjectWrapper{
		Raw: obj,
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	log := collector.processServiceEntity(objWrapper, "create")
	assert.NotNilf(t, log, "log should not be nil")
}
