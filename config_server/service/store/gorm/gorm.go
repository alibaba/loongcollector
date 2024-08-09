// Copyright 2024 iLogtail Authors
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

package gorm

import (
	"errors"
	"fmt"
	"log"
	"os"
	"reflect"
	"regexp"
	"strings"
	"time"

	"config-server/model"
	"config-server/setting"
	database "config-server/store/interface_database"

	"gorm.io/driver/mysql"
	"gorm.io/driver/postgres"
	"gorm.io/driver/sqlite"
	"gorm.io/driver/sqlserver"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

type Store struct {
	db *gorm.DB
}

func (s *Store) Connect() error {
	slowLogger := logger.New(
		log.New(os.Stdout, "\r\n", log.LstdFlags),
		logger.Config{
			SlowThreshold:             1 * time.Second,
			LogLevel:                  logger.Warn,
			IgnoreRecordNotFoundError: true,
			Colorful:                  true,
		},
	)
	option := gorm.Config{
		Logger: slowLogger,
	}
	var db *gorm.DB
	var err error
	driver := setting.GetSetting().Driver
	dsn := setting.GetSetting().Dsn
	switch driver {
	case "mysql":
		db, err = gorm.Open(mysql.Open(dsn), &option)
	case "postgre":
		db, err = gorm.Open(postgres.Open(dsn), &option)
	case "sqlite":
		db, err = gorm.Open(sqlite.Open(dsn), &option)
	case "sqlserver":
		db, err = gorm.Open(sqlserver.Open(dsn), &option)
	default:
		return errors.New("wrong gorm driver: " + driver)
	}
	if err != nil {
		return err
	}
	if setting.GetSetting().AutoMigrateSchema {
		db.AutoMigrate(&model.AgentGroup{}, &model.ConfigDetail{}, &model.Agent{})
	}
	s.db = db
	return nil
}

func (s *Store) GetMode() string {
	return "gorm"
}

func (s *Store) Close() error {
	db, err := s.db.DB()
	if err != nil {
		return err
	}
	return db.Close()
}

func (s *Store) Get(table string, entityKey string) (interface{}, error) {
	model, pk, err := initModelAndPrimaryKey(table)
	if err != nil {
		return nil, err
	}
	err = s.db.Table(table).Where(fmt.Sprintf("%s = ?", pk), entityKey).Take(&model).Error
	if err != nil {
		return nil, err
	}
	return model, nil
}

func (s *Store) Add(table string, entityKey string, entity interface{}) error {
	err := s.db.Table(table).Create(entity).Error
	if err != nil {
		return err
	}
	return nil
}

func (s *Store) Update(table string, entityKey string, entity interface{}) error {
	err := s.db.Table(table).Save(entity).Error
	if err != nil {
		return err
	}
	return nil
}

func (s *Store) Has(table string, entityKey string) (bool, error) {
	_, pk, err := initModelAndPrimaryKey(table)
	if err != nil {
		return false, err
	}
	var count int64
	err = s.db.Table(table).Where(fmt.Sprintf("%s = ?", pk), entityKey).Limit(1).Count(&count).Error
	if err != nil {
		return false, err
	}
	return count > 0, nil
}

func (s *Store) Delete(table string, entityKey string) error {
	_, pk, err := initModelAndPrimaryKey(table)
	if err != nil {
		return err
	}
	err = s.db.Table(table).Where(fmt.Sprintf("%s = ?", pk), entityKey).Delete(nil).Error
	if err != nil {
		return err
	}
	return nil
}

func (s *Store) GetAll(table string) ([]interface{}, error) {
	return s.GetWithPairs(table)
}

func (s *Store) GetWithPairs(table string, pairs ...interface{}) ([]interface{}, error) {
	if len(pairs)%2 != 0 {
		return nil, errors.New("params must be in pairs")
	}

	objectType, ok := model.ModelTypes[table]
	if !ok {
		return nil, errors.New("unsupported table: " + table)
	}

	objectsType := reflect.SliceOf(objectType.Elem())
	objects := reflect.New(objectsType)

	db := s.db.Table(table)
	for i := 0; i < len(pairs); i += 2 {
		column := camelCaseToSnakeCase(fmt.Sprintf("%v", pairs[i]))
		value := pairs[i+1]
		db = db.Where(fmt.Sprintf("%s = ?", column), value)
	}

	err := db.Find(objects.Interface()).Error
	if err != nil {
		return nil, err
	}

	objects = objects.Elem()

	result := make([]interface{}, objects.Len())
	for i := 0; i < objects.Len(); i++ {
		result[i] = objects.Index(i).Interface()
	}
	return result, nil
}

func (s *Store) Count(table string) (int, error) {
	var count int64
	err := s.db.Table(table).Count(&count).Error
	if err != nil {
		return 0, err
	}
	return int(count), nil
}

func (s *Store) WriteBatch(batch *database.Batch) error {
	tx := s.db.Begin()

	for !batch.Empty() {
		data := batch.Pop()
		_, pk, err := initModelAndPrimaryKey(data.Table)
		if err != nil {
			tx.Rollback()
			return err
		}
		if data.Opt == database.OptDelete {
			err = tx.Table(data.Table).Where(fmt.Sprintf("%s = ?", pk), data.Key).Delete(nil).Error
			if err != nil {
				tx.Rollback()
				return err
			}
		} else if data.Opt == database.OptAdd || data.Opt == database.OptUpdate {
			isExist, err := s.Has(data.Table, data.Key)
			if err != nil {
				tx.Rollback()
				return err
			}
			if isExist {
				err = tx.Table(data.Table).Save(data.Value).Error
			} else {
				err = tx.Table(data.Table).Create(data.Value).Error
			}
			if err != nil {
				tx.Rollback()
				return err
			}
		}
	}

	if err := tx.Commit().Error; err != nil {
		tx.Rollback()
		return err
	}
	return nil
}

func initModelAndPrimaryKey(table string) (interface{}, string, error) {
	m, ok := model.Models[table]
	if !ok {
		return nil, "", errors.New("unknown table: " + table)
	}

	ans := reflect.New(reflect.TypeOf(m).Elem()).Interface()
	t := reflect.TypeOf(ans).Elem()
	for i := 0; i < t.NumField(); i++ {
		field := t.Field(i)
		if tag := field.Tag.Get("gorm"); strings.Contains(tag, "primaryKey") {
			column := getPrimaryKeyColumn(tag)
			if column == "" {
				return ans, field.Name, nil
			}
			return ans, column, nil
		}
	}
	return ans, "", errors.New(table + " primary key not found")
}

func getPrimaryKeyColumn(s string) string {
	splitStrings := strings.Split(s, ";")
	for _, splitString := range splitStrings {
		if strings.Contains(splitString, "column") {
			columnParts := strings.Split(splitString, ":")
			if len(columnParts) == 2 {
				return columnParts[1]
			}
		}
	}
	return ""
}

func camelCaseToSnakeCase(s string) string {
	r := regexp.MustCompile("([a-z0-9])([A-Z])")
	snakeCase := r.ReplaceAllString(s, "${1}_${2}")
	return strings.ToLower(snakeCase)
}
