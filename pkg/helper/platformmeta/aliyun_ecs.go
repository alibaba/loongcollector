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

package platformmeta

import (
	"context"
	"errors"
	"io"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
)

// global var
var (
	error404 = errors.New("404")
)

func AlibabaCloudEcsPlatformRequest(api string, method string, f func(header *http.Header)) (string, error) {
	r, _ := http.NewRequest(http.MethodPut, "http://100.100.100.200/latest"+api, nil)
	r.Method = method
	f(&r.Header)
	c := new(http.Client)
	c.Timeout = time.Second
	resp, err := c.Do(r)
	if err != nil {
		return "", err
	}
	defer func() {
		_ = resp.Body.Close()
	}()
	logger.Debug(context.Background(), "api", r.URL.Path)
	if resp.StatusCode == 404 {
		return "", error404
	}
	bytes, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}
	return string(bytes), nil
}

func AlibabaCloudEcsPlatformReadMetaVal(api string, token string) (val string, err error) {
	for i := 0; i < 2; i++ {
		val, err = AlibabaCloudEcsPlatformRequest(api, http.MethodGet, func(header *http.Header) {
			(*header)["X-aliyun-ecs-metadata-token"] = []string{token}
		})
		if err == nil || err == error404 {
			return
		}
	}
	return
}

type tag struct {
	k string
	v string
}
type Data struct {
	// unchanged meta
	id           string
	region       string
	zone         string
	instanceType string
	imageID      string

	// dynamic changed meta
	name          string
	tags          map[string]string
	maxNetEngress int64
	maxNetIngress int64
	vpcID         string
	vswitchID     string
}

type ECSManager struct {
	mutex                   sync.RWMutex
	data                    Data
	ecsToken                string
	ecsLastFetchTokenTime   time.Time
	ecsMinimumFetchInterval time.Duration
	ecsTokenExpireTime      int
	fetchRes                bool
	once                    sync.Once
	unchangedAlreadyRead    bool
	resChan                 chan bool
}

func (m *ECSManager) fetchToken() (err error) {
	var val string
	for i := 0; i < 2; i++ {
		val, err = AlibabaCloudEcsPlatformRequest("/api/token", http.MethodPut, func(header *http.Header) {
			(*header)["X-aliyun-ecs-metadata-token-ttl-seconds"] = []string{strconv.Itoa(m.ecsTokenExpireTime)}
		})
		if err == nil {
			break
		}
	}
	if err != nil {
		return err
	}
	m.ecsToken = val
	return nil
}

func (m *ECSManager) startFetch() {
	m.once.Do(func() {
		m.fetchAPI()
		go func() {
			for range time.NewTicker(m.ecsMinimumFetchInterval).C {
				m.mutex.Lock()
				m.fetchAPI()
				m.mutex.Unlock()
			}
		}()
	})
}

func (m *ECSManager) fetchAPI() {
	defer func() {
		logger.Debug(context.Background(), "fetch ecs meta api res", m.fetchRes)
	}()
	now := time.Now()
	if now.Sub(m.ecsLastFetchTokenTime).Seconds() > float64(m.ecsTokenExpireTime)*3/4 {
		if err := m.fetchToken(); err != nil {
			logger.Error(context.Background(), "ECS_ALARM", "read token error", err)
			return
		}
		m.ecsLastFetchTokenTime = now
	}

	for k := range m.data.tags {
		delete(m.data.tags, k)
	}

	asyncCount := 0
	asyncReadMetaFunc := func(api string, key string, configFunc func(key, val string)) {
		asyncCount++
		go func() {
			val, err := AlibabaCloudEcsPlatformReadMetaVal(api, m.ecsToken)
			if err != nil && err != error404 {
				logger.Error(context.Background(), "ECS_ALARM", "read meta error", err)
				m.resChan <- false
				return
			}
			configFunc(key, val)
			m.resChan <- true
		}()

	}
	success := true

	if !m.unchangedAlreadyRead {
		asyncReadMetaFunc("/meta-data/instance-id", "", func(key, val string) { m.data.id = val })
		asyncReadMetaFunc("/meta-data/region-id", "", func(key, val string) { m.data.region = val })
		asyncReadMetaFunc("/meta-data/zone-id", "", func(key, val string) { m.data.zone = val })
		asyncReadMetaFunc("/meta-data/image-id", "", func(key, val string) { m.data.imageID = val })
		asyncReadMetaFunc("/meta-data/instance/instance-type", "", func(key, val string) { m.data.instanceType = val })
		for i := 0; i < asyncCount; i++ {
			ok := <-m.resChan
			success = success && ok
		}
		asyncCount = 0
	}
	if !success {
		return
	}
	m.unchangedAlreadyRead = true
	var tags string
	asyncReadMetaFunc("/meta-data/instance/max-netbw-egress", "", func(key, val string) { m.data.maxNetEngress, _ = strconv.ParseInt(val, 10, 64) })
	asyncReadMetaFunc("/meta-data/instance/max-netbw-ingress", "", func(key, val string) { m.data.maxNetIngress, _ = strconv.ParseInt(val, 10, 64) })
	asyncReadMetaFunc("/meta-data/instance/instance-name", "", func(key, val string) { m.data.name = val })
	asyncReadMetaFunc("/meta-data/vswitch-id", "", func(key, val string) { m.data.vswitchID = val })
	asyncReadMetaFunc("/meta-data/vpc-id", "", func(key, val string) { m.data.vpcID = val })
	asyncReadMetaFunc("/meta-data/tags/instance/", "", func(key, val string) { tags = val })
	for i := 0; i < asyncCount; i++ {
		ok := <-m.resChan
		success = success && ok
	}
	asyncCount = 0
	if success && tags != "" {
		keys := strings.Split(tags, "\n")
		num := 0
		for i, key := range keys {
			key = strings.TrimSpace(key)
			if key == "" {
				continue
			}
			keys[num] = keys[i]
			num++
		}
		keys = keys[:num]
		res := make(chan *tag, len(keys))
		for _, key := range keys {
			asyncReadMetaFunc("/meta-data/tags/instance/"+key, key, func(key, val string) {
				res <- &tag{
					k: key,
					v: val,
				}
			})
		}
		for i := 0; i < len(keys); i++ {
			<-m.resChan
		}
		count := len(res)
		for i := 0; i < count; i++ {
			t := <-res
			m.data.tags[t.k] = t.v
		}
	}
	m.fetchRes = true
}

