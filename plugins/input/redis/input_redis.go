// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package redis

import (
	"bufio"
	"errors"
	"fmt"
	"net"
	"net/url"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type InputRedis struct {
	ServerUrls []string
	context    pipeline.Context
}

// ## specify servers via a url matching:
// ##  [protocol://][:password]@address[:port]
// ##  e.g.
// ##    tcp://localhost:6379
// ##    tcp://:password@192.168.99.100
// ##    unix:///var/run/redis.sock
// ##
// ## If no servers are specified, then localhost is used as the host.
// ## If no port is specified, 6379 is used
// servers = ["tcp://localhost:6379"]

func (r *InputRedis) Init(context pipeline.Context) (int, error) {
	r.context = context
	return 0, nil
}

func (r *InputRedis) Description() string {
	return "redis input plugin for logtail"
}

var Tracking = map[string]string{
	"uptime_in_seconds": "uptime",
	"connected_clients": "clients",
	"role":              "replication_role",
}

type TotalDBInfo struct {
	keys    int64
	avgTTL  int64
	expires int64
	dbCount int64
}

var ErrProtocolError = errors.New("redis protocol error")

const defaultPort = "6379"

func (r *InputRedis) Collect(collector pipeline.Collector) error {

	if len(r.ServerUrls) == 0 {
		return r.gatherServer(&url.URL{
			Scheme: "tcp",
			Host:   ":6379",
		}, collector)
	}

	var wg sync.WaitGroup
	for _, serv := range r.ServerUrls {
		if !strings.HasPrefix(serv, "tcp://") && !strings.HasPrefix(serv, "unix://") {
			serv = "tcp://" + serv
		}

		u, err := url.Parse(serv)
		if err != nil {
			logger.Warning(r.context.GetRuntimeContext(), "REDIS_PARSE_ADDRESS_ALARM", "Unable to parse to address", serv, "error", err)
			continue
		} else if u.Scheme == "" {
			// fallback to simple string based address (i.e. "10.0.0.1:10000")
			u.Scheme = "tcp"
			u.Host = serv
			u.Path = ""
		}
		if u.Scheme == "tcp" {
			_, _, err := net.SplitHostPort(u.Host)
			if err != nil {
				u.Host = u.Host + ":" + defaultPort
			}
		}

		wg.Add(1)
		go func(url *url.URL) {
			defer wg.Done()
			err := r.gatherServer(url, collector)
			if err != nil {
				logger.Error(r.context.GetRuntimeContext(), "REDIS_COLLECT_ALARM", err)
			}
		}(u)
	}

	wg.Wait()
	return nil
}

var defaultTimeout = 5 * time.Second

func (r *InputRedis) gatherServer(addr *url.URL, collector pipeline.Collector) error {
	var address string

	if addr.Scheme == "unix" {
		address = addr.Path
	} else {
		address = addr.Host
	}
	c, err := net.DialTimeout(addr.Scheme, address, defaultTimeout)
	if err != nil {
		return fmt.Errorf("Unable to connect to redis server '%s': %s", address, err)
	}
	defer func(c net.Conn) {
		_ = c.Close()
	}(c)

	// Extend connection
	_ = c.SetDeadline(time.Now().Add(defaultTimeout))

	if addr.User != nil {
		pwd, set := addr.User.Password()
		if set && pwd != "" {
			_, _ = c.Write([]byte(fmt.Sprintf("AUTH %s\r\n", pwd)))

			rdr := bufio.NewReader(c)

			line, err := rdr.ReadString('\n')
			if err != nil {
				return err
			}
			if line[0] != '+' {
				return fmt.Errorf("%s", strings.TrimSpace(line)[1:])
			}
		}
	}

	_, _ = c.Write([]byte("INFO\r\n"))
	_, _ = c.Write([]byte("EOF\r\n"))
	rdr := bufio.NewReader(c)

	var tags map[string]string

	if addr.Scheme == "unix" {
		tags = map[string]string{"socket": addr.Path}
	} else {
		// If there's an error, ignore and use 'unknown' tags
		host, port, err := net.SplitHostPort(addr.Host)
		if err != nil {
			host, port = "unknown", "unknown"
		}
		tags = map[string]string{"server": host, "port": port}
	}
	return gatherInfoOutput(rdr, collector, tags)
}

// gatherInfoOutput gathers
func gatherInfoOutput(
	rdr *bufio.Reader,
	collector pipeline.Collector,
	tags map[string]string,
) error {
	var section string

	var dbTotal TotalDBInfo
	scanner := bufio.NewScanner(rdr)
	fields := make(map[string]string)
	for scanner.Scan() {
		line := scanner.Text()
		if strings.Contains(line, "ERR") {
			break
		}

		if len(line) == 0 {
			continue
		}
		if line[0] == '#' {
			if len(line) > 2 {
				section = line[2:]
			}
			continue
		}

		parts := strings.SplitN(line, ":", 2)
		if len(parts) < 2 {
			continue
		}
		name := parts[0]

		if section == "Server" {
			if name != "lru_clock" && name != "uptime_in_seconds" && name != "redis_version" {
				continue
			}
		}

		if name == "mem_allocator" {
			continue
		}

		metric, ok := Tracking[name]
		if !ok {
			if section == "Keyspace" {
				kline := strings.TrimSpace(parts[1])
				gatherKeyspaceLine(name, kline, &fields, &dbTotal)
				continue
			}
			metric = name
		}

		val := strings.TrimSpace(parts[1])
		fields[metric] = val
	}
	if dbTotal.dbCount > 0 {
		dbTotal.avgTTL /= dbTotal.dbCount
	}
	fields["total_db_count"] = strconv.FormatInt(dbTotal.dbCount, 10)
	fields["total_db_avg_ttl"] = strconv.FormatInt(dbTotal.avgTTL, 10)
	fields["total_db_keys"] = strconv.FormatInt(dbTotal.keys, 10)
	fields["total_db_expires"] = strconv.FormatInt(dbTotal.expires, 10)
	collector.AddData(tags, fields)
	return nil
}

// Parse the special Keyspace line at end of redis stats
// This is a special line that looks something like:
//
//	db0:keys=2,expires=0,avg_ttl=0
//
// And there is one for each db on the redis instance
func gatherKeyspaceLine(name, line string, fields *map[string]string, dbTotal *TotalDBInfo) {
	if strings.Contains(line, "keys=") {
		dbparts := strings.Split(line, ",")
		for _, dbp := range dbparts {
			if kv := strings.Split(dbp, "="); len(kv) >= 2 {
				(*fields)[name+"_"+kv[0]] = kv[1]
				val, _ := strconv.ParseInt(kv[1], 10, 64)
				switch kv[0] {
				case "keys":
					dbTotal.keys += val
				case "expires":
					dbTotal.expires += val
				case "avg_ttl":
					dbTotal.avgTTL += val
				}
			}
		}
		dbTotal.dbCount++
	}
}

func init() {
	pipeline.MetricInputs["metric_redis"] = func() pipeline.MetricInput {
		return &InputRedis{}
	}
}
