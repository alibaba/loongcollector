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

package baseagg

// // Sample code to use AggregatorDefault as base in other aggregators such AggregatorShardHash.
//
// import (
// 	"ilogtail"
// 	"sync"
// 	"time"
// )
//
// // Queue to wrap que passed through Init.
// type queueWrapper struct {
// 	// The target queue.
// 	queue ilogtail.LogGroupQueue
// }
//
// func (q *queueWrapper) DoYourOwnOperation(logGroup *ilogtail.LogGroup) {
// 	// TODO: ...
//
// 	// For example, add extra tags for logGroup.
// 	// NOTE: must check, the logGroup might be passed to this method multiple times
// 	// if it is added because of  quick flush policy, which might fail to Add.
// 	const tagKey = "my_tag"
// 	alreadyAdded := false
// 	for _, tag := range logGroup.LogTags {
// 		if tag.Key == tagKey {
// 			alreadyAdded = true
// 			break
// 		}
// 	}
// 	if !alreadyAdded {
// 		logGroup.LogTags = append(logGroup.LogTags, &ilogtail.LogTag{Key: tagKey, Value: "my_value"})
// 	}
// }
//
// func (q *queueWrapper) Add(logGroup *ilogtail.LogGroup) error {
// 	q.DoYourOwnOperation(logGroup)
// 	// Pass to real queue.
// 	return q.queue.Add(logGroup)
// }
//
// func (q *queueWrapper) AddWithWait(logGroup *ilogtail.LogGroup, duration time.Duration) error {
// 	q.DoYourOwnOperation(logGroup)
// 	// Pass to real queue.
// 	return q.queue.AddWithWait(logGroup, duration)
// }
//
// // Your aggregator that uses AggregatorDefault.
// type outerAggregator struct {
// 	defaultAgg AggregatorDefault
//
// 	q    *queueWrapper
// 	lock *sync.Mutex
// }
//
// // Init ...
// func (p *outerAggregator) Init(context ilogtail.Context, que ilogtail.LogGroupQueue) (int, error) {
// 	// Wrap the que.
// 	p.q = &queueWrapper{queue: que}
//
// 	// Init AggregatorDefault with wrapped queue.
// 	_, err := p.defaultAgg.Init(context, p.q)
// 	if err != nil {
// 		// Handle err here.
// 		return 0, err
// 	}
//
// 	// If you want change some parameters of AggregatorDefault, call InitInner.
// 	// For example, disable pack ID generation in AggregatorDefault.
// 	p.defaultAgg.InitInner(false, "",
// 		p.lock,
// 		"",
// 		"",
// 		1024,
// 		4)
//
// 	// Your own initializing logic.
// 	// ...
// 	return 0, nil
// }
//
// // Flush ...
// func (p *outerAggregator) Flush() []*ilogtail.LogGroup {
// 	logGroupArray := p.defaultAgg.Flush()
// 	for _, logGroup := range logGroupArray {
// 		// do your own operation here
// 		// make sure that logGroupArray is handled before flushing
// 		p.q.DoYourOwnOperation(logGroup)
// 	}
// 	return logGroupArray
// }
//
// // Implement other methods in interface.
// // ...
//
// func NewAggregator() *outerAggregator {
// 	return &outerAggregator{
// 		defaultAgg: AggregatorDefault{},
// 		lock:       &sync.Mutex{},
// 	}
// }
