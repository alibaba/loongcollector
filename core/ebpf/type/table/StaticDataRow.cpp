#include "ebpf/type/table/StaticDataRow.h"

namespace logtail {
namespace ebpf {

template class StaticDataRow<&kConnTrackerTable>;
template class StaticDataRow<&kAppMetricsTable>;
template class StaticDataRow<&kNetMetricsTable>;

template class StaticDataRow<&kProcessCacheTable>;

} // namespace ebpf
} // namespace logtail
