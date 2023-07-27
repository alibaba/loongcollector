package pluginmanager

import (
	"context"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/plugin_main/flags"
)

func getExcludePorts() []int {
	ports := []int{}
	// logtail service port
	if *flags.StatefulSetFlag {
		add := *flags.HTTPAddr
		exportLogtailPortsPort, _ := strconv.Atoi(strings.Split(add, ":")[1])
		ports = append(ports, exportLogtailPortsPort)
	}

	// env "HTTP_PROBE_PORT"
	checkAlivePort := os.Getenv("HTTP_PROBE_PORT")
	if len(checkAlivePort) != 0 {
		port, err := strconv.Atoi(checkAlivePort)
		if err == nil {
			ports = append(ports, port)
		}
	}
	return ports
}

func getListenPortsFromFile(pid int, protocol string) ([]int, error) {
	ports := []int{}
	filepath.Join()
	file := fmt.Sprintf("/proc/%d/net/%s", pid, protocol)
	data, err := ioutil.ReadFile(filepath.Clean(file))
	if err != nil {
		return ports, err
	}

	lines := strings.Split(string(data), "\n")
	for _, line := range lines[1:] {
		fields := strings.Fields(line)
		if len(fields) < 10 {
			continue
		}
		/*
			file /proc/{pid}/net/tcp or /proc/{pid}/net/tcp6
			sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
			0: 0100007F:97ED 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 259386485 1 0000000000000000 100 0 0 10 0
			1: 1600580A:8DDE C8E81275:01BB 01 00000000:00000000 00:00000000 00000000     0        0 259384917 1 0000000000000000 393 4 24 10 -1
			2: 0100007F:97ED 0100007F:D30A 01 00000000:00000000 00:00000000 00000000     0        0 259384905 2 0000000000000000 20 4 6 10 -1
			st's value=="0A" means LISTEN
		*/
		if strings.HasPrefix(protocol, "tcp") && fields[3] != "0A" {
			continue
		}
		/*
			file /proc/{pid}/net/udp or /proc/{pid}/net/udp6
			sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode ref pointer drops
			0: 0100007F:0017 0100007F:0035 01 00000000:00000000 00:00000000 00000000  1000        0 17032 1 ffff8a682eed8000 0
			st's value=="07" means LISTEN
		*/
		if strings.HasPrefix(protocol, "udp") && fields[3] != "07" {
			continue
		}
		// local_address is ip:port
		port, err := strconv.ParseUint((strings.Split(fields[1], ":")[1]), 16, 32)
		if err != nil {
			logger.Error(context.Background(), "parse port fail", err.Error())
			continue
		}
		ports = append(ports, int(port))
	}
	return ports, nil
}

func getLogtailLitsenPorts() ([]int, []int) {
	portsTCPMap := map[int]int{}
	portsUDPMap := map[int]int{}
	pid := os.Getpid()
	portsTCP := []int{}
	portsUDP := []int{}
	// get tcp ports
	tcpPorts, err := getListenPortsFromFile(pid, "tcp")
	if err != nil {
		logger.Error(context.Background(), "get tcp port fail", err.Error())
	}
	for _, port := range tcpPorts {
		portsTCPMap[port]++
	}
	// get tcp6 ports
	tcp6Ports, err := getListenPortsFromFile(pid, "tcp6")
	if err != nil {
		logger.Error(context.Background(), "get tcp6 port fail", err.Error())
	}
	for _, port := range tcp6Ports {
		portsTCPMap[port]++
	}
	// get udp ports
	udpPorts, err := getListenPortsFromFile(pid, "udp")
	if err != nil {
		logger.Error(context.Background(), "get udp port fail", err.Error())
	}
	for _, port := range udpPorts {
		portsUDPMap[port]++
	}
	// get udp6 ports
	udp6Ports, err := getListenPortsFromFile(pid, "udp6")
	if err != nil {
		logger.Error(context.Background(), "get udp6 port fail", err.Error())
	}
	for _, port := range udp6Ports {
		portsUDPMap[port]++
	}

	excludePorts := getExcludePorts()
	for _, port := range excludePorts {
		delete(portsTCPMap, port)
	}

	for port := range portsTCPMap {
		portsTCP = append(portsTCP, port)
	}
	for port := range portsUDPMap {
		portsUDP = append(portsUDP, port)
	}
	return portsTCP, portsUDP
}

func FindPort(res http.ResponseWriter, req *http.Request) {
	portsTCP, portsUDP := getLogtailLitsenPorts()
	logger.Info(context.Background(), "get logtail's listen ports tcp", portsTCP, "udp", portsUDP)

	param := &struct {
		PortsTCP []int `json:"ports_tcp"`
		PortsUDP []int `json:"ports_udp"`
	}{}
	param.PortsTCP = portsTCP
	param.PortsUDP = portsUDP
	jsonBytes, err := json.Marshal(param)
	if err != nil {
		res.WriteHeader(http.StatusInternalServerError)
		_, WriteErr := res.Write([]byte(err.Error()))
		if WriteErr != nil {
			logger.Error(context.Background(), "write response err", WriteErr.Error())
		}
		return
	}
	res.WriteHeader(http.StatusOK)
	_, WriteErr := res.Write(jsonBytes)
	if WriteErr != nil {
		logger.Error(context.Background(), "write response err", WriteErr.Error())
	}
}
