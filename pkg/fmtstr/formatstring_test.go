package fmtstr

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestFormatString(t *testing.T) {
	topic := "kafka_%{app_name}"
	fields := map[string]string{
		"app_name": "ilogtail",
	}

	expectTopic := "kafka_ilogtail"

	sf, _ := Compile(topic, func(key string, ops []VariableOp) (FormatEvaler, error) {
		if v, found := fields[key]; found {
			return StringElement{v}, nil
		}
		return StringElement{key}, nil
	})
	actualTopic, _ := sf.Run(nil)

	assert.Equal(t, expectTopic, actualTopic)
}

func TestCompileKeys(t *testing.T) {
	topic := "kafka_%{app_name}"
	expectTopicKeys := []string{"app_name"}
	topicKeys, _ := CompileKeys(topic)
	assert.Equal(t, expectTopicKeys, topicKeys)
}
