// Package dogstatsd parses DogStatsD metrics string.
// See http://docs.datadoghq.com/guides/dogstatsd/
package dogstatsd

import (
	"fmt"
	"strconv"
	"strings"
)

// MetricType represents type of the metric.
//
// DogStatsD format assumes the following types:
// 	"c"  - Counter
//	"g"  - Gauge
// 	"h"  - Histogram
// 	"ms" - Timer
// 	"s"  - Set
type MetricType string

const (
	Counter   MetricType = "c"
	Gauge     MetricType = "g"
	Histogram MetricType = "h"
	Meter     MetricType = "m"
	Set       MetricType = "s"
	Timer     MetricType = "ms"
)

// Metric represents a single parsed metric.
type Metric struct {
	Type  MetricType
	Name  string
	Value interface{}
	Rate  float32
	Tags  map[string]string
}

// Parse parses string using (Dog)StatsD metrics format and returns the metric value.
// DogStatsD format looks like:
// 	<name>:<value>|<metric_type>|@<sample_rate>|#<tag1_name>:<tag1_value>,<tag2_name>:<tag2_value>...
// It returns parsed metric and an parse error if encountered.
func Parse(rawmetric string) (m *Metric, err error) {
	d := strings.SplitN(rawmetric, ":", 2)
	if len(d) != 2 {
		return nil, fmt.Errorf("Unparseable metric %s", rawmetric)
	}
	v := strings.Split(d[1], "|")
	if len(v) < 2 {
		return nil, fmt.Errorf("Unparseable metric %s", rawmetric)
	}
	if m, err = parse(d[0], v[0], v[1], v[2:]); err != nil {
		return nil, err;
	}
	return m, nil
}

func parse(name, rawval, rawtype string, tail []string) (m *Metric, err error) {
	m = &Metric{
		Name: name,
		Type: MetricType(rawtype),
		Rate: float32(1),
	}

	var val interface{}

	switch m.Type {
	case Set:
		val = string(rawval)

	case Counter:
		if val, err = strconv.ParseFloat(rawval, 64) ; err != nil {
			return nil, err
		}
		val = int64(val.(float64))

	case Meter:
		if v, err := strconv.ParseFloat(rawval, 64) ; err != nil {
			return nil, err
		} else if v < 0 {
			return nil, fmt.Errorf("Meter value can not be less than 0")
		} else {
			val = int64(v)
		}

	case Gauge, Histogram:
		if val, err = strconv.ParseFloat(rawval, 64); err != nil {
			return nil, err
		}

	case Timer:
		if v, err := strconv.ParseFloat(rawval, 64); err != nil {
			return nil, err
		} else if v < 0 {
			return nil, fmt.Errorf("Timer value can not be less than 0")
		} else {
			val = v
		}

	default:
		return nil, fmt.Errorf("Unknown metric type %q", m.Type)
	}

	m.Value = val

	for _, meta := range tail {
		if meta[0] == '@' {
			if err = parseSampling(m, string(meta[1:])); err != nil {
				return nil, err
			}
		} else if meta[0] == '#' {
			parseTags(m, meta[1:])
		}
	}
	return
}

func parseSampling(m *Metric, s string) error {
	f64, err := strconv.ParseFloat(s, 32);
	if err != nil {
		return err
	}
	if f64 < 0 || f64 > 1 {
		return fmt.Errorf("Sampling must be more than 0 and less than 1")
	}
	m.Rate = float32(f64)
	return nil
}

func parseTags(m *Metric, t string) {
	m.Tags = make(map[string]string)
	rawtags := strings.Split(t, ",")
	for _, key := range rawtags {
		value := ""
		if i := strings.IndexRune(key, ':'); i >= 0 {
			key, value = key[:i], key[i + 1:]
		}
		if key == "" {
			continue
		}
		m.Tags[key] = value
	}
}
