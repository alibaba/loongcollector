// Copyright 2023 iLogtail Authors
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

package pulsar

import (
	"context"
	"fmt"
	"sync"

	"github.com/apache/pulsar-client-go/pulsar"
	lru "github.com/hashicorp/golang-lru/simplelru"
	"go.uber.org/multierr"

	"github.com/alibaba/ilogtail/pkg/logger"
)

type Producers struct {
	mu    sync.RWMutex
	cache *lru.LRU
}

func NewProducers(maxProducers int) *Producers {
	cache, err := lru.NewLRU(maxProducers, func(key interface{}, value interface{}) {
		producer := value.(pulsar.Producer)
		err := close(producer)
		if err != nil {
			logger.Error(context.TODO(), "close pulsar producer error", err)
			return
		}
	})
	if err != nil {
		return nil
	}
	return &Producers{
		cache: cache,
	}
}

func (p *Producers) GetProducer(topic string, client pulsar.Client, producerOptions pulsar.ProducerOptions) (pulsar.Producer, error) {
	producer, ok := p.getProducer(topic)
	if ok {
		return producer, nil
	}
	return p.getOrCreateProducer(topic, client, producerOptions)
}

func (p *Producers) Close() error {
	var errs error
	p.mu.Lock()
	for key := range p.cache.Keys() {
		if value, ok := p.cache.Peek(key); ok {
			producer := value.(pulsar.Producer)
			if err := close(producer); err != nil {
				errs = multierr.Append(errs, err)
			}
		}
	}
	p.mu.Unlock()
	return errs
}
func (p *Producers) getProducer(topic string) (pulsar.Producer, bool) {
	p.mu.RLock()
	defer p.mu.RUnlock()
	value, ok := p.cache.Get(topic)
	if ok {
		return value.(pulsar.Producer), ok
	}
	return nil, false
}

func (p *Producers) getOrCreateProducer(topic string, client pulsar.Client, producerOptions pulsar.ProducerOptions) (pulsar.Producer, error) {
	p.mu.Lock()
	value, ok := p.cache.Get(topic)
	if ok {
		p.mu.Unlock()
		return value.(pulsar.Producer), nil
	}
	newProducerOptions := pulsar.ProducerOptions{
		Topic:                   topic,
		Name:                    producerOptions.Name,
		SendTimeout:             producerOptions.SendTimeout,
		DisableBlockIfQueueFull: producerOptions.DisableBlockIfQueueFull,
		MaxPendingMessages:      producerOptions.MaxPendingMessages,
		HashingScheme:           producerOptions.HashingScheme,
		CompressionType:         producerOptions.CompressionType,
		BatchingMaxPublishDelay: producerOptions.BatchingMaxPublishDelay,
		BatchingMaxMessages:     producerOptions.BatchingMaxMessages,
	}
	newProducer, err := client.CreateProducer(newProducerOptions)
	if err == nil {
		p.cache.Add(topic, newProducer)
	} else {
		p.mu.Unlock()
		return nil, fmt.Errorf("creating pulsar producer{topic=%s} failed: %v", topic, err)
	}

	p.mu.Unlock()

	return newProducer, err
}

func close(producer pulsar.Producer) error {
	err := producer.Flush()
	if err != nil {
		err = fmt.Errorf("flush pulsar producer{topic=%s} failed: %v", producer.Topic(), err)
	}
	producer.Close()
	return err
}
