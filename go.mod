module github.com/alibaba/ilogtail

go 1.18

require (
	github.com/Shopify/sarama v1.28.0
	github.com/VictoriaMetrics/VictoriaMetrics v1.58.0
	github.com/VictoriaMetrics/metrics v1.17.2
	github.com/alibaba/ilogtail/pkg v0.0.0
	github.com/aliyun/alibaba-cloud-sdk-go/services/sls_inner v0.0.0
	github.com/aliyun/aliyun-log-go-sdk v0.1.12
	github.com/bsm/sarama-cluster v2.1.15+incompatible
	github.com/buger/jsonparser v0.0.0-20180808090653-f4dd9f5a6b44
	github.com/cespare/xxhash/v2 v2.1.1
	github.com/cihub/seelog v0.0.0-20170130134532-f561c5e57575
	github.com/containerd/cri v1.19.0
	github.com/coreos/go-systemd v0.0.0-20190719114852-fd7a80b32e1f
	github.com/danwakefield/fnmatch v0.0.0-20160403171240-cbb64ac3d964
	github.com/denisenkom/go-mssqldb v0.12.2
	github.com/eclipse/paho.mqtt.golang v1.2.0
	github.com/elastic/beats/v7 v7.7.1
	github.com/elastic/go-lumber v0.1.0
	github.com/fsouza/go-dockerclient v1.7.1
	github.com/go-kit/kit v0.10.0
	github.com/go-mysql-org/go-mysql v1.3.0
	github.com/go-ping/ping v0.0.0-20211130115550-779d1e919534
	github.com/go-sql-driver/mysql v1.5.0
	github.com/gogo/protobuf v1.3.2
	github.com/gosnmp/gosnmp v1.31.0
	github.com/influxdata/go-syslog v1.0.1
	github.com/influxdata/influxdb v1.8.4
	github.com/jackc/pgx/v4 v4.16.1
	github.com/jeromer/syslogparser v0.0.0-20190429161531-5fbaaf06d9e7
	github.com/json-iterator/go v1.1.10
	github.com/juju/errors v0.0.0-20170703010042-c7d06af17c68
	github.com/knz/strtime v0.0.0-20181018220328-af2256ee352c
	github.com/mailru/easyjson v0.7.7
	github.com/mindprince/gonvml v0.0.0-20180514031326-b364b296c732
	github.com/oschwald/geoip2-golang v1.1.0
	github.com/paulbellamy/ratecounter v0.2.1-0.20170719102518-a803f0e4f071
	github.com/pierrec/lz4 v2.6.0+incompatible
	github.com/pingcap/check v0.0.0-20200212061837-5e12011dc712
	github.com/pingcap/errors v0.11.5-0.20201126102027-b0a155152ca3
	github.com/pingcap/parser v0.0.0-20210415081931-48e7f467fd74
	github.com/pkg/errors v0.9.1
	github.com/prometheus/common v0.20.0
	github.com/prometheus/procfs v0.6.0
	github.com/shirou/gopsutil v3.21.1+incompatible
	github.com/siddontang/go v0.0.0-20180604090527-bdc77568d726
	github.com/siddontang/go-mysql v0.0.0-20180725024449-535abe8f2eba
	github.com/sirupsen/logrus v1.7.0
	github.com/smartystreets/goconvey v1.7.2
	github.com/stretchr/testify v1.7.0
	github.com/syndtr/goleveldb v0.0.0-20170725064836-b89cc31ef797
	go.uber.org/atomic v1.7.0
	golang.org/x/sys v0.0.0-20210915083310-ed5796bab164
	google.golang.org/grpc v1.40.0
	google.golang.org/protobuf v1.27.1
	gotest.tools v2.2.0+incompatible
	k8s.io/api v0.20.4
	k8s.io/apimachinery v0.20.4
	k8s.io/client-go v0.20.4
	k8s.io/cri-api v0.20.4
)

