#pragma once

#include <cstdint>
#include <string>

namespace logtail::prometheus {

// magic number for labels hash, from https://github.com/prometheus/common/blob/main/model/fnv.go#L19
const uint64_t PRIME64 = 1099511628211;
const uint64_t OFFSET64 = 14695981039346656037ULL;
const uint64_t RefeshIntervalSeconds = 5;
const char* const META = "__meta_";
const char* const UNDEFINED = "undefined";
const std::string PROMETHEUS = "prometheus";

// relabel config
const char* const SOURCE_LABELS = "source_labels";
const char* const SEPARATOR = "separator";
const char* const TARGET_LABEL = "target_label";
const char* const REGEX = "regex";
const char* const REPLACEMENT = "replacement";
const char* const ACTION = "action";
const char* const MODULUS = "modulus";
const char* const NAME = "__name__";

// prometheus env
const char* const OPERATOR_HOST = "OPERATOR_HOST";
const char* const OPERATOR_PORT = "OPERATOR_PORT";
const char* const POD_NAME = "POD_NAME";

// prometheus api
const char* const PROMETHEUS_PREFIX = "prometheus_";
const char* const REGISTER_COLLECTOR_PATH = "/register_collector";
const char* const UNREGISTER_COLLECTOR_PATH = "/unregister_collector";
const char* const ACCEPT = "Accept";
const char* const X_PROMETHEUS_REFRESH_INTERVAL_SECONDS = "X-Prometheus-Refresh-Interval-Seconds";
const char* const USER_AGENT = "User-Agent";
const char* const IF_NONE_MATCH = "If-None-Match";
const std::string ETAG = "ETag";
const char* const HTTP = "HTTP";
const char* const APPLICATION_JSON = "application/json";
const char* const HTTPS = "HTTPS";
const char* const TARGETS = "targets";
const char* const UNREGISTER_MS = "unregister_ms";

// scrape config
const char* const SCRAPE_CONFIG = "ScrapeConfig";
const char* const JOB_NAME = "job_name";
const char* const SCHEME = "scheme";
const char* const METRICS_PATH = "metrics_path";
const char* const SCRAPE_INTERVAL = "scrape_interval";
const char* const SCRAPE_TIMEOUT = "scrape_timeout";
const char* const HEADERS = "headers";
const char* const PARAMS = "params";
const char* const QUERY_STRING = "query_string";
const char* const RELABEL_CONFIGS = "relabel_configs";
const char* const SAMPLE_LIMIT = "sample_limit";
const char* const SERIES_LIMIT = "series_limit";
const char* const MAX_SCRAPE_SIZE = "max_scrape_size";
const char* const METRIC_RELABEL_CONFIGS = "metric_relabel_configs";
const char* const AUTHORIZATION = "authorization";
const char* const A_UTHORIZATION = "Authorization";
const char* const TYPE = "type";
const char* const CREDENTIALS_FILE = "credentials_file";

// metric labels
const char* const JOB = "job";
const std::string INSTANCE = "instance";
const char* const ADDRESS_LABEL_NAME = "__address__";
const char* const SCRAPE_INTERVAL_LABEL_NAME = "__scrape_interval__";
const char* const SCRAPE_TIMEOUT_LABEL_NAME = "__scrape_timeout__";
const char* const SCHEME_LABEL_NAME = "__scheme__";
const char* const METRICS_PATH_LABEL_NAME = "__metrics_path__";
const char* const PARAM_LABEL_NAME = "__param_";
const char* const LABELS = "labels";

} // namespace logtail::prometheus
