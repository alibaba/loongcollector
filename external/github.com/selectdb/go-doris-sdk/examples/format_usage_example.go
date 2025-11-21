package examples

import (
	"fmt"

	"github.com/selectdb/go-doris-sdk"
)

// FormatUsageExample demonstrates how to use Format interface
func FormatUsageExample() {
	fmt.Println("=== Format Interface 使用示例 ===")

	// 方法1: 直接构建 JSONFormat（推荐）
	jsonConfig := &doris.Config{
		Endpoints: []string{"http://localhost:8630"},
		User:      "root",
		Password:  "password",
		Database:  "example_db",
		Table:     "example_table",
		// 直接构建 JSONFormat 结构体
		Format: &doris.JSONFormat{
			Type: doris.JSONObjectLine, // 或 doris.JSONArray
		},
		Retry: &doris.Retry{
			MaxRetryTimes:  3,
			BaseIntervalMs: 1000,
			MaxTotalTimeMs: 60000, // 添加总时长限制
		},
		GroupCommit: doris.ASYNC,
	}

	// 方法2: 直接构建 CSVFormat（推荐）
	csvConfig := &doris.Config{
		Endpoints: []string{"http://localhost:8630"},
		User:      "root",
		Password:  "password",
		Database:  "example_db",
		Table:     "example_table",
		// 直接构建 CSVFormat 结构体
		Format: &doris.CSVFormat{
			ColumnSeparator: ",",
			LineDelimiter:   "\n",
		},
		Retry: &doris.Retry{
			MaxRetryTimes:  5,
			BaseIntervalMs: 2000,
			MaxTotalTimeMs: 60000, // 添加总时长限制
		},
		GroupCommit: doris.SYNC,
	}

	// 方法3: 自定义格式配置
	customConfig := &doris.Config{
		Endpoints: []string{"http://localhost:8630"},
		User:      "root",
		Password:  "password",
		Database:  "example_db",
		Table:     "example_table",
		// 自定义 CSV 分隔符
		Format: &doris.CSVFormat{
			ColumnSeparator: "|",   // 管道符分隔
			LineDelimiter:   "\\n", // 自定义换行符
		},
		Retry:       doris.DefaultRetry(),
		GroupCommit: doris.OFF,
	}

	// 演示 Format interface 的使用
	configs := []*doris.Config{jsonConfig, csvConfig, customConfig}
	configNames := []string{"JSON Config", "CSV Config", "Custom CSV Config"}

	for i, config := range configs {
		fmt.Printf("\n--- %s ---\n", configNames[i])
		fmt.Printf("Format Type: %s\n", config.Format.GetFormatType())
		fmt.Printf("Format Options: %v\n", config.Format.GetOptions())

		// 验证配置
		if err := config.ValidateInternal(); err != nil {
			fmt.Printf("Validation Error: %v\n", err)
			continue
		}

		// 创建客户端
		client, err := doris.NewLoadClient(config)
		if err != nil {
			fmt.Printf("Client Creation Error: %v\n", err)
			continue
		}

		fmt.Printf("Client created successfully for %s\n", config.Format.GetFormatType())

		// 模拟数据加载
		var sampleData string
		if config.Format.GetFormatType() == "json" {
			sampleData = `{"id": 1, "name": "Alice"}
{"id": 2, "name": "Bob"}`
		} else {
			sampleData = `1,Alice
2,Bob`
		}

		fmt.Printf("Sample data for %s format:\n%s\n", config.Format.GetFormatType(), sampleData)

		// 注意：这里只是演示，实际使用时需要真实的 Doris 服务器
		_ = client
		_ = sampleData
	}

	fmt.Println("\n=== 示例完成 ===")
}
