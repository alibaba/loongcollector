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
const std::string EXPORTED_PREFIX = "exported_";

// prometheus api
const char* const PROMETHEUS_PREFIX = "prometheus_";
const char* const REGISTER_COLLECTOR_PATH = "/register_collector";
const char* const UNREGISTER_COLLECTOR_PATH = "/unregister_collector";
const char* const ACCEPT = "Accept";
const char* const X_PROMETHEUS_REFRESH_INTERVAL_SECONDS = "X-Prometheus-Refresh-Interval-Seconds";
const char* const USER_AGENT = "User-Agent";
const char* const IF_NONE_MATCH = "If-None-Match";
const std::string ETAG = "ETag";
const char* const HTTP = "http";
const char* const APPLICATION_JSON = "application/json";
const char* const HTTPS = "https";
const char* const TARGETS = "targets";
const char* const UNREGISTER_MS = "unRegisterMs";

// scrape config
const char* const SCRAPE_CONFIG = "ScrapeConfig";
const char* const JOB_NAME = "job_name";
const char* const SCHEME = "scheme";
const char* const METRICS_PATH = "metrics_path";
const char* const SCRAPE_INTERVAL = "scrape_interval";
const char* const SCRAPE_TIMEOUT = "scrape_timeout";
const char* const SCRAPE_PROTOCOLS = "scrape_protocols";
const char* const HEADERS = "headers";
const char* const PARAMS = "params";
const char* const QUERY_STRING = "query_string";
const char* const RELABEL_CONFIGS = "relabel_configs";
const char* const SAMPLE_LIMIT = "sample_limit";
const char* const SERIES_LIMIT = "series_limit";
const char* const MAX_SCRAPE_SIZE = "max_scrape_size";
const char* const METRIC_RELABEL_CONFIGS = "metric_relabel_configs";
const char* const AUTHORIZATION = "authorization";
const char* const AUTHORIZATION_DEFAULT_TYEP = "Bearer";
const char* const A_UTHORIZATION = "Authorization";
const char* const TYPE = "type";
const char* const CREDENTIALS = "credentials";
const char* const CREDENTIALS_FILE = "credentials_file";
const char* const BASIC_AUTH = "basic_auth";
const char* const USERNAME = "username";
const char* const USERNAME_FILE = "username_file";
const char* const PASSWORD = "password";
const char* const PASSWORD_FILE = "password_file";
const char* const BASIC_PREFIX = "Basic ";
const char* const HONOR_LABELS = "honor_labels";
const char* const HONOR_TIMESTAMPS = "honor_timestamps";
const char* const FOLLOW_REDIRECTS = "follow_redirects";
const char* const TLS_CONFIG = "tls_config";
const char* const CA_FILE = "ca_file";
const char* const CERT_FILE = "cert_file";
const char* const KEY_FILE = "key_file";
const char* const SERVER_NAME = "server_name";
const char* const HOST = "Host";
const char* const INSECURE_SKIP_VERIFY = "insecure_skip_verify";

// scrape protocols, from https://prometheus.io/docs/prometheus/latest/configuration/configuration/#scrape_config
// text/plain, application/openmetrics-text will be used
// version of openmetrics is 1.0.0 or 0.0.1, from
// https://github.com/OpenObservability/OpenMetrics/blob/main/specification/OpenMetrics.md#extensions-and-improvements
const char* const PrometheusProto = "PrometheusProto";
const char* const PrometheusText0_0_4 = "PrometheusText0.0.4";
const char* const OpenMetricsText0_0_1 = "OpenMetricsText0.0.1";
const char* const OpenMetricsText1_0_0 = "OpenMetricsText1.0.0";

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

// auto metrics
const char* const SCRAPE_DURATION_SECONDS = "scrape_duration_seconds";
const char* const SCRAPE_RESPONSE_SIZE_BYTES = "scrape_response_size_bytes";
const char* const SCRAPE_SAMPLES_LIMIT = "scrape_samples_limit";
const char* const SCRAPE_SAMPLES_POST_METRIC_RELABELING = "scrape_samples_post_metric_relabeling";
const char* const SCRAPE_SAMPLES_SCRAPED = "scrape_samples_scraped";
const char* const SCRAPE_TIMEOUT_SECONDS = "scrape_timeout_seconds";
const char* const UP = "up";

const char* const SCRAPE_TIMESTAMP_MILLISEC = "scrape_timestamp_millisec";

// scrape config compression
const char* const ENABLE_COMPRESSION = "enable_compression";
const char* const ACCEPT_ENCODING = "Accept-Encoding";
const char* const GZIP = "gzip";
const char* const IDENTITY = "identity";

} // namespace logtail::prometheus
