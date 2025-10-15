package examples

import (
	"fmt"
	"strings"

	"github.com/selectdb/go-doris-sdk"
)

// LabelRemovalDemo demonstrates the logging when labels are removed due to group commit
func LabelRemovalDemo() {
	fmt.Println("=== Label Removal Logging Demo ===")

	// 设置日志级别以便看到警告信息
	doris.SetLogLevel(doris.LogLevelInfo)

	// 演示 1: 使用自定义 Label + Group Commit
	fmt.Println("\n--- 演示 1: Custom Label + Group Commit ---")
	configWithLabel := &doris.Config{
		Endpoints:   []string{"http://localhost:8630"},
		User:        "root",
		Password:    "password",
		Database:    "test_db",
		Table:       "test_table",
		Label:       "my_custom_label_123", // 用户指定的自定义 label
		Format:      doris.DefaultJSONFormat(),
		Retry:       doris.DefaultRetry(),
		GroupCommit: doris.ASYNC, // 启用 group commit
	}

	client1, err := doris.NewLoadClient(configWithLabel)
	if err != nil {
		fmt.Printf("Failed to create client: %v\n", err)
		return
	}

	testData := `{"id": 1, "name": "test"}`
	fmt.Println("尝试加载数据，观察 label 删除日志...")
	_, err = client1.Load(strings.NewReader(testData))
	if err != nil {
		fmt.Printf("预期的连接错误（测试环境）: %v\n", err)
	}

	// 演示 2: 使用 LabelPrefix + Group Commit
	fmt.Println("\n--- 演示 2: Label Prefix + Group Commit ---")
	configWithPrefix := &doris.Config{
		Endpoints:   []string{"http://localhost:8630"},
		User:        "root",
		Password:    "password",
		Database:    "test_db",
		Table:       "test_table",
		LabelPrefix: "batch_load", // 用户指定的 label 前缀
		Format:      doris.DefaultCSVFormat(),
		Retry:       doris.DefaultRetry(),
		GroupCommit: doris.SYNC, // 启用 group commit (SYNC 模式)
	}

	client2, err := doris.NewLoadClient(configWithPrefix)
	if err != nil {
		fmt.Printf("Failed to create client: %v\n", err)
		return
	}

	csvData := "1,Alice,30\n2,Bob,25"
	fmt.Println("尝试加载数据，观察 label prefix 删除日志...")
	_, err = client2.Load(strings.NewReader(csvData))
	if err != nil {
		fmt.Printf("预期的连接错误（测试环境）: %v\n", err)
	}

	// 演示 3: 同时使用 Label 和 LabelPrefix + Group Commit
	fmt.Println("\n--- 演示 3: Label + Label Prefix + Group Commit ---")
	configWithBoth := &doris.Config{
		Endpoints:   []string{"http://localhost:8630"},
		User:        "root",
		Password:    "password",
		Database:    "test_db",
		Table:       "test_table",
		Label:       "specific_job_001", // 自定义 label
		LabelPrefix: "production",       // label 前缀
		Format:      doris.DefaultJSONFormat(),
		Retry:       doris.DefaultRetry(),
		GroupCommit: doris.ASYNC, // 启用 group commit
	}

	client3, err := doris.NewLoadClient(configWithBoth)
	if err != nil {
		fmt.Printf("Failed to create client: %v\n", err)
		return
	}

	jsonData := `{"id": 3, "name": "Charlie"}`
	fmt.Println("尝试加载数据，观察两个 label 相关配置的删除日志...")
	_, err = client3.Load(strings.NewReader(jsonData))
	if err != nil {
		fmt.Printf("预期的连接错误（测试环境）: %v\n", err)
	}

	// 演示 4: 不启用 Group Commit 的正常情况
	fmt.Println("\n--- 演示 4: 正常情况 (Group Commit 关闭) ---")
	configNormal := &doris.Config{
		Endpoints:   []string{"http://localhost:8630"},
		User:        "root",
		Password:    "password",
		Database:    "test_db",
		Table:       "test_table",
		Label:       "normal_label_456",
		LabelPrefix: "normal_prefix",
		Format:      doris.DefaultJSONFormat(),
		Retry:       doris.DefaultRetry(),
		GroupCommit: doris.OFF, // 关闭 group commit
	}

	client4, err := doris.NewLoadClient(configNormal)
	if err != nil {
		fmt.Printf("Failed to create client: %v\n", err)
		return
	}

	fmt.Println("尝试加载数据，观察正常的 label 生成日志...")
	_, err = client4.Load(strings.NewReader(testData))
	if err != nil {
		fmt.Printf("预期的连接错误（测试环境）: %v\n", err)
	}

	fmt.Println("\n=== Demo 完成 ===")
	fmt.Println("💡 注意: 以上演示了在启用 group commit 时的 label 删除日志功能")
	fmt.Println("📋 日志级别说明:")
	fmt.Println("   - WARN: 用户配置的 label/label_prefix 被删除的警告")
	fmt.Println("   - INFO: Group commit 启用时的合规性删除操作")
	fmt.Println("   - DEBUG: 正常的 label 生成过程")
}
