module github.com/alibaba/ilogtail

go 1.19

require (
	github.com/ClickHouse/clickhouse-go/v2 v2.6.0
	github.com/IBM/sarama v1.42.2
	github.com/VictoriaMetrics/VictoriaMetrics v1.83.0
	github.com/alibaba/ilogtail/pkg v0.0.0
	github.com/apache/pulsar-client-go v0.10.0
	github.com/buger/jsonparser v1.1.1
	github.com/cespare/xxhash/v2 v2.2.0
	github.com/cihub/seelog v0.0.0-20170130134532-f561c5e57575
	github.com/coreos/go-systemd v0.0.0-20190719114852-fd7a80b32e1f
	github.com/danwakefield/fnmatch v0.0.0-20160403171240-cbb64ac3d964
	github.com/denisenkom/go-mssqldb v0.12.2
	github.com/dlclark/regexp2 v1.11.5
	github.com/docker/docker v20.10.23+incompatible
	github.com/eclipse/paho.mqtt.golang v1.3.5
	github.com/elastic/beats/v7 v7.7.1
	github.com/elastic/go-elasticsearch/v8 v8.6.0
	github.com/elastic/go-lumber v0.1.0
	github.com/go-mysql-org/go-mysql v1.8.0
	github.com/go-ping/ping v0.0.0-20211130115550-779d1e919534
	github.com/go-playground/validator/v10 v10.22.0
	github.com/go-sql-driver/mysql v1.7.1
	github.com/gogo/protobuf v1.3.2
	github.com/golang/snappy v0.0.4
	github.com/gosnmp/gosnmp v1.34.0
	github.com/grafana/loki-client-go v0.0.0-20230116142646-e7494d0ef70c
	github.com/hashicorp/golang-lru/v2 v2.0.2
	github.com/influxdata/go-syslog v1.0.1
	github.com/jackc/pgx/v4 v4.16.1
	github.com/jarcoal/httpmock v1.2.0
	github.com/jeromer/syslogparser v0.0.0-20190429161531-5fbaaf06d9e7
	github.com/json-iterator/go v1.1.12
	github.com/knz/strtime v0.0.0-20181018220328-af2256ee352c
	github.com/mailru/easyjson v0.7.7
	github.com/mindprince/gonvml v0.0.0-20180514031326-b364b296c732
	github.com/mitchellh/mapstructure v1.5.0
	github.com/openkruise/kruise-api v1.4.0
	github.com/oschwald/geoip2-golang v1.1.0
	github.com/paulbellamy/ratecounter v0.2.1-0.20170719102518-a803f0e4f071
	github.com/pingcap/check v0.0.0-20200212061837-5e12011dc712
	github.com/pkg/errors v0.9.1
	github.com/prometheus/client_golang v1.14.0
	github.com/prometheus/common v0.42.0
	github.com/prometheus/procfs v0.8.0
	github.com/prometheus/prometheus v1.8.2-0.20210430082741-2a4b8e12bbf2
	github.com/pyroscope-io/pyroscope v1.5.0
	github.com/selectdb/go-doris-sdk v0.0.0-20250608154242-7467dabcf56b
	github.com/shirou/gopsutil v3.21.11+incompatible
	github.com/sirupsen/logrus v1.8.1
	github.com/smartystreets/goconvey v1.7.2
	github.com/streadway/handy v0.0.0-20230327021402-6a47ec586270
	github.com/stretchr/testify v1.8.4
	github.com/syndtr/goleveldb v0.0.0-20170725064836-b89cc31ef797
	github.com/xdg-go/scram v1.1.2
	go.opentelemetry.io/collector/consumer v0.66.0
	go.opentelemetry.io/collector/pdata v1.0.0
	go.opentelemetry.io/proto/otlp v1.0.0
	go.uber.org/atomic v1.11.0
	go.uber.org/multierr v1.11.0
	golang.org/x/sys v0.17.0
	google.golang.org/grpc v1.59.0
	google.golang.org/protobuf v1.31.0
	gopkg.in/yaml.v2 v2.4.0
	gopkg.in/yaml.v3 v3.0.1
	gotest.tools v2.2.0+incompatible
	k8s.io/api v0.26.1
	k8s.io/apimachinery v0.26.1
	k8s.io/client-go v0.26.1
)

