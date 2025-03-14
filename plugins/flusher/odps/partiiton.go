package odps

import (
	"fmt"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

var timeFormatMap = map[string]string{
	"%Y": "2006",
	"%y": "06",
	"%m": "01",
	"%d": "02",
	"%H": "15",
	"%I": "03",
	"%M": "04",
	"%S": "05",
	"%p": "PM",
	"%a": "Mon",
	"%A": "Monday",
	"%b": "Jan",
	"%B": "January",
	"%U": "00",
	"%W": "00",
	"%j": "000",
}

type ColumnType int

const (
	Default ColumnType = iota
	Data
	Time
)

const timeRangeUnit = 15

type FormatColumn struct {
	colType ColumnType
	format  string
}

type PartitionHelper struct {
	timeRange int
	columns   []FormatColumn
}

func NewPartitionHelper() *PartitionHelper {
	return &PartitionHelper{}
}

func (ph *PartitionHelper) Init(config string, timeRange int) error {
	if len(config) == 0 {
		return nil
	}

	if timeRange < timeRangeUnit {
		return fmt.Errorf("timeRange must greater equal %d, current:%d", timeRangeUnit, timeRange)
	}

	if timeRange%timeRangeUnit != 0 {
		return fmt.Errorf("timeRange must be multiple of %d, current:%d", timeRangeUnit, timeRange)
	}

	ph.timeRange = timeRange
	if err := ph.parseColumns(config); err != nil {
		return err
	}
	return nil
}

func FindFormatPos(input string) []int {
	flags := map[rune]struct{}{
		'{': {},
		'}': {},
	}

	positions := make([]int, 0)
	for idx, ch := range input {
		if _, exists := flags[ch]; exists {
			positions = append(positions, idx)
		}
	}

	return positions
}

func validateTimeFormat(format string) (string, error) {
	if len(format)%2 != 0 {
		return "", fmt.Errorf("invalid time format %s", format)
	}

	newFormat := ""
	for i := 0; i < len(format); i += 2 {
		strftime := format[i : i+2]
		gofmt, exists := timeFormatMap[strftime]
		if !exists {
			return "", fmt.Errorf("invalid time format %s", format)
		}

		newFormat += gofmt
	}
	return newFormat, nil
}

func (ph *PartitionHelper) addDefaultColumn(input string, start, end int) {
	if start >= end {
		return
	}

	ph.columns = append(ph.columns, FormatColumn{
		colType: Default,
		format:  input[start:end],
	})
}

func (ph *PartitionHelper) addDataColumn(input string, start, end int) error {
	if start >= end {
		return fmt.Errorf("invalid partition format %s, content of {} cannot be empty", input)
	}

	col := FormatColumn{
		colType: Data,
		format:  input[start:end],
	}
	if strings.HasPrefix(col.format, "%") {
		newFormat, err := validateTimeFormat(col.format)
		if err != nil {
			return err
		}

		col.colType = Time
		col.format = newFormat
	}

	ph.columns = append(ph.columns, col)
	return nil
}

func (ph *PartitionHelper) parseColumns(formatStr string) error {
	ph.columns = make([]FormatColumn, 0)
	positions := FindFormatPos(formatStr)

	if len(positions) == 0 {
		ph.addDefaultColumn(formatStr, 0, len(formatStr))
		return nil
	}

	if len(positions)%4 != 0 {
		return fmt.Errorf("invalid partition format %s", formatStr)
	}

	for i := 0; i < len(positions); i += 4 {
		if formatStr[positions[i]] != '{' || formatStr[positions[i+1]] != '{' ||
			formatStr[positions[i+2]] != '}' || formatStr[positions[i+3]] != '}' {
			return fmt.Errorf("invalid partition format %s", formatStr)
		}

		if positions[i]+1 != positions[i+1] || positions[i+2]+1 != positions[i+3] {
			return fmt.Errorf("invalid partition format %s", formatStr)
		}

		start := 0
		if i > 0 {
			start = positions[i-1] + 1
		}

		ph.addDefaultColumn(formatStr, start, positions[i])

		if err := ph.addDataColumn(formatStr, positions[i+1]+1, positions[i+2]); err != nil {
			return err
		}
	}

	if positions[len(positions)-1] != len(formatStr)-1 {
		ph.addDefaultColumn(formatStr, positions[len(positions)-1]+1, len(formatStr))
	}

	return nil
}

func (ph *PartitionHelper) leftAlign(ts uint32) uint32 {
	remainder := ts % uint32(ph.timeRange*60)
	return ts - remainder
}

func (ph *PartitionHelper) GenPartition(log *protocol.Log) string {
	if len(ph.columns) == 0 {
		return DefaultPartition
	}

	str := ""
	ts := time.Unix(int64(ph.leftAlign(log.Time)), 0)
	for _, col := range ph.columns {
		switch col.colType {
		case Default:
			str += col.format
		case Time:
			str += ts.Format(col.format)
		case Data:
			for _, content := range log.Contents {
				if content.Key == col.format {
					str += content.Value
					break
				}
			}
		}
	}

	if len(str) == 0 {
		return DefaultPartition
	}
	return str
}
