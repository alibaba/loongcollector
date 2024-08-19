// Licensed to Apache Software Foundation (ASF) under one or more contributor
// license agreements. See the NOTICE file distributed with
// this work for additional information regarding copyright
// ownership. Apache Software Foundation (ASF) licenses this file to you under
// the Apache License, Version 2.0 (the "License"); you may
// not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

package main

import (
	"database/sql"
	"fmt"
	"net/http"
	"os"
	"strings"
	"time"

	_ "github.com/go-sql-driver/mysql"
)

const (
	dealInitSleepSecond = 2
)

var DB *sql.DB
var dataBase = "root:root@tcp(mysql:3306)/mysql"

func getConnection() {
	//Start mysql client using `server`'s config
	var err error
	DB, err = sql.Open("mysql", dataBase)
	if err != nil {
		panic(err)
	}
	err = DB.Ping()
	if err != nil {
		panic(err)
	}
}

// dbCreate Create action using DB information
func dbCreate() int64 {
	getConnection()
	nowTime := time.Now().Unix()
	queryCreateTable := fmt.Sprintf("CREATE TABLE IF NOT EXISTS `specialalarmtest%v` %v",
		nowTime,
		"(`id` int(11) unsigned NOT NULL AUTO_INCREMENT,"+
			"`time` datetime NOT NULL,"+
			" `alarmtype` varchar(64) NOT NULL,"+
			"`ip` varchar(16) NOT NULL,"+
			"`count` int(11) unsigned NOT NULL,"+
			"PRIMARY KEY (`id`),"+
			"KEY `time` (`time`) USING BTREE,"+
			"KEY `alarmtype` (`alarmtype`) USING BTREE) AUTO_INCREMENT=1;")
	DBExecute(queryCreateTable)
	time.Sleep(time.Second * dealInitSleepSecond)
	return nowTime
}

func DBExecute(query string) {
	fmt.Println("INFO: Executing ", query)
	if hasCreate := strings.Contains(strings.ToLower(query), strings.ToLower("CREATE TABLE")); hasCreate {
		fmt.Println("INFO: Executing ", query)
	}
	_, err := DB.Exec(query)
	if err != nil {
		panic(fmt.Sprintf("ERROR: Failed to execute sql commands, err:%v", err))
	}
}

// dbInsert Insert action using DB information
func dbInsert(insertTimes int) int64 {
	nowTime := dbCreate()
	queryInsert := fmt.Sprintf("insert into specialalarmtest%v"+
		" (`time`, `alarmType`, `ip`, `count`)"+
		" values(now(), \"NO_ALARM\", \"10.10.***.***\", 0);", nowTime)
	for i := 0; i < insertTimes; i++ {
		DBExecute(queryInsert)
	}
	return nowTime
}

func main() {
	http.HandleFunc("/add/data", func(w http.ResponseWriter, r *http.Request) {
		dbInsert(3)
	})
	fmt.Printf("=========Client is starting=========")
	err := http.ListenAndServe(":10999", nil)
	if err != nil {
		fmt.Printf("start error: %v", err)
		os.Exit(1)
	}
}
