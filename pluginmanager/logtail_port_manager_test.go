package pluginmanager

import (
	"context"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"runtime"
	"testing"
	"time"

	"github.com/stretchr/testify/suite"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/plugin_main/flags"
)

func TestLogtailPortManager(t *testing.T) {
	suite.Run(t, new(logtailPortManagerTestSuite))
}

type logtailPortManagerTestSuite struct {
	suite.Suite
}

func (s *logtailPortManagerTestSuite) BeforeTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test start ========================", suiteName, testName)
}

func (s *logtailPortManagerTestSuite) AfterTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test end =======================", suiteName, testName)
}

func (s *logtailPortManagerTestSuite) TestGetLogtailLitsenPorts() {
	listener1, err := net.Listen("tcp", ":18688")
	if err == nil {
		defer listener1.Close()
	}
	addr, err := net.ResolveUDPAddr("udp", ":18687")
	if err == nil {
		listener2, err := net.ListenUDP("udp", addr)
		if err == nil {
			defer listener2.Close()
		}
	}

	tcp, udp := getLogtailLitsenPorts()
	logger.Info(context.Background(), "get ports success tcp", tcp, "udp", udp)
	if runtime.GOOS == "linux" {
		s.Contains(tcp, 18688)
		s.Contains(udp, 18687)
	}
}

func (s *logtailPortManagerTestSuite) TestExportLogtailLitsenPorts() {
	go func() {
		mux := http.NewServeMux()
		mux.HandleFunc("/export/port", FindPort)
		server := &http.Server{
			Addr:              *flags.HTTPAddr,
			Handler:           mux,
			ReadHeaderTimeout: 10 * time.Second,
		}
		err := server.ListenAndServe()
		defer server.Close()
		if err != nil && err != http.ErrServerClosed {
			logger.Error(context.Background(), "export logtail's ports failed", err.Error())
			return
		}
	}()

	listener1, err := net.Listen("tcp", ":18688")
	if err == nil {
		defer listener1.Close()
	}
	addr, err := net.ResolveUDPAddr("udp", ":18687")
	if err == nil {
		listener2, err := net.ListenUDP("udp", addr)
		if err == nil {
			defer listener2.Close()
		}
	}

	ticker := time.NewTicker(2 * time.Second)
	count := 0
	for range ticker.C {
		count++
		client := &http.Client{}
		req, err := http.NewRequest("GET", fmt.Sprintf("http://127.0.0.1%s/export/port", *flags.HTTPAddr), nil)
		s.NoError(err)
		res, err := client.Do(req)
		s.NoError(err)

		s.Equal(res.StatusCode, http.StatusOK)
		param := &struct {
			PortsTCP []int `json:"ports_tcp"`
			PortsUDP []int `json:"ports_udp"`
		}{}
		err = json.NewDecoder(res.Body).Decode(param)
		s.NoError(err)
		logger.Info(context.Background(), "receive ports success, tcp", param.PortsTCP, "udp", param.PortsUDP)
		if runtime.GOOS == "linux" {
			s.Contains(param.PortsTCP, 18688)
			s.Contains(param.PortsUDP, 18687)
		}
		if count == 3 {
			return
		}
	}
}