require (
	cloud.google.com/go/compute v1.23.0 // indirect
	cloud.google.com/go/compute/metadata v0.2.3 // indirect
	github.com/99designs/go-keychain v0.0.0-20191008050251-8e49817e8af4 // indirect
	github.com/99designs/keyring v1.2.1 // indirect
	github.com/AthenZ/athenz v1.10.39 // indirect
	github.com/BurntSushi/toml v1.3.2 // indirect
	github.com/ClickHouse/ch-go v0.51.2 // indirect
	github.com/DataDog/zstd v1.5.0 // indirect
	github.com/Masterminds/semver v1.5.0 // indirect
	github.com/Microsoft/go-winio v0.5.2 // indirect
	github.com/Microsoft/hcsshim v0.9.4 // indirect
	github.com/VictoriaMetrics/fasthttp v1.1.0 // indirect
	github.com/VictoriaMetrics/metrics v1.23.0 // indirect
	github.com/VictoriaMetrics/metricsql v0.45.0 // indirect
	github.com/andrewkroh/sys v0.0.0-20151128191922-287798fe3e43 // indirect
	github.com/andybalholm/brotli v1.0.4 // indirect
	github.com/ardielle/ardielle-go v1.5.2 // indirect
	github.com/beorn7/perks v1.0.1 // indirect
	github.com/bits-and-blooms/bitset v1.4.0 // indirect
	github.com/blang/semver/v4 v4.0.0 // indirect
	github.com/cespare/xxhash v1.1.0 // indirect
	github.com/cilium/ebpf v0.7.0 // indirect
	github.com/containerd/cgroups v1.0.4 // indirect
	github.com/containerd/containerd v1.6.8 // indirect
	github.com/containerd/continuity v0.2.2 // indirect
	github.com/containerd/fifo v1.0.0 // indirect
	github.com/containerd/go-cni v1.1.6 // indirect
	github.com/containerd/imgcrypt v1.1.4 // indirect
	github.com/containerd/nri v0.1.0 // indirect
	github.com/containerd/ttrpc v1.1.0 // indirect
	github.com/containerd/typeurl v1.0.2 // indirect
	github.com/containernetworking/cni v1.1.1 // indirect
	github.com/containernetworking/plugins v1.1.1 // indirect
	github.com/containers/ocicrypt v1.1.3 // indirect
	github.com/coreos/go-systemd/v22 v22.3.2 // indirect
	github.com/coreos/pkg v0.0.0-20180928190104-399ea9e2e55f // indirect
	github.com/cznic/mathutil v0.0.0-20181122101859-297441e03548 // indirect
	github.com/danieljoos/wincred v1.1.2 // indirect
	github.com/davecgh/go-spew v1.1.1 // indirect
	github.com/dennwc/varint v1.0.0 // indirect
	github.com/docker/distribution v2.8.1+incompatible // indirect
	github.com/docker/go-connections v0.4.0 // indirect
	github.com/docker/go-events v0.0.0-20190806004212-e31b211e4f1c // indirect
	github.com/docker/go-metrics v0.0.1 // indirect
	github.com/docker/go-units v0.5.0 // indirect
	github.com/dvsekhvalnov/jose2go v1.5.0 // indirect
	github.com/eapache/go-resiliency v1.6.0 // indirect
	github.com/eapache/go-xerial-snappy v0.0.0-20230731223053-c322873962e3 // indirect
	github.com/eapache/queue v1.1.0 // indirect
	github.com/elastic/elastic-transport-go/v8 v8.0.0-20211216131617-bbee439d559c // indirect
	github.com/emicklei/go-restful v2.15.0+incompatible // indirect
	github.com/emicklei/go-restful/v3 v3.9.0 // indirect
	github.com/frankban/quicktest v1.14.5 // indirect
	github.com/fsnotify/fsnotify v1.6.0 // indirect
	github.com/gabriel-vasile/mimetype v1.4.3 // indirect
	github.com/go-faster/city v1.0.1 // indirect
	github.com/go-faster/errors v0.6.1 // indirect
	github.com/go-kit/kit v0.12.0 // indirect
	github.com/go-kit/log v0.2.1 // indirect
	github.com/go-logfmt/logfmt v0.5.1 // indirect
	github.com/go-logr/logr v1.2.3 // indirect
	github.com/go-ole/go-ole v1.2.6 // indirect
	github.com/go-openapi/jsonpointer v0.19.5 // indirect
	github.com/go-openapi/jsonreference v0.20.0 // indirect
	github.com/go-openapi/swag v0.21.1 // indirect
	github.com/go-playground/locales v0.14.1 // indirect
	github.com/go-playground/universal-translator v0.18.1 // indirect
	github.com/goccy/go-json v0.10.2 // indirect
	github.com/godbus/dbus v0.0.0-20190726142602-4481cbc300e2 // indirect
	github.com/godbus/dbus/v5 v5.0.6 // indirect
	github.com/gofrs/uuid v4.2.0+incompatible // indirect
	github.com/gogo/googleapis v1.4.0 // indirect
	github.com/golang-jwt/jwt v3.2.1+incompatible // indirect
	github.com/golang-sql/civil v0.0.0-20220223132316-b832511892a9 // indirect
	github.com/golang-sql/sqlexp v0.1.0 // indirect
	github.com/golang/groupcache v0.0.0-20210331224755-41bb18bfe9da // indirect
	github.com/golang/protobuf v1.5.3 // indirect
	github.com/google/gnostic v0.5.7-v3refs // indirect
	github.com/google/go-cmp v0.5.9 // indirect
	github.com/google/gofuzz v1.2.0 // indirect
	github.com/google/uuid v1.4.0 // indirect
	github.com/gopherjs/gopherjs v0.0.0-20200217142428-fce0ec30dd00 // indirect
	github.com/gorilla/websocket v1.4.2 // indirect
	github.com/grafana/regexp v0.0.0-20220304095617-2e8d9baf4ac2 // indirect
	github.com/grpc-ecosystem/grpc-gateway/v2 v2.16.0 // indirect
	github.com/gsterjov/go-libsecret v0.0.0-20161001094733-a6f4afe4910c // indirect
	github.com/hashicorp/errwrap v1.1.0 // indirect
	github.com/hashicorp/go-multierror v1.1.1 // indirect
	github.com/hashicorp/go-uuid v1.0.3 // indirect
	github.com/imdario/mergo v0.3.15 // indirect
	github.com/influxdata/influxdb v1.11.0 // indirect
	github.com/influxdata/line-protocol/v2 v2.2.1 // indirect
	github.com/influxdata/telegraf v1.20.0 // indirect
	github.com/intel/goresctrl v0.2.0 // indirect
	github.com/jackc/chunkreader/v2 v2.0.1 // indirect
	github.com/jackc/pgconn v1.12.1 // indirect
	github.com/jackc/pgio v1.0.0 // indirect
	github.com/jackc/pgpassfile v1.0.0 // indirect
	github.com/jackc/pgproto3/v2 v2.3.0 // indirect
	github.com/jackc/pgservicefile v0.0.0-20200714003250-2b9c44734f2b // indirect
	github.com/jackc/pgtype v1.11.0 // indirect
	github.com/jcmturner/aescts/v2 v2.0.0 // indirect
	github.com/jcmturner/dnsutils/v2 v2.0.0 // indirect
	github.com/jcmturner/gofork v1.7.6 // indirect
	github.com/jcmturner/gokrb5/v8 v8.4.4 // indirect
	github.com/jcmturner/rpc/v2 v2.0.3 // indirect
	github.com/joeshaw/multierror v0.0.0-20140124173710-69b34d4ec901 // indirect
	github.com/josharian/intern v1.0.0 // indirect
	github.com/jpillora/backoff v1.0.0 // indirect
	github.com/jtolds/gls v4.20.0+incompatible // indirect
	github.com/klauspost/compress v1.17.7 // indirect
	github.com/leodido/go-urn v1.4.0 // indirect
	github.com/linkedin/goavro/v2 v2.9.8 // indirect
	github.com/matttproud/golang_protobuf_extensions v1.0.4 // indirect
	github.com/miekg/pkcs11 v1.1.1 // indirect
	github.com/moby/locker v1.0.1 // indirect
	github.com/moby/spdystream v0.2.0 // indirect
	github.com/moby/sys/mountinfo v0.6.2 // indirect
	github.com/moby/sys/signal v0.6.0 // indirect
	github.com/moby/sys/symlink v0.2.0 // indirect
	github.com/modern-go/concurrent v0.0.0-20180306012644-bacd9c7ef1dd // indirect
	github.com/modern-go/reflect2 v1.0.2 // indirect
	github.com/mtibben/percent v0.2.1 // indirect
	github.com/munnerz/goautoneg v0.0.0-20191010083416-a7dc8b61c822 // indirect
	github.com/mwitkow/go-conntrack v0.0.0-20190716064945-2f068394615f // indirect
	github.com/narqo/go-dogstatsd-parser v0.2.0 // indirect
	github.com/opencontainers/go-digest v1.0.0 // indirect
	github.com/opencontainers/image-spec v1.0.3-0.20211202183452-c5a74bcca799 // indirect
	github.com/opencontainers/runc v1.1.12 // indirect
	github.com/opencontainers/runtime-spec v1.0.3-0.20210326190908-1c3f411f0417 // indirect
	github.com/opencontainers/selinux v1.10.1 // indirect
	github.com/oschwald/maxminddb-golang v1.2.1 // indirect
	github.com/paulmach/orb v0.8.0 // indirect
	github.com/pelletier/go-toml v1.9.3 // indirect
	github.com/pierrec/lz4 v2.6.1+incompatible // indirect
	github.com/pierrec/lz4/v4 v4.1.21 // indirect
	github.com/pingcap/errors v0.11.5-0.20221009092201-b66cddb77c32 // indirect
	github.com/pingcap/failpoint v0.0.0-20220801062533-2eaa32854a6c // indirect
	github.com/pingcap/log v1.1.1-0.20230317032135-a0d097d16e22 // indirect
	github.com/pingcap/tidb/pkg/parser v0.0.0-20231103042308-035ad5ccbe67 // indirect
	github.com/pmezard/go-difflib v1.0.0 // indirect
	github.com/prometheus/client_model v0.3.0 // indirect
	github.com/pyroscope-io/jfr-parser v0.6.0 // indirect
	github.com/rcrowley/go-metrics v0.0.0-20201227073835-cf1acfcdf475 // indirect
	github.com/remyoudompheng/bigfft v0.0.0-20230129092748-24d4a6f8daec // indirect
	github.com/richardartoul/molecule v1.0.0 // indirect
	github.com/segmentio/asm v1.2.0 // indirect
	github.com/shopspring/decimal v1.3.1 // indirect
	github.com/siddontang/go v0.0.0-20180604090527-bdc77568d726 // indirect
	github.com/siddontang/go-log v0.0.0-20180807004314-8d05993dda07 // indirect
	github.com/smartystreets/assertions v1.2.0 // indirect
	github.com/spaolacci/murmur3 v1.1.0 // indirect
	github.com/spf13/pflag v1.0.5 // indirect
	github.com/stefanberger/go-pkcs11uri v0.0.0-20201008174630-78d3cae3a980 // indirect
	github.com/tchap/go-patricia v2.3.0+incompatible // indirect
	github.com/tklauser/go-sysconf v0.3.11 // indirect
	github.com/tklauser/numcpus v0.6.0 // indirect
	github.com/valyala/bytebufferpool v1.0.0 // indirect
	github.com/valyala/fastjson v1.6.3 // indirect
	github.com/valyala/fastrand v1.1.0 // indirect
	github.com/valyala/fasttemplate v1.2.2 // indirect
	github.com/valyala/gozstd v1.17.0 // indirect
	github.com/valyala/histogram v1.2.0 // indirect
	github.com/valyala/quicktemplate v1.7.0 // indirect
	github.com/vishvananda/netlink v1.1.1-0.20210330154013-f5de75959ad5 // indirect
	github.com/vishvananda/netns v0.0.0-20210104183010-2eb08e3e575f // indirect
	github.com/xdg-go/pbkdf2 v1.0.0 // indirect
	github.com/xdg-go/stringprep v1.0.4 // indirect
	github.com/yusufpapurcu/wmi v1.2.2 // indirect
	go.etcd.io/bbolt v1.3.6 // indirect
	go.mozilla.org/pkcs7 v0.0.0-20200128120323-432b2356ecb1 // indirect
	go.opencensus.io v0.24.0 // indirect
	go.opentelemetry.io/otel v1.11.2 // indirect
	go.opentelemetry.io/otel/trace v1.11.2 // indirect
	go.uber.org/goleak v1.2.1 // indirect
	go.uber.org/zap v1.26.0 // indirect
	golang.org/x/crypto v0.19.0 // indirect
	golang.org/x/exp v0.0.0-20231006140011-7918f672742d // indirect
	golang.org/x/mod v0.13.0 // indirect
	golang.org/x/net v0.21.0 // indirect
	golang.org/x/oauth2 v0.11.0 // indirect
	golang.org/x/sync v0.6.0 // indirect
	golang.org/x/term v0.17.0 // indirect
	golang.org/x/text v0.14.0 // indirect
	golang.org/x/time v0.3.0 // indirect
	google.golang.org/appengine v1.6.7 // indirect
	google.golang.org/genproto/googleapis/api v0.0.0-20230822172742-b8732ec3820d // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20230822172742-b8732ec3820d // indirect
	gopkg.in/inf.v0 v0.9.1 // indirect
	gopkg.in/natefinch/lumberjack.v2 v2.2.1 // indirect
	gopkg.in/square/go-jose.v2 v2.5.1 // indirect
	k8s.io/apiserver v0.22.5 // indirect
	k8s.io/component-base v0.24.0 // indirect
	k8s.io/cri-api v0.23.1 // indirect
	k8s.io/klog/v2 v2.80.1 // indirect
	k8s.io/kube-openapi v0.0.0-20221207184640-f3cff1453715 // indirect
	k8s.io/utils v0.0.0-20221128185143-99ec85e7a448 // indirect
	launchpad.net/gocheck v0.0.0-20140225173054-000000000087 // indirect
	sigs.k8s.io/controller-runtime v0.12.1 // indirect
	sigs.k8s.io/gateway-api v0.6.2 // indirect
	sigs.k8s.io/json v0.0.0-20220713155537-f223a00ba0e2 // indirect
	sigs.k8s.io/structured-merge-diff/v4 v4.2.3 // indirect
	sigs.k8s.io/yaml v1.3.0 // indirect
)

