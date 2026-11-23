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

package geoip

import (
	"net"
	"strconv"

	"github.com/oschwald/geoip2-golang"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
	"github.com/alibaba/ilogtail/pkg/util"
)

// ProcessorGeoIP is a processor plugin to insert geographical information into log according
// to IP address specified by SourceKey.
// DBPath and related Language must be set because plugin does not contain any GeoIP database,
// the type of database should be mmdb.
// NoProvince/City/... are used to control the information granularity.
// The keys of geographical information will be prefixed with SourceKey, such as SourceKey_city_.
type ProcessorGeoIP struct {
	NoProvince    bool
	NoCity        bool
	NoCountry     bool
	NoCountryCode bool
	NoCoordinate  bool
	IPValueFlag   bool
	NoKeyError    bool
	NoMatchError  bool
	KeepSource    bool
	DBPath        string
	SourceKey     string
	Language      string

	context  pipeline.Context
	db       *geoip2.Reader
	sourceIP bool
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorGeoIP) Init(context pipeline.Context) error {
	p.context = context
	var err error
	if p.db, err = geoip2.Open(p.DBPath); err != nil {
		return err
	}
	if p.SourceKey == "__source__" {
		p.sourceIP = true
	}
	return nil
}

func (*ProcessorGeoIP) Description() string {
	return "geoip processor for logtail"
}

func inetNtoa(ipValueStr string) net.IP {
	ipnr, _ := strconv.Atoi(ipValueStr)
	var bytes [4]byte
	bytes[0] = byte(ipnr & 0xFF)
	bytes[1] = byte((ipnr >> 8) & 0xFF)
	bytes[2] = byte((ipnr >> 16) & 0xFF)
	bytes[3] = byte((ipnr >> 24) & 0xFF)
	return net.IPv4(bytes[3], bytes[2], bytes[1], bytes[0])
}

// addGeoFields looks up geographical information for an IP address and emits each derived field.
func (p *ProcessorGeoIP) addGeoFields(val string, add func(key, value string)) {
	var ip net.IP
	if p.IPValueFlag {
		ip = inetNtoa(val)
	} else {
		ip = net.ParseIP(val)
	}
	if ip == nil {
		if p.NoMatchError {
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.GeoipAlarm, "invalid ip", val)
		}
		return
	}
	record, err := p.db.City(ip)
	if err != nil && p.NoMatchError {
		logger.Warning(p.context.GetRuntimeContext(), selfmonitor.GeoipAlarm, "parse ip", ip, "error", err)
		return
	}

	if !p.NoCity && len(record.City.Names) > 0 {
		if city, ok := record.City.Names[p.Language]; ok {
			add(p.SourceKey+"_city_", city)
		}
	}

	if !p.NoProvince && len(record.Subdivisions) > 0 && len(record.Subdivisions[0].Names) > 0 {
		if province, ok := record.Subdivisions[0].Names[p.Language]; ok {
			add(p.SourceKey+"_province_", province)
		}
		add(p.SourceKey+"_province_code_", record.Subdivisions[0].IsoCode)
	}

	if !p.NoCountry && len(record.Country.Names) > 0 {
		if country, ok := record.Country.Names[p.Language]; ok {
			add(p.SourceKey+"_country_", country)
		}
		add(p.SourceKey+"_country_code_", record.Country.IsoCode)
	}

	if !p.NoCoordinate {
		add(p.SourceKey+"_longitude_", strconv.FormatFloat(record.Location.Longitude, 'f', 8, 64))
		add(p.SourceKey+"_latitude_", strconv.FormatFloat(record.Location.Latitude, 'f', 8, 64))
	}
}

func init() {
	pipeline.Processors["processor_geoip"] = func() pipeline.Processor {
		return &ProcessorGeoIP{
			KeepSource: true,
			Language:   "zh-CN",
		}
	}
}

// Process enriches logs with geographical information when the GeoIP database is loaded.
func (p *ProcessorGeoIP) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	if p.db == nil {
		pipeline.CollectGroupEvents(context, in)
		return
	}
	pipeline.ProcessLogEventsOnly(in, context, p.processLogEvent)
}

func (p *ProcessorGeoIP) processLogEvent(log *models.Log) {
	contents := log.GetIndices()
	if p.sourceIP {
		ip := util.GetIPAddress()
		p.addGeoFields(ip, func(key, value string) {
			contents.Add(key, value)
		})
		return
	}
	if !contents.Contains(p.SourceKey) {
		if p.NoKeyError {
			logger.Warning(p.context.GetRuntimeContext(), selfmonitor.GeoipAlarm, "cannot find key", p.SourceKey)
		}
		return
	}
	value := pipeline.GetStringValue(contents.Get(p.SourceKey))
	if !p.KeepSource {
		contents.Delete(p.SourceKey)
	}
	p.addGeoFields(value, func(key, val string) {
		contents.Add(key, val)
	})
}
