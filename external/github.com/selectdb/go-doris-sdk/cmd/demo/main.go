package main

import (
	"fmt"

	"github.com/selectdb/go-doris-sdk"
)

func main() {
	fmt.Println("Doris SDK - Format Interface 演示")

	// ========== 演示 1: 使用 JSONFormat ==========
	fmt.Println("\n=== 演示 1: JSON 格式配置 ===")

	jsonConfig := &doris.Config{
		Endpoints:   []string{"http://10.16.10.6:8630"},
		User:        "root",
		Password:    "password",
		Database:    "test_db",
		Table:       "test_table",
		LabelPrefix: "json_demo",
		// 用户直接构建 JSONFormat
		Format:      &doris.JSONFormat{Type: doris.JSONObjectLine},
		Retry:       doris.DefaultRetry(),
		GroupCommit: doris.ASYNC,
		Options: map[string]string{
			"strict_mode": "true",
		},
	}

	fmt.Printf("JSON Config Format Type: %s\n", jsonConfig.Format.GetFormatType())
	fmt.Printf("JSON Format Options: %+v\n", jsonConfig.Format.GetOptions())

	// ========== 演示 2: 使用 CSVFormat ==========
	fmt.Println("\n=== 演示 2: CSV 格式配置 ===")

	csvConfig := &doris.Config{
		Endpoints:   []string{"http://10.16.10.6:8630"},
		User:        "root",
		Password:    "password",
		Database:    "test_db",
		Table:       "test_table",
		LabelPrefix: "csv_demo",
		// 用户直接构建 CSVFormat
		Format: &doris.CSVFormat{
			ColumnSeparator: ",",
			LineDelimiter:   "\n",
		},
		Retry:       doris.DefaultRetry(),
		GroupCommit: doris.SYNC,
		Options: map[string]string{
			"max_filter_ratio": "0.1",
		},
	}

	fmt.Printf("CSV Config Format Type: %s\n", csvConfig.Format.GetFormatType())
	fmt.Printf("CSV Format Options: %+v\n", csvConfig.Format.GetOptions())

	// ========== 演示 3: 其他 JSON 格式 ==========
	fmt.Println("\n=== 演示 3: JSON Array 格式 ===")

	jsonArrayConfig := &doris.Config{
		Endpoints: []string{"http://10.16.10.6:8630"},
		User:      "root",
		Password:  "password",
		Database:  "test_db",
		Table:     "test_table",
		// 直接构建 JSONFormat - Array 类型
		Format:      &doris.JSONFormat{Type: doris.JSONArray},
		Retry:       &doris.Retry{MaxRetryTimes: 3, BaseIntervalMs: 2000},
		GroupCommit: doris.OFF,
	}

	fmt.Printf("JSON Array Format Type: %s\n", jsonArrayConfig.Format.GetFormatType())
	fmt.Printf("JSON Array Format Options: %+v\n", jsonArrayConfig.Format.GetOptions())

	// ========== 验证配置 ==========
	fmt.Println("\n=== 配置验证 ===")

	configs := []*doris.Config{jsonConfig, csvConfig, jsonArrayConfig}
	configNames := []string{"JSON ObjectLine Config", "CSV Config", "JSON Array Config"}

	for i, config := range configs {
		if err := config.ValidateInternal(); err != nil {
			fmt.Printf("%s validation failed: %v\n", configNames[i], err)
		} else {
			fmt.Printf("%s validation passed!\n", configNames[i])
		}
	}

	fmt.Println("\n演示完成！")
}
