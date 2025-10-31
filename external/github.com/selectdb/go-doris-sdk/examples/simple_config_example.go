package examples

import (
	"fmt"
	"strings"

	"github.com/selectdb/go-doris-sdk"
)

func SimpleConfigExample() {
	// 直接构建 Config 结构体，使用新的默认函数
	config := &doris.Config{
		Endpoints:   []string{"http://10.16.10.6:8630"},
		User:        "root",
		Password:    "password",
		Database:    "test_db",
		Table:       "test_table",
		Format:      doris.DefaultJSONFormat(), // 使用新的默认 JSON 格式
		Retry:       doris.DefaultRetry(),      // 使用新的默认重试策略
		GroupCommit: doris.ASYNC,
		Options: map[string]string{
			"strict_mode":      "true",
			"max_filter_ratio": "0.1",
		},
	}

	// 创建客户端
	client, err := doris.NewLoadClient(config)
	if err != nil {
		fmt.Printf("Failed to create client: %v\n", err)
		return
	}

	// 准备数据
	jsonData := `{"id": 1, "name": "Alice", "age": 30}
{"id": 2, "name": "Bob", "age": 25}
{"id": 3, "name": "Charlie", "age": 35}`

	// 执行加载
	response, err := client.Load(strings.NewReader(jsonData))
	if err != nil {
		fmt.Printf("Load failed: %v\n", err)
		return
	}

	fmt.Printf("Load completed successfully!\n")
	fmt.Printf("Status: %s\n", response.Status)
	if response.Status == doris.SUCCESS {
		fmt.Printf("Loaded rows: %d\n", response.Resp.NumberLoadedRows)
		fmt.Printf("Load bytes: %d\n", response.Resp.LoadBytes)
	}
}
