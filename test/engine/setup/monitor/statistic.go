package monitor

import (
	"encoding/json"
	"fmt"
	"sort"

	v1 "github.com/google/cadvisor/info/v1"
)

type Info struct {
	values []float64
}

func (s *Info) Add(val float64) {
	s.values = append(s.values, val)
}

func (s *Info) CalculateMax() float64 {
	maxVal := 0.0
	for _, val := range s.values {
		if val > maxVal {
			maxVal = val
		}
	}
	return maxVal
}

func (s *Info) CalculateAvg() float64 {
	valuesCopy := make([]float64, len(s.values))
	copy(valuesCopy, s.values)

	// Step 1: Sort the valuesCopy
	sort.Float64s(valuesCopy)

	// Step 2: Calculate Q1 and Q3
	Q1 := valuesCopy[len(valuesCopy)/4]
	Q3 := valuesCopy[3*len(valuesCopy)/4]

	// Step 3: Calculate IQR
	IQR := Q3 - Q1

	// Step 4: Determine the lower and upper bounds for outliers
	lowerBound := Q1 - 1.5*IQR
	upperBound := Q3 + 1.5*IQR

	// Step 5: Filter out the outliers
	cnt := 0
	avg := 0.0
	for _, value := range valuesCopy {
		if value >= lowerBound && value <= upperBound {
			if cnt == 0 {
				avg = value
			} else {
				avg = float64(cnt)/float64(cnt+1)*avg + value/float64(cnt+1)
			}
			cnt++
		}
	}

	return avg
}

type Statistic struct {
	name     string
	cpu      Info
	mem      Info
	lastStat *v1.ContainerStats
}

func NewMonitorStatistic(name string) *Statistic {
	return &Statistic{
		name:     name,
		cpu:      Info{},
		mem:      Info{},
		lastStat: nil,
	}
}

func (m *Statistic) UpdateStatistic(stat *v1.ContainerStats) {
	if m.lastStat != nil && !stat.Timestamp.After(m.lastStat.Timestamp) {
		return
	}
	cpuUsageRateTotal := calculateCPUUsageRate(m.lastStat, stat)
	m.cpu.Add(cpuUsageRateTotal)
	m.mem.Add(float64(stat.Memory.Usage) / 1024 / 1024)
	m.lastStat = stat
	// fmt.Println("CPU Usage Rate(%):", cpuUsageRateTotal, "CPU Usage Rate Max(%):", m.cpu.maxVal, "CPU Usage Rate Avg(%):", m.cpu.avgVal, "Memory Usage Max(MB):", m.mem.maxVal, "Memory Usage Avg(MB):", m.mem.avgVal)
}

func (m *Statistic) ClearStatistic() {
	m.cpu.values = m.cpu.values[:0]
	m.mem.values = m.mem.values[:0]
	m.lastStat = nil
}

func (m *Statistic) GetCPURawData() []float64 {
	return m.cpu.values
}

func calculateCPUUsageRate(lastStat, stat *v1.ContainerStats) float64 {
	if lastStat == nil {
		return 0
	}
	cpuUsageTotal := stat.Cpu.Usage.Total - lastStat.Cpu.Usage.Total
	cpuUsageRateTotal := float64(cpuUsageTotal) * 100 / float64(stat.Timestamp.Sub(lastStat.Timestamp).Nanoseconds())
	return cpuUsageRateTotal
}

type StatisticItem struct {
	Name  string  `json:"name"`
	Value float64 `json:"value"`
	Unit  string  `json:"unit"`
}

func (m *Statistic) MarshalStatisticJSON() ([]byte, error) {
	items := []StatisticItem{
		{"CPU_Usage_Max - " + m.name, m.cpu.CalculateMax(), "%"},
		{"CPU_Usage_Avg - " + m.name, m.cpu.CalculateAvg(), "%"},
		{"Memory_Usage_Max - " + m.name, m.mem.CalculateMax(), "MB"},
		{"Memory_Usage_Avg - " + m.name, m.mem.CalculateAvg(), "MB"},
	}

	// Serialize the slice to JSON
	jsonData, err := json.MarshalIndent(items, "", "    ")
	if err != nil {
		fmt.Println("Error serializing statistics:", err)
		return nil, err
	}

	// Output the JSON string
	return jsonData, nil
}

type RecordsItem struct {
	Name    string    `json:"name"`
	Records []float64 `json:"records"`
	Unit    string    `json:"unit"`
}

func (m *Statistic) MarshalRecordsJSON() ([]byte, error) {
	items := []RecordsItem{
		{"CPU_Usage - " + m.name, m.cpu.values, "%"},
		{"Memory_Usage - " + m.name, m.mem.values, "MB"},
	}

	// Serialize the slice to JSON
	jsonData, err := json.MarshalIndent(items, "", "    ")
	if err != nil {
		fmt.Println("Error serializing statistics:", err)
		return nil, err
	}

	// Output the JSON string
	return jsonData, nil
}
