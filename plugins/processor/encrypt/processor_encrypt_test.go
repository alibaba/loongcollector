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

package encrypt

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"io"
	"os"
	"os/exec"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	pluginmanager "github.com/alibaba/ilogtail/pluginmanager"
)

func getRandomBytes(len int) []byte {
	b := make([]byte, len)
	if _, err := io.ReadFull(rand.Reader, b); err != nil {
		panic(err)
	}
	return b
}

// nil means default value.
func newProcessor(sourceKeys []string, key []byte, iv []byte, keyFilePath string) *ProcessorEncrypt {
	p := newProcessorEncrypt()
	if sourceKeys != nil {
		p.SourceKeys = sourceKeys
	} else {
		p.SourceKeys = []string{"source", "source2"}
	}
	if len(key) == 0 {
		p.EncryptionParameters.Key = hex.EncodeToString(getRandomBytes(32))
	} else {
		p.EncryptionParameters.Key = string(key)
	}
	if iv != nil {
		p.EncryptionParameters.IV = string(iv)
	}
	p.EncryptionParameters.KeyFilePath = keyFilePath

	ctxImpl := &pluginmanager.ContextImp{}
	ctxImpl.InitContext("test", "test", "test")
	ctxImpl.RegisterMetricRecord(make([]pipeline.LabelPair, 0))
	_ = p.Init(ctxImpl)
	return p
}

func testProcess(t *testing.T, p *ProcessorEncrypt) {
	logCount := 3
	testF := func(text string) {
		var inputArray []*protocol.Log
		for c := 0; c < logCount; c++ {
			lg := &protocol.Log{}
			lg.Contents = append(lg.Contents, &protocol.Log_Content{Key: "test", Value: "test"})
			for _, sourceName := range p.SourceKeys {
				lg.Contents = append(lg.Contents, &protocol.Log_Content{Key: sourceName, Value: text})
			}
			lg.Contents = append(lg.Contents, &protocol.Log_Content{Key: "test2", Value: "test"})
			inputArray = append(inputArray, lg)
		}
		logArray := p.ProcessLogs(inputArray)
		require.Equal(t, 3, len(logArray))

		for idx := 0; idx < logCount; idx++ {
			ciphertext := logArray[idx].Contents[2].Value
			require.Equal(t, 0, len(ciphertext)%(p.blockSize*2))
			iv := p.EncryptionParameters.IV
			if len(iv) == 0 { // IV is generated randomly, read from ciphertext.
				iv = ciphertext[:p.blockSize*2]
				ciphertext = ciphertext[p.blockSize*2:]
			}

			inputFilePath := "/tmp/test_aes_input"
			outputFilePath := "/tmp/test_aes_output"
			ciphertextBytes, err := hex.DecodeString(ciphertext)
			require.NoError(t, err)
			require.NoError(t, os.WriteFile(inputFilePath, ciphertextBytes, 0600|0755))
			require.NoError(t, exec.Command("openssl", "enc", "-d", "-aes-256-cbc", "-iv", iv, //nolint:gosec
				"-K", p.EncryptionParameters.Key, "-in", inputFilePath, "-out", outputFilePath).Run())

			plaintext, err := os.ReadFile(outputFilePath)
			require.NoError(t, err)
			require.Equal(t, text, string(plaintext))
		}
	}

	testF("0123456")          // padding
	testF("0123456789012345") // padding all
	testF("")                 // empty
}

func TestDefault(t *testing.T) {
	p := newProcessor(nil, nil, nil, "")
	testProcess(t, p)
}

func TestKeyFile(t *testing.T) {
	fileBytes, err := json.Marshal(struct{ Key string }{hex.EncodeToString(getRandomBytes(32))})
	require.NoError(t, err)
	keyFilePath := "/tmp/test_aes_key.json"
	require.NoError(t, os.WriteFile(keyFilePath, fileBytes, 0600|0755))
	p := newProcessor(nil, []byte("file://"+keyFilePath), nil, keyFilePath)

	testProcess(t, p)
}

// [DISABLED] Random IV
// func TestRandomIV(t *testing.T) {
//	p := newProcessor(nil, nil, []byte(""))
//
//	testProcess(t, p)
// }

func benchmarkEncrypt(b *testing.B, p *ProcessorEncrypt, valueLen int) {
	valueBytes := make([]byte, valueLen)
	_, err := io.ReadFull(rand.Reader, valueBytes)
	require.NoError(b, err)
	value := string(valueBytes)

	b.ResetTimer()
	for loop := 0; loop < b.N; loop++ {
		p.ProcessLogs([]*protocol.Log{{
			Contents: []*protocol.Log_Content{
				{Key: p.SourceKeys[0], Value: value},
			},
		}})
	}
}

func BenchmarkConstantIV(b *testing.B) {
	p := newProcessor([]string{"source"}, nil, nil, "")

	benchmarkEncrypt(b, p, 255)
}

// [DISABLED] Random IV
// func BenchmarkRandomIV(b *testing.B) {
//	p := newProcessor([]string{"source"}, nil, []byte(""))
//
//	benchmarkEncrypt(b, p, 255)
// }
