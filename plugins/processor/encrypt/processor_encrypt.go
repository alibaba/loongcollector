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
	"bytes"
	"crypto/aes"
	"crypto/cipher"
	"crypto/md5" //nolint:gosec
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"strings"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const (
	pluginName       = "processor_encrypt"
	defaultAlarmType = "PROCESSOR_ENCRYPT_ALARM"
	encryptErrorText = "ENCRYPT_ERROR"
)

type EncryptionInfo struct {
	// Load parameters from file, it will override the value specified through independent parameters.
	KeyFilePath string

	// Key to encrypt data, if it starts with file://, read key from the specified file.
	Key string
	// Random IV for each log if it is empty, and it will be prepended to ciphertext, all 0 by default.
	IV string
}

func newEncryptionInfo() *EncryptionInfo {
	return &EncryptionInfo{
		IV: strings.Repeat("0", 16*2),
	}
}

type ProcessorEncrypt struct {
	SourceKeys             []string
	EncryptionParameters   *EncryptionInfo
	KeepSourceValueIfError bool

	context   pipeline.Context
	keyDict   map[string]bool
	cipher    cipher.Block
	blockSize int
	key       []byte
	iv        []byte

	encryptedCountMetric pipeline.CounterMetric
	encryptedBytesMetric pipeline.CounterMetric
}

func (p *ProcessorEncrypt) Init(context pipeline.Context) error {
	p.context = context
	if len(p.SourceKeys) == 0 {
		return fmt.Errorf("plugin %v must specify SourceKey", pluginName)
	}
	p.keyDict = make(map[string]bool)
	for _, key := range p.SourceKeys {
		p.keyDict[key] = true
	}
	if err := p.parseKey(); err != nil {
		return err
	}
	if err := p.parseIV(); err != nil {
		return err
	}

	metricsRecord := p.context.GetMetricRecord()
	p.encryptedCountMetric = helper.NewCounterMetricAndRegister(metricsRecord, "encrypted_count")
	p.encryptedBytesMetric = helper.NewCounterMetricAndRegister(metricsRecord, "encrypted_bytes")
	return nil
}

func (*ProcessorEncrypt) Description() string {
	return fmt.Sprintf("processor %v is used to encrypt data with AES CBC", pluginName)
}

func (p *ProcessorEncrypt) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	if p.key == nil {
		return logArray
	}
	for _, log := range logArray {
		p.processLog(log)
	}
	return logArray
}

// @return (the index of encrypted field,  -1 means not found
// any error,  nil if the field is not found)
func (p *ProcessorEncrypt) processLog(log *protocol.Log) {
	for _, cont := range log.Contents {
		if _, exists := p.keyDict[cont.Key]; !exists {
			continue
		}

		p.encryptedCountMetric.Add(1)
		p.encryptedBytesMetric.Add(int64(len(cont.Value)))
		ciphertext, err := p.encrypt(cont.Value)
		if err == nil {
			cont.Value = hex.EncodeToString(ciphertext)
		} else {
			logger.Errorf(p.context.GetRuntimeContext(), defaultAlarmType, "encrypt field %v error: %v", cont.Key, err)
			if !p.KeepSourceValueIfError {
				cont.Value = encryptErrorText
			}
		}
	}
}

func (p *ProcessorEncrypt) encrypt(plaintext string) ([]byte, error) {
	paddingData := p.paddingWithPKCS7(plaintext)
	var ciphertext []byte
	iv := p.iv
	if iv != nil {
		ciphertext = make([]byte, len(paddingData))
	} else {
		ciphertext = make([]byte, p.blockSize+len(paddingData))
		iv = ciphertext[:p.blockSize]
		if _, err := io.ReadFull(rand.Reader, iv); err != nil {
			return nil, fmt.Errorf("construct IV error: %v", iv)
		}
	}

	mode := cipher.NewCBCEncrypter(p.cipher, iv)
	if p.iv != nil {
		mode.CryptBlocks(ciphertext, paddingData)
	} else {
		mode.CryptBlocks(ciphertext[p.blockSize:], paddingData)
	}
	return ciphertext, nil
}

func (p *ProcessorEncrypt) paddingWithPKCS7(data string) []byte {
	paddingSize := p.blockSize - len(data)%p.blockSize
	dataBytes := make([]byte, len(data)+paddingSize)
	copy(dataBytes, data)
	copy(dataBytes[len(data):], bytes.Repeat([]byte{byte(paddingSize)}, paddingSize))
	return dataBytes
}

func (p *ProcessorEncrypt) parseKey() error {
	if len(p.EncryptionParameters.Key) == 0 && len(p.EncryptionParameters.KeyFilePath) == 0 {
		return fmt.Errorf("plugin %v must specify Key or KeyFilePath", pluginName)
	}
	var err error
	if len(p.EncryptionParameters.KeyFilePath) > 0 {
		// Key is stored in local file in JSON format, load from file.
		var fileBytes []byte
		if fileBytes, err = os.ReadFile(p.EncryptionParameters.KeyFilePath); err == nil {
			err = json.Unmarshal(fileBytes, p.EncryptionParameters)
		}
		if err != nil {
			return fmt.Errorf("plugin %v loads key file %v error: %v",
				pluginName, p.EncryptionParameters.KeyFilePath, err)
		}

		logger.Infof(p.context.GetRuntimeContext(), "read key from file %v, hash: %v", p.EncryptionParameters.KeyFilePath,
			hex.EncodeToString(md5.New().Sum([]byte(p.EncryptionParameters.Key)))) //nolint:gosec
	}

	// Decode from hex to bytes.
	if p.key, err = hex.DecodeString(p.EncryptionParameters.Key); err != nil {
		return fmt.Errorf("plugin %v decodes key from hex error: %v, try hex", pluginName, err)
	}
	if p.cipher, err = aes.NewCipher(p.key); err != nil {
		return fmt.Errorf("plugin %s create cipher with key error: %v", pluginName, err)
	}
	p.blockSize = p.cipher.BlockSize()
	return nil
}

func (p *ProcessorEncrypt) parseIV() error {
	if len(p.EncryptionParameters.IV) == 0 {
		return fmt.Errorf("plugin %s must specify IV", pluginName)
		// [DISABLED] Random IV
		// logger.Infof("IV is not specified, use random IV and prepend it to ciphertext")
		// return nil
	}

	var err error
	if p.iv, err = hex.DecodeString(p.EncryptionParameters.IV); err != nil {
		return fmt.Errorf("plugin %v decodes IV %v error: %v", pluginName, p.EncryptionParameters.IV, err)
	}
	if len(p.iv) != p.blockSize {
		return fmt.Errorf("plugin %v finds size mismatch between IV(%v) and BlockSize(%v), must be same",
			pluginName, len(p.iv), p.blockSize)
	}
	return nil
}

func newProcessorEncrypt() *ProcessorEncrypt {
	return &ProcessorEncrypt{
		EncryptionParameters:   newEncryptionInfo(),
		KeepSourceValueIfError: false,
	}
}

func init() {
	pipeline.Processors[pluginName] = func() pipeline.Processor {
		return newProcessorEncrypt()
	}
}