func (m *ECSManager) StartCollect() {
	m.startFetch()
}

func (m *ECSManager) GetInstanceID() string {
	if !m.fetchRes {
		return ""
	}
	return m.data.id
}

func (m *ECSManager) GetInstanceImageID() string {
	if !m.fetchRes {
		return ""
	}
	return m.data.imageID
}

func (m *ECSManager) GetInstanceRegion() string {
	if !m.fetchRes {
		return ""
	}
	return m.data.region
}

func (m *ECSManager) GetInstanceZone() string {
	if !m.fetchRes {
		return ""
	}
	return m.data.zone
}

func (m *ECSManager) GetInstanceType() string {
	if !m.fetchRes {
		return ""
	}
	return m.data.instanceType
}

func (m *ECSManager) GetInstanceName() string {
	m.mutex.RLock()
	defer m.mutex.RUnlock()
	if !m.fetchRes {
		return ""
	}
	return m.data.name
}

func (m *ECSManager) GetInstanceMaxNetEgress() int64 {
	m.mutex.RLock()
	defer m.mutex.RUnlock()
	if !m.fetchRes {
		return -1
	}
	return m.data.maxNetEngress
}

func (m *ECSManager) GetInstanceMaxNetIngress() int64 {
	m.mutex.RLock()
	defer m.mutex.RUnlock()
	if !m.fetchRes {
		return -1
	}
	return m.data.maxNetIngress
}

func (m *ECSManager) GetInstanceVpcID() string {
	m.mutex.RLock()
	defer m.mutex.RUnlock()
	if !m.fetchRes {
		return ""
	}
	return m.data.vpcID
}

func (m *ECSManager) GetInstanceVswitchID() string {
	m.mutex.RLock()
	defer m.mutex.RUnlock()
	if !m.fetchRes {
		return ""
	}
	return m.data.vswitchID
}

func (m *ECSManager) GetInstanceTags() map[string]string {
	m.mutex.RLock()
	defer m.mutex.RUnlock()
	if !m.fetchRes {
		return map[string]string{}
	}
	res := make(map[string]string)
	for k, v := range m.data.tags {
		res[k] = v
	}
	return res
}

func (m *ECSManager) Ping() bool {
	_, err := AlibabaCloudEcsPlatformRequest("/meta-data/instance-id", http.MethodGet, func(header *http.Header) {
	})
	if err != nil && strings.Contains(err.Error(), "Timeout") {
		return false
	}
	return true
}

func initAliyun() {
	e := &ECSManager{
		data: Data{
			tags: map[string]string{},
		},
		resChan: make(chan bool, 30),
	}
	var val int
	_ = util.InitFromEnvInt("ALIYUN_ECS_MINIMUM_REFLUSH_INTERVAL", &val, 30)
	e.ecsMinimumFetchInterval = time.Second * time.Duration(val)
	_ = util.InitFromEnvInt("ALIYUN_ECS_TOKEN_EXPIRE_TIME", &e.ecsTokenExpireTime, 300)
	register[Aliyun] = e
}
