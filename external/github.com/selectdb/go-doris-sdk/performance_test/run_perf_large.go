package main

import (
	"fmt"
	"io"
	"sync"
	"sync/atomic"
	"time"

	"github.com/selectdb/go-doris-sdk"
)

func main() {
	fmt.Printf("🎯 ==================== SDK性能极限测试 (固定数据量) ====================\n")
	fmt.Printf("📊 测试目标: 固定1亿条数据，测试不同并发级别的完成时间和吞吐量\n")
	fmt.Printf("🔬 重点指标: 总完成时间、每秒写入条数、每秒写入 MB\n\n")

	// 测试参数
	totalRecords := int64(100_000_000) // 1亿条数据
	batchSize := 50000                 // 每批5万条，需要2000批次
	concurrencies := []int{1, 4, 8, 12}

	// 计算基本信息
	totalBatches := (totalRecords + int64(batchSize) - 1) / int64(batchSize)

	// Doris配置
	config := &doris.Config{
		Endpoints:   []string{"http://10.16.10.6:8630"},
		User:        "root",
		Password:    "123456",
		Database:    "test",
		Table:       "orders",
		Format:      doris.DefaultCSVFormat(),
		Retry:       &doris.Retry{MaxRetryTimes: 2, BaseIntervalMs: 200, MaxTotalTimeMs: 10000},
		GroupCommit: doris.ASYNC,
		Options: map[string]string{
			"timeout": "60",
		},
	}

	client, err := doris.NewLoadClient(config)
	if err != nil {
		fmt.Printf("❌ 创建客户端失败: %v\n", err)
		return
	}

	// 预生成测试数据（所有批次使用相同数据，确保测试一致性）
	fmt.Printf("🔧 预生成测试数据 (%s 条)...\n", formatNumber(int64(batchSize)))
	testData := generateTestData(0, 0, batchSize) // 使用固定参数生成标准数据
	dataSize := int64(len(testData))

	// 基于真实数据计算总数据大小
	singleRecordSize := float64(dataSize) / float64(batchSize)                     // 单条记录的真实大小
	totalDataSize := float64(totalRecords) * singleRecordSize / 1024 / 1024 / 1024 // 总数据大小(GB)

	fmt.Printf("✅ 数据生成完成，单批数据大小: %.2f MB\n", float64(dataSize)/1024/1024)
	fmt.Printf("📏 单条记录实际大小: %.1f 字节\n", singleRecordSize)

	fmt.Printf("\n📋 测试配置:\n")
	fmt.Printf("   总数据量: %s 条记录\n", formatNumber(totalRecords))
	fmt.Printf("   批次大小: %s 条/批\n", formatNumber(int64(batchSize)))
	fmt.Printf("   总批次数: %s 批\n", formatNumber(totalBatches))
	fmt.Printf("   并发级别: %v\n", concurrencies)
	fmt.Printf("   预计数据大小: %.3f GB\n", totalDataSize)

	// 存储结果
	results := make([]TestResult, 0, len(concurrencies))

	// 执行测试
	for _, concurrency := range concurrencies {
		fmt.Printf("🚀 ==================== 并发级别: %d ====================\n", concurrency)
		fmt.Printf("⏰ 开始时间: %s\n", time.Now().Format("15:04:05"))

		result := runFixedVolumeTest(client, concurrency, batchSize, totalRecords, testData, dataSize)
		results = append(results, result)

		printResult(result)
		fmt.Printf("⏰ 完成时间: %s\n\n", time.Now().Format("15:04:05"))

		// 休息10秒再进行下一轮测试
		if concurrency < concurrencies[len(concurrencies)-1] {
			fmt.Printf("😴 休息10秒后进行下一轮测试...\n\n")
			time.Sleep(10 * time.Second)
		}
	}

	// 分析结果
	analyzeResults(results)
}

type TestResult struct {
	Concurrency      int
	BatchSize        int
	TotalRecords     int64
	TotalBytes       int64
	TotalBatches     int64
	SuccessBatches   int64
	FailedBatches    int64
	TotalDuration    time.Duration
	RecordsPerSecond float64
	MBPerSecond      float64
	BatchesPerSecond float64
	AvgBatchDuration time.Duration
	SuccessRate      float64
}

