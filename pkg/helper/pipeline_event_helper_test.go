package helper

import (
	"testing"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestTransferMetricEventMultiValueRoundTrip(t *testing.T) {
	tags := models.NewTags()
	tags.Add("hostname", "host-1")
	multiValue := models.NewMetricMultiValueWithMap(map[string]float64{
		"cpu":            0.2,
		"memory_used_mb": 30,
	})
	metric := models.NewMultiValuesMetric("agent", models.MetricTypeUntyped, tags, 1234567890, multiValue.Values)

	pbMetric, err := TransferMetricEventToPB(metric)
	require.NoError(t, err)

	restored, err := TransferPBToMetricEvent(pbMetric)
	require.NoError(t, err)
	assert.Equal(t, "agent", restored.GetName())
	assert.True(t, restored.GetValue().IsMultiValues())
	assert.Equal(t, 0.2, restored.GetValue().GetMultiValues().Get("cpu"))
	assert.Equal(t, 30.0, restored.GetValue().GetMultiValues().Get("memory_used_mb"))
}
