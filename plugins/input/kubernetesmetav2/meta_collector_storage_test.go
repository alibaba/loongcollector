package kubernetesmetav2

import (
	"testing"

	"github.com/stretchr/testify/assert"
	storage "k8s.io/api/storage/v1" //nolint:typecheck

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
)

func TestProcessEmptyStorageClass(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &storage.StorageClass{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processStorageClassEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}
