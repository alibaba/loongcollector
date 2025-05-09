package helper

import (
	"bufio"
	"context"
	"fmt"
	"os"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
)

const (
	testFilePath = "./test.log"
)

var testFileStat StateOS

func setup() error {
	fmt.Println("Setting up for tests...")

	os.Create(testFilePath)
	fileInfo, err := os.Stat(testFilePath)
	if err != nil {
		return err
	}
	testFileStat = GetOSState(fileInfo)
	return nil
}

func teardown() error {
	fmt.Println("Tearing down after tests...")
	if _, err := os.Stat(testFilePath); os.IsNotExist(err) {
		return nil
	}
	err := os.Remove(testFilePath)
	if err != nil {
		return err
	}
	return nil
}

func getMockCheckpoint() LogFileReaderCheckPoint {
	return LogFileReaderCheckPoint{
		Path:   testFilePath,
		Offset: 0,
		State:  testFileStat,
	}
}

func getMockLogReaderConfig() LogFileReaderConfig {
	return LogFileReaderConfig{
		CloseFileSec:     60,
		MaxReadBlockSize: 1024,
		ReadIntervalMs:   1000,
		Tracker:          nil,
	}
}

func writeContent(filePath string) error {
	file, err := os.OpenFile(filePath, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		return err
	}
	writer := bufio.NewWriter(file)
	_, err = writer.WriteString("hello world" + "\n")
	if err != nil {
		return err
	}
	err = writer.Flush()
	if err != nil {
		return err
	}
	return nil
}

type MockLogFileProcessor struct {
}

func (m *MockLogFileProcessor) Process(fileBlock []byte, noChangeInterval time.Duration) int {
	return 0
}

func getMockLogFileProcessor() *MockLogFileProcessor {
	return &MockLogFileProcessor{}
}

func TestCheckFileChange(t *testing.T) {
	reader, err := NewLogFileReader(context.Background(), getMockCheckpoint(), getMockLogReaderConfig(), getMockLogFileProcessor())
	assert.Nil(t, err)
	change := reader.CheckFileChange()
	assert.Equal(t, false, change)

	// write to file
	writeContent(testFilePath)
	change = reader.CheckFileChange()
	assert.Equal(t, true, change)
	reader.ReadAndProcess(false)

	// write to file and remove, mock the file path in logfilereader unreachable
	writeContent(testFilePath)
	os.Remove(testFilePath)
	change = reader.CheckFileChange()
	assert.Equal(t, true, change)

	reader.CloseFile("finish test")
}

func TestMain(m *testing.M) {
	err := setup()
	if err != nil {
		fmt.Println("Setup failed:", err)
		os.Exit(1)
	}
	exitCode := m.Run()
	err = teardown()
	if err != nil {
		fmt.Println("Teardown failed:", err)
		os.Exit(1)
	}
	os.Exit(exitCode)
}
