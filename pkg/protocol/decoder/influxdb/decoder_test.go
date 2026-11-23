// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package influxdb

import (
	"net/http"
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/models"
)

var textFormat = `
# integer value
cpu value=1i

# float value
cpu_load value=1

cpu_load value=1.0

cpu_load value=1.2

# boolean value
error fatal=true

# string value
event msg="logged out"

# multiple values
cpu load=10,alert=true,reason="value above maximum threshold"

cpu,host=server01,region=uswest value=1 1434055562000000000
cpu,host=server02,region=uswest value=3 1434055562000010000
temperature,machine=unit42,type=assembly internal=32,external=100 1434055562000000035
temperature,machine=unit143,type=assembly internal=22,external=130 1434055562005000035
cpu,host=server\ 01,region=uswest value=1,msg="all systems nominal"
cpu,host=server\ 01,region=us\,west value_int=1i
`

var mySQLFormat = `

cpu,host=server01,region=uswest value=1 1434055562000000000

mysql,host=Vm-Req-170328120400894271-tianchi113855.tc,server=rm-bp1eomqfte2vj91tkjo.mysql.rds.aliyuncs.com:3306 bytes_sent=19815071437i,com_assign_to_keycache=0i,com_alter_event=0i,com_alter_function=0i,com_alter_server=0i,com_alter_table=0i,aborted_clients=7738i,binlog_cache_use=136756i,binlog_stmt_cache_use=136759i,com_alter_procedure=0i,binlog_stmt_cache_disk_use=0i,bytes_received=6811387420i,com_alter_db_upgrade=0i,com_alter_instance=0i,aborted_connects=7139i,binlog_cache_disk_use=0i,com_admin_commands=3478164i,com_alter_db=0i,com_alter_tablespace=0i,com_alter_user=0i 1595818360000000000

mysql,host=Vm-Req-170328120400894271-tianchi113855.tc,server=rm-bp1eomqfte2vj91tkjo.mysql.rds.aliyuncs.com:3306 innodb_buffer_pool_read_ahead_rnd=0i,innodb_data_pending_fsyncs=0i,innodb_buffer_pool_bytes_dirty=4325376i,innodb_buffer_pool_pages_flushed=21810i,innodb_buffer_pool_pages_total=40960i,innodb_buffer_pool_read_ahead_evicted=0i,innodb_buffer_pool_reads=757i,innodb_buffer_pool_load_status="Buffer pool(s) load completed at 200702 21:33:49",innodb_buffer_pool_pages_data=846i,innodb_buffer_pool_read_ahead=0i,innodb_buffer_pool_write_requests=36830857i,innodb_data_fsyncs=344588i,innodb_buffer_pool_dump_status="Dumping of buffer pool not started",innodb_buffer_pool_pages_dirty=264i,innodb_buffer_pool_pages_misc=3i,innodb_buffer_pool_read_requests=45390218i,innodb_buffer_pool_wait_free=0i,innodb_buffer_pool_bytes_data=13860864i,innodb_buffer_pool_pages_free=40111i 1595406780000000000
`

var txtWithDotNames = `
cpu.load,host=server01,region=uswest value=1 1434055562000000000
cpu.load,host.dd=server02,region=uswest tt="xx",value=3 1434055562000010000
`

func TestMixedPointFields(t *testing.T) {
	d := &Decoder{}
	for _, tc := range []struct {
		data   string
		points int
	}{{textFormat, 13}, {mySQLFormat, 3}} {
		groups, err := d.Decode([]byte(tc.data), &http.Request{})
		if assert.NoError(t, err) && assert.Len(t, groups, 1) {
			assert.Len(t, groups[0].Events, tc.points)
		}
	}
}

func TestDecode(t *testing.T) {
	decoder := &Decoder{}
	groups, err := decoder.Decode([]byte(txtWithDotNames), &http.Request{Form: map[string][]string{"db": {"mydb"}}})
	assert.Nil(t, err)
	assert.Len(t, groups, 1) // should convert to one eventGroup

	want := &models.PipelineGroupEvents{
		Group: models.NewGroup(models.NewMetadataWithKeyValues("db", "mydb"), models.NewTags()),
		Events: []models.PipelineEvent{
			models.NewMetric("cpu.load",
				models.MetricTypeUntyped,
				models.NewTagsWithKeyValues("host", "server01", "region", "uswest"),
				1434055562000000000,
				models.NewMetricMultiValueWithMap(map[string]float64{"value": float64(1)}),
				models.NewMetricTypedValues(),
			),
			models.NewMetric("cpu.load",
				models.MetricTypeUntyped,
				models.NewTagsWithKeyValues("host.dd", "server02", "region", "uswest"),
				1434055562000010000,
				models.NewMetricMultiValueWithMap(map[string]float64{"value": float64(3)}),
				models.NewMetricTypedValueWithMap(map[string]*models.TypedValue{"tt": {Type: models.ValueTypeString, Value: "xx"}}),
			),
		},
	}
	assert.Equal(t, want, groups[0])
}

func TestDecodeExtendedFieldsIntoEvents(t *testing.T) {
	decoder := &Decoder{FieldsExtend: true}
	request := &http.Request{Form: map[string][]string{"db": {"mydb"}}}
	groups, err := decoder.Decode([]byte("weather,city=hz value=32"), request)
	assert.NoError(t, err)
	if !assert.Len(t, groups, 1) || !assert.Len(t, groups[0].Events, 1) {
		return
	}
	log, ok := groups[0].Events[0].(*models.Log)
	if !assert.True(t, ok) {
		return
	}
	assert.Equal(t, "weather", log.Contents.Get("__name__"))
	assert.Equal(t, "32", log.Contents.Get("__value__"))
	assert.Equal(t, "float", log.Contents.Get("__type__"))
	assert.Equal(t, "mydb", log.Contents.Get("__tag__:db"))
	assert.Equal(t, "city#$#hz", log.Contents.Get("__labels__"))
}
