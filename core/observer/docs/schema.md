# Data structure

The doc shows the observer data structure. Each metrics data structure contains 4 parts, which are connection labels,
process labels, timestamp and metrics values.

## Common structure

Common structure means which are referenced by the most other structures.

### Local info

| Column           | Type   | Meaning                                    | Required column |
|------------------|--------|--------------------------------------------|-----------------|
| `_namespace_`    | string | local kubernetes namespace                 | false           |
| `_container_`    | string | local container name                       | false           |
| `_pod_`          | string | local kubernetes pod name                  | false           |
| `_workload_`     | string | local kubernetes workload name             | false           |
| `_running_mode_` | string | currently running mode, such as kubernetes | false           |

### Remote info

| Column          | Type   | Meaning     | Required column |
|-----------------|--------|-------------|-----------------|
| `_remote_addr_` | string | remote addr | true            |
| `_remote_port_` | int    | remote port | true            |

## Layer 7 data structure

### DB metric data structure

| Column          | Type               | Meaning                                                               | Required column |
|-----------------|--------------------|-----------------------------------------------------------------------|-----------------|
| time            | int                | timestamp for aggregation                                             | true            |
| type            | int                | 0 means L7 DB metrics, 1 means L7 RPC metrics, and 2 means L4 metrics | true            |
| conn_id         | string             | each aggregation belongs to one unique connection id                  | true            |
| interval_ns     | int                | unit is nano seconds                                                  | true            |
| local_info      | `Local Info` Json  | local information                                                     | true            |
| remote_info     | `Remote Info` Json | remote information                                                    | true            |
| role            | int                | 0 means client, 1 means server                                        | true            |
| protocol        | string             | L7 detect protocol                                                    | true            |
| query_cmd       | string             | DB query command，such as select, insert and etc.                      | true            |
| query           | string             | DB query statement.                                                   | true            |
| extra           | json               | extra message for feature.                                            | true            |
| status          | int                | the query result, 0 means failure, 1 means success.                   | true            |
| latency_ns      | int                | the total invoke cost ns                                              | true            |
| tdigest_latency | string             | base64 tdigest data for compute Pxx                                   | true            |
| count           | int                | the total invoke cost                                                 | true            |
| req_bytes       | int                | the total requst bytes                                                | true            |
| resp_bytes      | int                | the total response bytes                                              | true            |

### Request metric data structure

Request metric works in RPC, DNS or MQ transfer.

| Column          | Type               | Meaning                                                               | Required column |
|-----------------|--------------------|-----------------------------------------------------------------------|-----------------|
| time            | int                | timestamp for aggregation                                             | true            |
| type            | int                | 0 means L7 DB metrics, 1 means L7 RPC metrics, and 2 means L4 metrics | true            |
| conn_id         | string             | each aggregation belongs to one unique connection id                  | true            |
| interval_ns     | int                | unit is nano seconds                                                  | true            |
| local_info      | `Local Info` Json  | local information                                                     | true            |
| remote_info     | `Remote Info` Json | remote information                                                    | true            |
| role            | int                | 0 means client, 1 means server                                        | true            |
| protocol        | string             | L7 detect protocol                                                    | true            |
| req_type        | string             | request type, such as POST in HTTP.                                   | true            |
| req_domain      | string             | request dommain, such as domain in HTTP.                              | true            |
| req_resource    | string             | request specific resource, such HTTP path or RPC method.              | true            |
| resp_code       | int                | response code, currently only works in HTTP.                          | true            |
| resp_status     | int                | response status, 0 means success, non-zero means failure.             | true            |
| extra           | json               | extra message for feature.                                            | true            |
| latency_ns      | int                | the total invoke cost ns                                              | true            |
| tdigest_latency | string             | base64 tdigest data for compute Pxx                                   | true            |
| count           | int                | the total invoke cost                                                 | true            |
| req_bytes       | int                | the total requst bytes                                                | true            |
| resp_bytes      | int                | the total response bytes                                              | true            |

## Layer 4 metrics data structure

| Column        | Type               | Meaning                                                               | Required column |
|---------------|--------------------|-----------------------------------------------------------------------|-----------------|
| __time__      | int                | timestamp for aggregation                                             | true            |
| _local_addr_  | string             | local address                                                         | true            |
| _local_port_  | int                | local port                                                            | true            |
| _remote_addr_ | string             | remote address                                                        | true            |
| _remote_port_ | int                | remote port                                                           | true            |
| type          | int                | 0 means L7 DB metrics, 1 means L7 RPC metrics, and 2 means L4 metrics | true            |
| local_info    | `Local Info` Json  | local information                                                     | true            |
| remote_info   | `Remote Info` Json | remote information                                                    | true            |
| conn_id       | int                | each aggregation belongs to one unique connection id                  | true            |
| interval      | string             | unit is  seconds, aggregation interval                                | true            |
| role          | int                | 0 means client, 1 means server                                        | true            |
| conn_type     | int                | 0 means TCP, and 1 means UDP.                                         | true            |
| extra         | json               | extra message for feature.                                            | true            |
| recv_bytes    | int                | the total receive bytes                                               | true            |
| send_bytes    | int                | the total send bytes                                                  | true            |
| recv_packets  | int                | the total receive packets                                             | true            |
| send_packets  | int                | the total send packets                                                | true            |
