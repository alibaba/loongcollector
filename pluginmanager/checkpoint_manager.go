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

package pluginmanager

import (
	"context"
	"errors"
	"flag"
	"path/filepath"
	"strings"
	"sync"
	"time"

	"github.com/syndtr/goleveldb/leveldb"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
)

var CheckPointFile = flag.String("CheckPointFile", "go_plugin_checkpoint", "checkpoint file name, base dir(binary dir)")
var CheckPointCleanInterval = flag.Int("CheckPointCleanInterval", 600, "checkpoint clean interval, second")
var MaxCleanItemPerInterval = flag.Int("MaxCleanItemPerInterval", 1000, "max clean items per interval")

const DefaultCleanThreshold = 6 // one hour

type checkPointManager struct {
	db             *leveldb.DB
	shutdown       chan struct{}
	waitgroup      sync.WaitGroup
	initFlag       bool
	configCounter  map[string]int
	cleanThreshold int
}

var CheckPointManager checkPointManager

var ErrCheckPointNotInit = errors.New("checkpoint db not init")

func (p *checkPointManager) SaveCheckpoint(configName, key string, value []byte) error {
	if p.db == nil {
		return ErrCheckPointNotInit
	}
	err := p.db.Put([]byte(configName+"^"+key), value, nil)
	if err != nil {
		logger.Error(context.Background(), "CHECKPOINT_SAVE_ALARM", "save checkpoint error, key", key, "error", err)
	}
	return err
}

func (p *checkPointManager) GetCheckpoint(configName, key string) ([]byte, error) {
	if p.db == nil {
		return nil, ErrCheckPointNotInit
	}
	val, err := p.db.Get([]byte(configName+"^"+key), nil)
	if err != nil && err != leveldb.ErrNotFound {
		logger.Error(context.Background(), "CHECKPOINT_GET_ALARM", "get checkpoint error, key", key, "error", err)
	}
	return val, err
}

func (p *checkPointManager) DeleteCheckpoint(configName, key string) error {
	if p.db == nil {
		return ErrCheckPointNotInit
	}
	return p.db.Delete([]byte(configName+"^"+key), nil)
}

func (p *checkPointManager) Init() error {
	if p.initFlag {
		return nil
	}
	p.shutdown = make(chan struct{}, 1)
	p.configCounter = make(map[string]int)
	p.cleanThreshold = DefaultCleanThreshold
	logtailDataDir := config.LoongcollectorGlobalConfig.LoongcollectorDataDir
	pathExist, err := util.PathExists(logtailDataDir)
	var dbPath string
	if err == nil && pathExist {
		dbPath = filepath.Join(logtailDataDir, *CheckPointFile)
	} else {
		// c++程序如果这个目录创建失败会直接exit，所以这里一般应该不会走进来
		logger.Error(context.Background(), "CHECKPOINT_ALARM", "logtailDataDir not exist", logtailDataDir, "err", err)
		return err
	}

	p.db, err = leveldb.OpenFile(dbPath, nil)
	if err != nil {
		logger.Warning(context.Background(), "CHECKPOINT_ALARM", "open checkpoint error", err, "try recover db file", dbPath)
		p.db, err = leveldb.RecoverFile(dbPath, nil)
	}

	if err != nil {
		logger.Error(context.Background(), "CHECKPOINT_ALARM", "recover db file error", err)
		return err
	}
	p.initFlag = true
	logger.Info(context.Background(), "init checkpoint", "success")
	return nil
}

func (p *checkPointManager) Stop() {
	logger.Info(context.Background(), "checkpoint", "Stop")
	if p.db == nil {
		return
	}
	p.shutdown <- struct{}{}
	p.waitgroup.Wait()
}

func (p *checkPointManager) Start() {
	logger.Info(context.Background(), "checkpoint", "Start")
	if p.db == nil {
		return
	}
	p.waitgroup.Add(1)
	go p.run()
}

func (p *checkPointManager) run() {
	for {
		if util.RandomSleep(time.Second*time.Duration(*CheckPointCleanInterval), 0.1, p.shutdown) {
			logger.Info(context.Background(), "checkpoint", "Stop success")
			p.waitgroup.Done()
			return
		}
		p.check()
	}
}

func (p *checkPointManager) keyMatch(key []byte) bool {
	keyStr := string(key)
	index := strings.IndexByte(keyStr, '^')
	if index <= 0 {
		logger.Error(context.Background(), "CHECKPOINT_ALARM", "key format not match, key", keyStr)
		return false
	}
	configName := keyStr[0:index]
	// configName in checkpoint is real config Name, while configName in LogtailConfig has suffix '/1' or '/2'
	// since checkpoint is only used in input, so configName can only be 'realConfigName/1', meaning go pipeline with input
	configName += "/1"
	LogtailConfigLock.RLock()
	_, existFlag := LogtailConfig[configName]
	LogtailConfigLock.RUnlock()
	return existFlag
}

func (p *checkPointManager) check() {
	if p.db == nil {
		return
	}
	// use string to copy iter.Key()
	cleanItems := make([]string, 0, 10)
	iter := p.db.NewIterator(nil, nil)
	for iter.Next() {
		// Use key/value.
		if !p.keyMatch(iter.Key()) {
			cleanItems = append(cleanItems, string(iter.Key()))
			if len(cleanItems) >= *MaxCleanItemPerInterval {
				break
			}
		} else {
			delete(p.configCounter, string(iter.Key()))
		}
	}
	iter.Release()
	err := iter.Error()
	if err != nil {
		logger.Warning(context.Background(), "CHECKPOINT_ALARM", "iterate checkpoint error", err)
	}
	for _, key := range cleanItems {
		p.configCounter[key]++
		if p.configCounter[key] > p.cleanThreshold {
			_ = p.db.Delete([]byte(key), nil)
			logger.Info(context.Background(), "no config, delete checkpoint", key)
			delete(p.configCounter, key)
		}
	}
}