require (
	cloud.google.com/go v0.81.0 // indirect
	github.com/Azure/go-ansiterm v0.0.0-20170929234023-d6e3b3328b78 // indirect
	github.com/BurntSushi/toml v0.3.1 // indirect
	github.com/Microsoft/go-winio v0.4.16 // indirect
	github.com/Microsoft/hcsshim v0.8.14 // indirect
	github.com/StackExchange/wmi v0.0.0-20181212234831-e0a55b97c705 // indirect
	github.com/VictoriaMetrics/fasthttp v1.0.14 // indirect
	github.com/VictoriaMetrics/metricsql v0.14.0 // indirect
	github.com/agiledragon/gomonkey/v2 v2.4.0 // indirect
	github.com/aliyun/alibaba-cloud-sdk-go v0.0.0-20181127062202-5462e9f9dc05 // indirect
	github.com/andrewkroh/sys v0.0.0-20151128191922-287798fe3e43 // indirect
	github.com/cenkalti/backoff v2.2.1+incompatible // indirect
	github.com/containerd/cgroups v0.0.0-20210114181951-8a68de567b68 // indirect
	github.com/containerd/containerd v1.4.3 // indirect
	github.com/containerd/continuity v0.0.0-20210208174643-50096c924a4e // indirect
	github.com/containerd/fifo v0.0.0-20210129194248-f8e8fdba47ef // indirect
	github.com/containerd/go-cni v1.0.1 // indirect
	github.com/containerd/imgcrypt v1.0.3 // indirect
	github.com/containerd/ttrpc v1.0.2 // indirect
	github.com/containerd/typeurl v1.0.1 // indirect
	github.com/containernetworking/cni v0.8.1 // indirect
	github.com/containernetworking/plugins v0.9.1 // indirect
	github.com/containers/ocicrypt v1.0.3 // indirect
	github.com/coreos/go-systemd/v22 v22.1.0 // indirect
	github.com/coreos/pkg v0.0.0-20180928190104-399ea9e2e55f // indirect
	github.com/davecgh/go-spew v1.1.1 // indirect
	github.com/dlclark/regexp2 v1.7.0
	github.com/docker/docker v20.10.3+incompatible // indirect
	github.com/docker/go-connections v0.4.0 // indirect
	github.com/docker/go-events v0.0.0-20190806004212-e31b211e4f1c // indirect
	github.com/docker/go-units v0.4.0 // indirect
	github.com/docker/spdystream v0.0.0-20160310174837-449fdfce4d96 // indirect
	github.com/eapache/go-resiliency v1.2.0 // indirect
	github.com/eapache/go-xerial-snappy v0.0.0-20180814174437-776d5712da21 // indirect
	github.com/eapache/queue v1.1.0 // indirect
	github.com/emicklei/go-restful v2.15.0+incompatible // indirect
	github.com/fsnotify/fsnotify v1.4.9 // indirect
	github.com/go-logfmt/logfmt v0.5.0 // indirect
	github.com/go-logr/logr v0.4.0 // indirect
	github.com/go-ole/go-ole v1.2.4 // indirect
	github.com/godbus/dbus/v5 v5.0.3 // indirect
	github.com/gogo/googleapis v1.4.0 // indirect
	github.com/golang-sql/civil v0.0.0-20220223132316-b832511892a9 // indirect
	github.com/golang-sql/sqlexp v0.1.0 // indirect
	github.com/golang/groupcache v0.0.0-20210331224755-41bb18bfe9da // indirect
	github.com/golang/protobuf v1.5.2 // indirect
	github.com/golang/snappy v0.0.3 // indirect
	github.com/google/go-cmp v0.5.5 // indirect
	github.com/google/gofuzz v1.1.0 // indirect
	github.com/google/uuid v1.2.0 // indirect
	github.com/googleapis/gnostic v0.4.1 // indirect
	github.com/gopherjs/gopherjs v0.0.0-20181017120253-0766667cb4d1 // indirect
	github.com/hashicorp/go-uuid v1.0.2 // indirect
	github.com/hashicorp/golang-lru v0.5.4 // indirect
	github.com/imdario/mergo v0.3.8 // indirect
	github.com/jackc/chunkreader/v2 v2.0.1 // indirect
	github.com/jackc/pgconn v1.12.1 // indirect
	github.com/jackc/pgio v1.0.0 // indirect
	github.com/jackc/pgpassfile v1.0.0 // indirect
	github.com/jackc/pgproto3/v2 v2.3.0 // indirect
	github.com/jackc/pgservicefile v0.0.0-20200714003250-2b9c44734f2b // indirect
	github.com/jackc/pgtype v1.11.0 // indirect
	github.com/jcmturner/aescts/v2 v2.0.0 // indirect
	github.com/jcmturner/dnsutils/v2 v2.0.0 // indirect
	github.com/jcmturner/gofork v1.0.0 // indirect
	github.com/jcmturner/gokrb5/v8 v8.4.2 // indirect
	github.com/jcmturner/rpc/v2 v2.0.3 // indirect
	github.com/jmespath/go-jmespath v0.4.0 // indirect
	github.com/joeshaw/multierror v0.0.0-20140124173710-69b34d4ec901 // indirect
	github.com/josharian/intern v1.0.0 // indirect
	github.com/jtolds/gls v4.20.0+incompatible // indirect
	github.com/juju/testing v0.0.0-20200608005635-e4eedbc6f7aa // indirect
	github.com/klauspost/compress v1.11.13 // indirect
	github.com/matttproud/golang_protobuf_extensions v1.0.2-0.20181231171920-c182affec369 // indirect
	github.com/moby/sys/mount v0.2.0 // indirect
	github.com/moby/sys/mountinfo v0.4.0 // indirect
	github.com/moby/sys/symlink v0.1.0 // indirect
	github.com/moby/term v0.0.0-20201216013528-df9cb8a40635 // indirect
	github.com/modern-go/concurrent v0.0.0-20180306012644-bacd9c7ef1dd // indirect
	github.com/modern-go/reflect2 v1.0.1 // indirect
	github.com/morikuni/aec v1.0.0 // indirect
	github.com/onsi/ginkgo v1.15.0 // indirect
	github.com/onsi/gomega v1.10.5 // indirect
	github.com/opencontainers/go-digest v1.0.0 // indirect
	github.com/opencontainers/image-spec v1.0.1 // indirect
	github.com/opencontainers/runc v1.0.0-rc8.0.20190926000215-3e425f80a8c9 // indirect
	github.com/opencontainers/runtime-spec v1.0.2 // indirect
	github.com/opencontainers/selinux v1.8.0 // indirect
	github.com/oschwald/maxminddb-golang v1.2.1 // indirect
	github.com/pingcap/log v0.0.0-20210317133921-96f4fcab92a4 // indirect
	github.com/pmezard/go-difflib v1.0.0 // indirect
	github.com/prometheus/client_model v0.2.0 // indirect
	github.com/rcrowley/go-metrics v0.0.0-20201227073835-cf1acfcdf475 // indirect
	github.com/satori/go.uuid v1.2.1-0.20181028125025-b2ce2384e17b // indirect
	github.com/shopspring/decimal v1.2.0 // indirect
	github.com/siddontang/go-log v0.0.0-20180807004314-8d05993dda07 // indirect
	github.com/smartystreets/assertions v1.2.0 // indirect
	github.com/spf13/pflag v1.0.5 // indirect
	github.com/syndtr/gocapability v0.0.0-20200815063812-42c35b437635 // indirect
	github.com/tatsushid/go-fastping v0.0.0-20160109021039-d7bb493dee3e // indirect
	github.com/tchap/go-patricia v2.3.0+incompatible // indirect
	github.com/valyala/bytebufferpool v1.0.0 // indirect
	github.com/valyala/fastjson v1.6.3 // indirect
	github.com/valyala/fastrand v1.0.0 // indirect
	github.com/valyala/fasttemplate v1.2.1 // indirect
	github.com/valyala/histogram v1.1.2 // indirect
	github.com/valyala/quicktemplate v1.6.3 // indirect
	github.com/willf/bitset v1.1.11 // indirect
	go.mozilla.org/pkcs7 v0.0.0-20200128120323-432b2356ecb1 // indirect
	go.opencensus.io v0.23.0 // indirect
	go.uber.org/multierr v1.6.0 // indirect
	go.uber.org/zap v1.16.0 // indirect
	golang.org/x/crypto v0.0.0-20220525230936-793ad666bf5e // indirect
	golang.org/x/net v0.0.0-20211112202133-69e39bad7dc2 // indirect
	golang.org/x/oauth2 v0.0.0-20210402161424-2e8d93401602 // indirect
	golang.org/x/sync v0.0.0-20210220032951-036812b2e83c // indirect
	golang.org/x/term v0.0.0-20201126162022-7de9c90e9dd1 // indirect
	golang.org/x/text v0.3.7 // indirect
	golang.org/x/time v0.0.0-20200630173020-3af7569d3a1e // indirect
	google.golang.org/appengine v1.6.7 // indirect
	google.golang.org/genproto v0.0.0-20210406143921-e86de6bf7a46 // indirect
	gopkg.in/birkirb/loggers.v1 v1.0.3 // indirect
	gopkg.in/inf.v0 v0.9.1 // indirect
	gopkg.in/natefinch/lumberjack.v2 v2.0.0 // indirect
	gopkg.in/square/go-jose.v2 v2.3.1 // indirect
	gopkg.in/yaml.v2 v2.4.0 // indirect
	gopkg.in/yaml.v3 v3.0.0-20210107192922-496545a6307b // indirect
	k8s.io/apiserver v0.20.4 // indirect
	k8s.io/klog v1.0.0 // indirect
	k8s.io/klog/v2 v2.5.0 // indirect
	k8s.io/kube-openapi v0.0.0-20201113171705-d219536bb9fd // indirect
	k8s.io/utils v0.0.0-20210111153108-fddb29f9d009 // indirect
	launchpad.net/gocheck v0.0.0-20140225173054-000000000087 // indirect
	sigs.k8s.io/structured-merge-diff/v4 v4.0.2 // indirect
	sigs.k8s.io/yaml v1.2.0 // indirect
)

replace (
	github.com/alibaba/ilogtail/pkg => ./pkg
	github.com/aliyun/alibaba-cloud-sdk-go/services/sls_inner => ./external/github.com/aliyun/alibaba-cloud-sdk-go/services/sls_inner
	github.com/elastic/beats/v7 => ./external/github.com/elastic/beats/v7
	github.com/jeromer/syslogparser => ./external/github.com/jeromer/syslogparser
	github.com/satori/go.uuid => github.com/satori/go.uuid v1.2.0
)
