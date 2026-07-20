// Copyright 2026 iLogtail Authors
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

package encrypt

import (
	"encoding/hex"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func newEncryptV2(t *testing.T, sourceKeys ...string) *ProcessorEncrypt {
	processor := &ProcessorEncrypt{
		SourceKeys: sourceKeys,
		EncryptionParameters: &EncryptionInfo{
			Key: "00112233445566778899aabbccddeeff",
			IV:  strings.Repeat("0", 32),
		},
	}
	require.NoError(t, processor.Init(mock.NewEmptyContext("p", "l", "c")))
	return processor
}

func TestProcessorEncrypt_ProcessV2EncryptsSourceKey(t *testing.T) {
	processor := newEncryptV2(t, "secret")

	log := models.NewLog("", nil, "", "", "", models.NewTags(), 0)
	log.GetIndices().Add("secret", "plaintext")
	log.GetIndices().Add("plain", "kept")

	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{log}}, context)

	encrypted, _ := log.GetIndices().Get("secret").(string)
	assert.NotEqual(t, "plaintext", encrypted, "source value must be encrypted")
	assert.NotEqual(t, encryptErrorText, encrypted, "encryption must succeed")
	_, err := hex.DecodeString(encrypted)
	assert.NoError(t, err, "ciphertext must be hex-encoded")
	assert.Equal(t, "kept", log.GetIndices().Get("plain"), "non-source keys are untouched")
}

func TestProcessorEncrypt_ProcessV2PassesThroughMetric(t *testing.T) {
	processor := newEncryptV2(t, "secret")

	metric := models.NewSingleValueMetric("m", models.MetricTypeGauge, models.NewTags(), 0, 1.0)
	context := helper.NewObservePipelineContext(10)
	processor.Process(&models.PipelineGroupEvents{Events: []models.PipelineEvent{metric}}, context)

	results := context.Collector().ToArray()
	require.Len(t, results, 1)
	require.Len(t, results[0].Events, 1)
	_, ok := results[0].Events[0].(*models.Metric)
	assert.True(t, ok, "metric event must pass through unchanged")
}
