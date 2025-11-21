// Package examples demonstrates production-level JSON data loading
// This example shows how to efficiently load large amounts of structured JSON data (50,000 records)
// Best practices: JSON optimization, structured data, memory efficiency
// Uses unified orders schema for consistency across all examples
package examples

import (
	"fmt"
	"time"

	doris "github.com/selectdb/go-doris-sdk"
)

const (
	// Production-level JSON batch size
	JSON_BATCH_SIZE = 50000 // 5万条JSON记录
)

// RunJSONExample demonstrates production-level JSON data loading
func RunJSONExample() {
	fmt.Println("=== Production-Level JSON Data Loading Demo ===")

	doris.SetLogLevel(doris.LogLevelInfo)

	logger := doris.NewContextLogger("JSONDemo")
	logger.Infof("Starting JSON loading demo with %d order records", JSON_BATCH_SIZE)

	// Production-level JSON configuration using direct struct construction
	config := &doris.Config{
		Endpoints:   []string{"http://10.16.10.6:8630"},
		User:        "root",
		Password:    "123456",
		Database:    "test",
		Table:       "orders", // Unified orders table
		LabelPrefix: "prod_json",
		Format:      &doris.JSONFormat{Type: doris.JSONObjectLine}, // JSON Lines format
		Retry:       doris.NewRetry(3, 2000),                       // Custom retry: 3 attempts, 2s base interval
		GroupCommit: doris.ASYNC,                                   // ASYNC mode for better JSON processing
	}

	// Create client with automatic validation
	client, err := doris.NewLoadClient(config)
	if err != nil {
		logger.Errorf("Failed to create load client: %v", err)
		return
	}

	logger.Infof("✅ JSON load client created successfully")

	// Generate realistic JSON order data using unified data generator
	genConfig := DataGeneratorConfig{
		BatchSize:   JSON_BATCH_SIZE,
		ContextName: "JSON-DataGen",
	}
	jsonData := GenerateOrderJSON(genConfig)

	// Perform the JSON load operation
	logger.Infof("Starting JSON load operation for %d order records...", JSON_BATCH_SIZE)
	loadStart := time.Now()

	response, err := client.Load(doris.StringReader(jsonData))

	loadTime := time.Since(loadStart)

	// Simple response handling
	if err != nil {
		fmt.Printf("❌ JSON load failed: %v\n", err)
		return
	}

	if response != nil && response.Status == doris.SUCCESS {
		fmt.Printf("🎉 JSON load completed successfully!\n")
		fmt.Printf("📊 JSON Records: %d, Size: %.1f MB, Time: %v\n", JSON_BATCH_SIZE, float64(len(jsonData))/1024/1024, loadTime)
		fmt.Printf("📈 JSON Rate: %.0f records/sec, %.1f MB/sec\n", float64(JSON_BATCH_SIZE)/loadTime.Seconds(), float64(len(jsonData))/1024/1024/loadTime.Seconds())
		if response.Resp.Label != "" {
			fmt.Printf("📋 Label: %s, Loaded: %d rows\n", response.Resp.Label, response.Resp.NumberLoadedRows)
			if response.Resp.LoadBytes > 0 {
				avgBytesPerRecord := float64(response.Resp.LoadBytes) / float64(response.Resp.NumberLoadedRows)
				fmt.Printf("📏 Average bytes per JSON record: %.1f\n", avgBytesPerRecord)
			}
		}
	} else {
		fmt.Printf("❌ JSON load failed with status: %v\n", response.Status)
	}

	fmt.Println("=== JSON Demo Complete ===")
}