func runFixedVolumeTest(client *doris.DorisLoadClient, concurrency, batchSize int, totalRecords int64, testData string, dataSize int64) TestResult {
	var processedRecords, totalBytes, completedBatches, failedBatches int64
	var totalDuration int64

	startTime := time.Now()

	// 计算需要的批次数
	totalBatches := totalRecords / int64(batchSize)
	if totalRecords%int64(batchSize) != 0 {
		totalBatches++
	}

	fmt.Printf("📦 需要处理 %d 个批次，每批 %d 条记录\n", totalBatches, batchSize)

	// 使用channel分发任务
	batchChan := make(chan int64, totalBatches)
	for i := int64(0); i < totalBatches; i++ {
		batchChan <- i
	}
	close(batchChan)

	var wg sync.WaitGroup

	// 启动worker
	for i := 0; i < concurrency; i++ {
		wg.Add(1)
		go func(workerID int) {
			defer wg.Done()

			for batchID := range batchChan {
				// 计算这个批次的实际记录数
				currentBatchSize := batchSize
				var currentData string
				var currentDataSize int64

				if batchID == totalBatches-1 && totalRecords%int64(batchSize) != 0 {
					// 最后一个批次如果不足5万条，需要生成对应数量的数据
					currentBatchSize = int(totalRecords % int64(batchSize))
					currentData = generateTestData(0, 0, currentBatchSize)
					currentDataSize = int64(len(currentData))
				} else {
					// 使用预生成的标准数据
					currentData = testData
					currentDataSize = dataSize
				}

				// 检查 StringReader 是否支持 Seeking（仅首次检查）
				reader := doris.StringReader(currentData)
				if batchID == 0 && workerID == 0 {
					if _, ok := reader.(io.Seeker); ok {
						fmt.Printf("   ✅ StringReader 支持 Seeking，无需额外缓冲\n")
					} else {
						fmt.Printf("   ❌ StringReader 不支持 Seeking，SDK会缓冲 %.1fMB 数据！\n", float64(len(currentData))/1024/1024)
					}
				}

				// 执行加载
				batchStart := time.Now()
				response, err := client.Load(reader)
				batchDuration := time.Since(batchStart)

				// 更新统计
				atomic.AddInt64(&totalBytes, currentDataSize)
				atomic.AddInt64(&totalDuration, int64(batchDuration))

				if err != nil || response == nil || response.Status != doris.SUCCESS {
					atomic.AddInt64(&failedBatches, 1)
					fmt.Printf("   ❌ Worker %d 批次 %d 失败: %v\n", workerID, batchID, err)
				} else {
					atomic.AddInt64(&completedBatches, 1)
					atomic.AddInt64(&processedRecords, int64(currentBatchSize))
				}

				// 进度显示（每100批次显示一次）
				if atomic.LoadInt64(&completedBatches)%100 == 0 {
					progress := float64(atomic.LoadInt64(&completedBatches)) / float64(totalBatches) * 100
					fmt.Printf("   📈 进度: %.1f%% (%d/%d 批次完成)\n",
						progress, atomic.LoadInt64(&completedBatches), totalBatches)
				}
			}
		}(i)
	}

	wg.Wait()
	actualDuration := time.Since(startTime)

	// 构建结果
	result := TestResult{
		Concurrency:    concurrency,
		BatchSize:      batchSize,
		TotalRecords:   processedRecords,
		TotalBytes:     totalBytes,
		TotalBatches:   completedBatches + failedBatches,
		SuccessBatches: completedBatches,
		FailedBatches:  failedBatches,
		TotalDuration:  actualDuration,
	}

	// 计算性能指标
	seconds := actualDuration.Seconds()
	if seconds > 0 {
		result.RecordsPerSecond = float64(processedRecords) / seconds
		result.MBPerSecond = float64(totalBytes) / 1024 / 1024 / seconds
		result.BatchesPerSecond = float64(completedBatches) / seconds
	}

	if completedBatches > 0 {
		result.AvgBatchDuration = time.Duration(totalDuration / completedBatches)
	}

	if result.TotalBatches > 0 {
		result.SuccessRate = float64(completedBatches) / float64(result.TotalBatches) * 100
	}

	return result
}

