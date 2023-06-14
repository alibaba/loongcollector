package pluginmanager

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/logger"
)

var exportLogtailPortsRunning = false

var exportLogtailPortsInterval = 30 * time.Second

func getListenPortsFromFile(pid int, protocol string) ([]int, error) {
	var ports []int

	file := fmt.Sprintf("/proc/%d/net/%s", pid, protocol)
	data, err := ioutil.ReadFile(file)
	if err != nil {
		return nil, err
	}

	lines := strings.Split(string(data), "\n")
	for _, line := range lines[1:] {
		fields := strings.Fields(line)
		if len(fields) < 10 {
			continue
		}
		if strings.HasPrefix(protocol, "tcp") && fields[3] != "0A" {
			continue
		}
		if strings.HasPrefix(protocol, "udp") && fields[3] != "07" {
			continue
		}
		port, err := strconv.ParseUint((strings.Split(fields[1], ":")[1]), 16, 32)
		if err != nil {
			return nil, err
		}
		ports = append(ports, int(port))
	}
	return ports, nil
}

func getLogtailLitsenPorts() ([]int, error) {
	portsMap := map[int]int{}
	pid := os.Getpid()
	ports := []int{}
	// get tcp ports
	tcpPorts, err := getListenPortsFromFile(pid, "tcp")
	if err != nil {
		return nil, err
	}
	for _, port := range tcpPorts {
		portsMap[port]++
	}
	// get tcp6 ports
	tcp6Ports, err := getListenPortsFromFile(pid, "tcp6")
	if err != nil {
		return nil, err
	}
	for _, port := range tcp6Ports {
		portsMap[port]++
	}
	// get udp ports
	udpPorts, err := getListenPortsFromFile(pid, "udp")
	if err != nil {
		return nil, err
	}
	for _, port := range udpPorts {
		portsMap[port]++
	}
	// get udp6 ports
	udp6Ports, err := getListenPortsFromFile(pid, "udp6")
	if err != nil {
		return nil, err
	}
	for _, port := range udp6Ports {
		portsMap[port]++
	}

	for port := range portsMap {
		ports = append(ports, port)
	}
	return ports, nil
}

func exportLogtailLitsenPorts(ports []int) error {
	param := &struct {
		Status string `json:"status"`
		Ports  []int  `json:"ports"`
	}{}
	param.Status = "success"
	param.Ports = ports
	jsonBytes, err := json.Marshal(param)
	if err != nil {
		return err
	}
	client := &http.Client{}
	req, err := http.NewRequest("POST", *flags.K8sControllerEndpoint, bytes.NewBuffer(jsonBytes))
	if err != nil {
		return err
	}
	res, err := client.Do(req)
	if err != nil {
		return err
	}
	if res.StatusCode != 200 {
		return errors.New(res.Status)
	}
	return nil
}

func ExportLogtailPorts() {
	exportLogtailPorts := func() {
		exportLogtailPortsTicker := time.NewTicker(exportLogtailPortsInterval)
		for range exportLogtailPortsTicker.C {
			ports, err := getLogtailLitsenPorts()
			if err != nil {
				logger.Error(context.Background(), "get logtail's listen ports failed", err.Error())
				continue
			}
			logger.Info(context.Background(), "get logtail's listen ports success", ports)

			err = exportLogtailLitsenPorts(ports)
			if err != nil {
				logger.Error(context.Background(), "export logtail's listen ports failed", err.Error())
				continue
			}
			logger.Info(context.Background(), "export logtail's listen ports success", ports)
		}
	}
	if !exportLogtailPortsRunning {
		go exportLogtailPorts()
		exportLogtailPortsRunning = true
	}
}
