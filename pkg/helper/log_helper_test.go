package helper

import (
	"github.com/alibaba/ilogtail/pkg/config"

	"github.com/stretchr/testify/require"

	"testing"
)

func TestMetricLabels_Append(t *testing.T) {
	var ml MetricLabels
	ml.Append("key2", "val")
	ml.Append("key", "val")
	log := NewMetricLog("name", 1691646109945, 1, &ml)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"key#$#val|key2#$#val" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

	ml.Replace("key", "val2")

	log = NewMetricLog("name", 1691646109945, 1, &ml)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"key#$#val2|key2#$#val" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

	ml.Replace("key3", "val3")
	log = NewMetricLog("name", 1691646109945, 1, &ml)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"key#$#val2|key2#$#val|key3#$#val3" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

	cloneLabel := ml.Clone()
	cloneLabel.Replace("key3", "val4")
	log = NewMetricLog("name", 1691646109945, 1, cloneLabel)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"key#$#val2|key2#$#val|key3#$#val4" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

	log = NewMetricLog("name", 1691646109945, 1, &ml)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"key#$#val2|key2#$#val|key3#$#val3" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

	log = NewMetricLog("name", 1691646109945, 1, nil)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

	log = NewMetricLog("name", 1691646109945, 1, nil)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

	var ml2 MetricLabels
	config.LogtailGlobalConfig.EnableSlsMetricsFormat = true
	ml2.Append("key@", "val|")

	log = NewMetricLog("name@", 1691646109945, 1, &ml2)
	require.Equal(t, `Time:1691646109 Contents:<Key:"__name__" Value:"name_" > Contents:<Key:"__time_nano__" Value:"1691646109945000000" > Contents:<Key:"__labels__" Value:"key_#$#val_" > Contents:<Key:"__value__" Value:"1" > Time_ns:945000000 `, log.String())

}