func printResult(result TestResult) {
	fmt.Printf("📊 ==================== 测试结果 (并发: %d) ====================\n", result.Concurrency)

	// 数据量信息
	fmt.Printf("📦 数据处理:\n")
	fmt.Printf("   📊 处理记录: %s 条\n", formatNumber(result.TotalRecords))
	fmt.Printf("   💾 数据总量: %.3f GB\n", float64(result.TotalBytes)/1024/1024/1024)
	fmt.Printf("   📦 成功批次: %d/%d 批 (成功率: %.2f%%)\n",
		result.SuccessBatches, result.TotalBatches, result.SuccessRate)

	// 时间信息
	fmt.Printf("⏱️  时间消耗:\n")
	fmt.Printf("   🕐 总耗时: %v\n", result.TotalDuration.Round(time.Millisecond))
	fmt.Printf("   📦 平均批次耗时: %v\n", result.AvgBatchDuration.Round(time.Millisecond))

	// 吞吐量指标
	fmt.Printf("🚀 吞吐量指标:\n")
	fmt.Printf("   📊 %s 条/秒\n", formatNumber(int64(result.RecordsPerSecond)))
	fmt.Printf("   💿 %.2f MB/秒\n", result.MBPerSecond)
	fmt.Printf("   📦 %.1f 批次/秒\n", result.BatchesPerSecond)

	if result.FailedBatches > 0 {
		fmt.Printf("⚠️  失败信息:\n")
		fmt.Printf("   ❌ 失败批次: %d\n", result.FailedBatches)
	}
	fmt.Printf("========================================================\n")
}

