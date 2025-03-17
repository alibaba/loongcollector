#include "ebpf/type/table/StaticDataRow.h"

#include "ebpf/type/table/AppTable.h"
#include "ebpf/type/table/NetTable.h"

namespace logtail {
namespace ebpf {

template class StaticDataRow<&kConnTrackerTable>;
template class StaticDataRow<&kAppMetricsTable>;
template class StaticDataRow<&kNetMetricsTable>;

} // namespace ebpf
} // namespace logtail
