package addfieldsv2

import (
	"os"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newProcessor() (*ProcessorAddFields, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	processor := &ProcessorAddFields{
		Fields: map[string]interface{}{
			"a": "1",
		},
		IgnoreIfExist: true,
	}
	err := processor.Init(ctx)
	return processor, err
}

// TestSourceKey ...
func TestSourceKey(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_key", Value: "test_value"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "a", Value: "6"})
	processor.processLog(log)
	assert.Equal(t, "test_value", log.Contents[0].Value)
	assert.Equal(t, "6", log.Contents[1].Value)
}

// TestIgnoreIfExistFalse...
func TestIgnoreIfExistFalse(t *testing.T) {
	processor, err := newProcessor()
	processor.IgnoreIfExist = false
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_key", Value: "test_value"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "a", Value: "6"})
	processor.processLog(log)
	assert.Equal(t, "test_value", log.Contents[0].Value)
	assert.Equal(t, "6", log.Contents[1].Value)
	assert.Equal(t, "1", log.Contents[2].Value)
}

// TestIgnoreIfExistTrue...
func TestIgnoreIfExistTrue(t *testing.T) {
	processor, err := newProcessor()
	processor.IgnoreIfExist = true
	require.NoError(t, err)
	log := &protocol.Log{Time: 0}
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "test_key", Value: "test_value"})
	log.Contents = append(log.Contents, &protocol.Log_Content{Key: "a", Value: "6"})
	processor.processLog(log)
	assert.Equal(t, "test_value", log.Contents[0].Value)
	assert.Equal(t, "6", log.Contents[1].Value)
}

// TestAddArrayFields ...
func TestAddArrayFields(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	processor.Fields = map[string]interface{}{
		"key1": "value1",
		"key2": []string{"value21", "value22"},
	}
	log := &protocol.Log{Time: 0}

	processor.processLog(log)
	assert.Equal(t, "[\"value21\",\"value22\"]", log.Contents[1].Value)
}

// TestAddObjectFields ...
func TestAddObjectFields(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	processor.Fields = map[string]interface{}{
		"key1": "value1",
		"key2": map[string]interface{}{
			"key31": "value31",
			"key32": []string{"value32"},
		},
	}
	log := &protocol.Log{Time: 0}

	processor.processLog(log)
	assert.Equal(t, "{\"key31\":\"value31\",\"key32\":[\"value32\"]}", log.Contents[1].Value)
}

// TestAddBoolFields ...
func TestAddBoolFields(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	processor.Fields = map[string]interface{}{
		"key1": false,
	}
	log := &protocol.Log{Time: 0}

	processor.processLog(log)
	assert.Equal(t, "false", log.Contents[0].Value)
}

// TestAddInnerFuncFields ...
func TestAddInnerFuncFields(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	processor.Fields = map[string]interface{}{
		"key1": "${uuid()}",
		"key2": "${env(RANDOM_STR)}",
		"key3": "${timestamp_ms()}",
		"key4": "${timestamp_ns()}",
	}
	log := &protocol.Log{Time: 0}

	_ = os.Setenv("RANDOM_STR", "abcdefg")

	processor.processLog(log)
	t.Log(log.Contents)
}