func analyzeResults(results []TestResult) {
	fmt.Printf("\n🎯 ==================== 性能对比分析 ====================\n")

	// 详细对比表格
	fmt.Printf("┌────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐\n")
	fmt.Printf("│ 并发数 │  总耗时  │ 数据量GB │ 记录数/秒 │  MB/秒   │ 成功率   │ 扩展效率 │\n")
	fmt.Printf("├────────┼──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤\n")

	var baselinePerformance float64

	for i, result := range results {
		// 计算扩展效率
		var efficiency float64
		if i == 0 {
			baselinePerformance = result.RecordsPerSecond
			efficiency = 100.0 // 基准为100%
		} else {
			theoreticalPerformance := baselinePerformance * float64(result.Concurrency)
			efficiency = (result.RecordsPerSecond / theoreticalPerformance) * 100
		}

		fmt.Printf("│ %-6d │ %-8v │ %-8.3f │ %-8s │ %-8.2f │ %-8.1f%% │ %-8.1f%% │\n",
			result.Concurrency,
			result.TotalDuration.Round(time.Second),
			float64(result.TotalBytes)/1024/1024/1024,
			formatNumber(int64(result.RecordsPerSecond)),
			result.MBPerSecond,
			result.SuccessRate,
			efficiency)
	}
	fmt.Printf("└────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘\n")

	// 性能提升分析
	fmt.Printf("\n📈 ==================== 性能提升分析 ====================\n")
	fmt.Printf("基准性能 (并发=1):\n")
	fmt.Printf("   📊 %s 条/秒 | %.2f MB/秒 | 耗时: %v\n",
		formatNumber(int64(results[0].RecordsPerSecond)),
		results[0].MBPerSecond,
		results[0].TotalDuration.Round(time.Second))

	fmt.Printf("\n各并发级别对比基准的提升:\n")
	for i, result := range results {
		if i == 0 {
			continue // 跳过基准自己
		}

		recordsSpeedup := result.RecordsPerSecond / results[0].RecordsPerSecond
		mbSpeedup := result.MBPerSecond / results[0].MBPerSecond
		timeReduction := float64(results[0].TotalDuration) / float64(result.TotalDuration)

		fmt.Printf("   并发 %d: 🚀 %.2fx 吞吐量 | ⚡ %.2fx 带宽 | ⏱️  %.2fx 时间缩短\n",
			result.Concurrency, recordsSpeedup, mbSpeedup, timeReduction)
	}

	// 找出最佳性能
	var bestResult TestResult
	var bestThroughput float64

	for _, result := range results {
		if result.RecordsPerSecond > bestThroughput && result.SuccessRate >= 99.0 {
			bestThroughput = result.RecordsPerSecond
			bestResult = result
		}
	}

	fmt.Printf("\n🏆 ==================== 最佳性能配置 ====================\n")
	if bestThroughput > 0 {
		fmt.Printf("推荐配置: 并发 %d\n", bestResult.Concurrency)
		fmt.Printf("   📊 吞吐量: %s 条/秒 (%.2fx 基准性能)\n",
			formatNumber(int64(bestThroughput)),
			bestThroughput/results[0].RecordsPerSecond)
		fmt.Printf("   💿 带宽: %.2f MB/秒\n", bestResult.MBPerSecond)
		fmt.Printf("   ⏱️  耗时: %v (比基准快 %.1fx)\n",
			bestResult.TotalDuration.Round(time.Second),
			float64(results[0].TotalDuration)/float64(bestResult.TotalDuration))
		fmt.Printf("   ✅ 成功率: %.2f%%\n", bestResult.SuccessRate)
	}

	// 性能等级评估
	fmt.Printf("\n🎖️  ==================== 性能等级评估 ====================\n")
	maxThroughput := 0.0
	for _, result := range results {
		if result.RecordsPerSecond > maxThroughput {
			maxThroughput = result.RecordsPerSecond
		}
	}

	if maxThroughput > 500000 {
		fmt.Printf("   🏅 性能等级: 卓越 (>50万条/秒)\n")
	} else if maxThroughput > 200000 {
		fmt.Printf("   🥈 性能等级: 优秀 (>20万条/秒)\n")
	} else if maxThroughput > 100000 {
		fmt.Printf("   🥉 性能等级: 良好 (>10万条/秒)\n")
	} else if maxThroughput > 50000 {
		fmt.Printf("   👍 性能等级: 合格 (>5万条/秒)\n")
	} else {
		fmt.Printf("   ⚠️  性能等级: 需要优化 (<5万条/秒)\n")
	}

	// 扩展性分析
	fmt.Printf("\n📊 ==================== 扩展性分析 ====================\n")
	fmt.Printf("理想扩展 vs 实际扩展:\n")
	for i, result := range results {
		if i == 0 {
			fmt.Printf("   并发 %d: 基准性能\n", result.Concurrency)
			continue
		}

		theoreticalPerformance := results[0].RecordsPerSecond * float64(result.Concurrency)
		efficiency := (result.RecordsPerSecond / theoreticalPerformance) * 100

		if efficiency >= 80 {
			fmt.Printf("   并发 %d: 🟢 优秀扩展 (效率: %.1f%%)\n", result.Concurrency, efficiency)
		} else if efficiency >= 60 {
			fmt.Printf("   并发 %d: 🟡 良好扩展 (效率: %.1f%%)\n", result.Concurrency, efficiency)
		} else {
			fmt.Printf("   并发 %d: 🔴 扩展受限 (效率: %.1f%%)\n", result.Concurrency, efficiency)
		}
	}

	fmt.Printf("\n💡 ==================== 生产环境建议 ====================\n")
	hasRecommendation := false
	for _, result := range results {
		if result.SuccessRate >= 99.5 {
			fmt.Printf("   ✅ 推荐并发 %d: 成功率 %.2f%%, 吞吐量 %s 条/秒\n",
				result.Concurrency, result.SuccessRate, formatNumber(int64(result.RecordsPerSecond)))
			hasRecommendation = true
		}
	}

	if !hasRecommendation {
		fmt.Printf("   ⚠️  所有配置成功率 <99.5%%, 建议检查系统配置\n")
	}

	fmt.Printf("========================================================\n")
}

// generateTestData 生成测试数据
func generateTestData(workerID, batchID, batchSize int) string {
	estimatedSize := batchSize * 120
	data := make([]byte, 0, estimatedSize)

	for i := 0; i < batchSize; i++ {
		orderID := fmt.Sprintf("PERF_W%d_B%d_R%d_%d", workerID, batchID, i, time.Now().UnixNano()%1000000)
		record := fmt.Sprintf("%s,Customer_%d,Product_%d,Electronics,Brand_%d,1,99.99,99.99,Shipped,2024-01-01,Region_%d\n",
			orderID, i%1000, i%100, i%50, i%10)
		data = append(data, record...)
	}
	return string(data)
}

// formatNumber 格式化数字显示
func formatNumber(n int64) string {
	if n >= 1000000 {
		return fmt.Sprintf("%.1fM", float64(n)/1000000)
	} else if n >= 1000 {
		return fmt.Sprintf("%.1fK", float64(n)/1000)
	}
	return fmt.Sprintf("%d", n)
}
