// Copyright 2022 iLogtail Authors
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

package leveldb

import (
	"encoding/json"
	"log"

	"github.com/syndtr/goleveldb/leveldb"

	"config-server/common"
	"config-server/model"
	"config-server/setting"
	database "config-server/store/interface_database"
)

var dbPath = []string{
	common.TypeAgentAttributes,
	common.TypeAgent,
	common.TypeConfigDetail,
	common.TypeAgentGROUP,
	common.TypeCommand,
}

type Store struct {
	db map[string]*leveldb.DB
}

func (l *Store) Connect() error {
	l.db = make(map[string]*leveldb.DB)

	var err error
	for _, c := range dbPath {
		l.db[c], err = leveldb.OpenFile(setting.GetSetting().DbPath+"/"+c, nil)
		if err != nil {
			return err
		}
	}

	return nil
}

func (l *Store) GetMode() string {
	return "leveldb"
}

func (l *Store) Close() error {
	for _, db := range l.db {
		err := db.Close()
		if err != nil {
			return err
		}
	}
	return nil
}

func (l *Store) Get(table string, entityKey string) (interface{}, error) {
	value, err := l.db[table].Get(generateKey(entityKey), nil)
	if err != nil {
		return nil, err
	}
	return parseValue(table, value), nil
}

func (l *Store) Add(table string, entityKey string, entity interface{}) error {
	key := generateKey(entityKey)
	value, err := generateValue(entity)
	if err != nil {
		return err
	}

	err = l.db[table].Put(key, value, nil)
	if err != nil {
		return err
	}
	return nil
}

func (l *Store) Update(table string, entityKey string, entity interface{}) error {
	key := generateKey(entityKey)
	value, err := generateValue(entity)
	if err != nil {
		return err
	}

	err = l.db[table].Put(key, value, nil)
	if err != nil {
		return err
	}
	return nil
}

func (l *Store) Has(table string, entityKey string) (bool, error) {
	key := generateKey(entityKey)
	ok, err := l.db[table].Has(key, nil)
	if err != nil {
		return false, err
	}
	return ok, nil
}

func (l *Store) Delete(table string, entityKey string) error {
	key := generateKey(entityKey)
	err := l.db[table].Delete(key, nil)
	if err != nil {
		return err
	}
	return nil
}

func (l *Store) GetAll(table string) ([]interface{}, error) {
	ans := make([]interface{}, 0)

	iter := l.db[table].NewIterator(nil, nil)
	for iter.Next() {
		ans = append(ans, parseValue(table, iter.Value()))
	}

	iter.Release()
	err := iter.Error()
	if err != nil {
		return nil, err
	}
	return ans, nil
}

func (l *Store) Count(table string) (int, error) {
	var ans int

	iter := l.db[table].NewIterator(nil, nil)
	for iter.Next() {
		ans++
	}

	iter.Release()
	err := iter.Error()
	if err != nil {
		return 0, err
	}
	return ans, nil
}

func (l *Store) WriteBatch(batch *database.Batch) error {
	batchTemp := *batch
	var leveldbBatch map[string]*leveldb.Batch = make(map[string]*leveldb.Batch)

	for !batch.Empty() {
		data := batch.Pop()
		key := generateKey(data.Key)

		if _, ok := leveldbBatch[data.Table]; !ok {
			leveldbBatch[data.Table] = new(leveldb.Batch)
		}

		if data.Opt == database.OptDelete {
			leveldbBatch[data.Table].Delete(key)
		} else if data.Opt == database.OptAdd || data.Opt == database.OptUpdate {
			value, err := generateValue(data.Value)
			if err != nil {
				*batch = batchTemp
				return err
			}
			leveldbBatch[data.Table].Put(key, value)
		}
	}

	for t, b := range leveldbBatch {
		err := l.db[t].Write(b, nil)
		if err != nil {
			*batch = batchTemp
			return err
		}
	}

	return nil
}

func generateKey(key string) []byte {
	return []byte(key)
}

func generateValue(entity interface{}) ([]byte, error) {
	value, err := json.Marshal(entity)
	if err != nil {
		return nil, err
	}
	return value, nil
}

func parseValue(table string, data []byte) interface{} {
	var ans interface{}
	switch table {
	case common.TypeAgentAttributes:
		ans = new(model.AgentAttributes)
	case common.TypeAgent:
		ans = new(model.Agent)
	case common.TypeConfigDetail:
		ans = new(model.ConfigDetail)
	case common.TypeAgentGROUP:
		ans = new(model.AgentGroup)
	case common.TypeCommand:
		ans = new(model.Command)
	}
	err := json.Unmarshal(data, ans)
	if err != nil {
		log.Println(err.Error())
	}
	return ans
}