replace (
	github.com/VictoriaMetrics/VictoriaMetrics => github.com/iLogtail/VictoriaMetrics v1.83.4-ilogtail
	github.com/VictoriaMetrics/metrics => github.com/iLogtail/metrics v1.23.0-ilogtail
	github.com/alibaba/ilogtail/pkg => ./pkg
	github.com/aliyun/alibaba-cloud-sdk-go/services/sls_inner => ./external/github.com/aliyun/alibaba-cloud-sdk-go/services/sls_inner
	github.com/elastic/beats/v7 => ./external/github.com/elastic/beats/v7
	github.com/jeromer/syslogparser => ./external/github.com/jeromer/syslogparser
	github.com/mindprince/gonvml => github.com/iLogtail/gonvml v1.0.0
	github.com/opencontainers/runc => github.com/opencontainers/runc v1.1.12
	github.com/pingcap/parser => github.com/iLogtail/parser v0.0.0-20210415081931-48e7f467fd74-ilogtail
	github.com/prometheus/prometheus => github.com/prometheus/prometheus v0.35.0
	github.com/pyroscope-io/jfr-parser => github.com/iLogtail/jfr-parser v0.6.0
	github.com/pyroscope-io/pyroscope => github.com/iLogtail/pyroscope-lib v0.35.2-ilogtail
	github.com/satori/go.uuid => github.com/satori/go.uuid v1.2.0
	github.com/selectdb/go-doris-sdk => ./external/github.com/selectdb/go-doris-sdk
	github.com/streadway/handy => github.com/iLogtail/handy v0.0.0-20230327021402-6a47ec586270
)
