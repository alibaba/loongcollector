# Data structure

The doc shows the observer data structure. Each metrics data structure contains 4 parts, which are connection labels,
process labels, timestamp and metrics values.

## Common structure

Common structure means which are referenced by the most other structures.

### Local info

| Column             | Type   | Meaning                                             | Required column |
|--------------------|--------|-----------------------------------------------------|-----------------|
| `_namespace_`      | string | local kubernetes namespace                          | false           |
| `_container_name_` | string | local container name                                | false           |
| `_pod_name_`       | string | local kubernetes pod name                           | false           |
| `_workload_name_`  | string | local kubernetes workload name                      | false           |
| `_running_mode_`   | string | currently running mode, such as kubernetes and host | false           |
| `__process_cmd_"_` | string | process command line                                | false           |
| `__process_pid_"_` | string | process id                                          | false           |

### Remote info

| Column          | Type   | Meaning             | Required column |
|-----------------|--------|---------------------|-----------------|
| `_remote_host_` | string | remote dns hostname | true            |

## Layer 7 data structure

### DB metric data structure

| Column          | Type               | Meaning                                                                         | Required column |
|-----------------|--------------------|---------------------------------------------------------------------------------|-----------------|
| `__time__`      | int                | timestamp for aggregation                                                       | true            |
| `_local_addr`_  | string             | local address                                                                   | true            |
| `_local_port`_  | int                | local port                                                                      | true            |
| `_remote_addr`_ | string             | remote address                                                                  | true            |
| `_remote_port`_ | int                | remote port                                                                     | true            |
| type            | string             | l7_db means L7 DB metrics, l7_req means L7 RPC metrics, and l4 means L4 metrics | true            |
| local_info      | `Local Info` Json  | local information                                                               | true            |
| remote_info     | `Remote Info` Json | remote information                                                              | true            |
| conn_id         | int                | each aggregation belongs to one unique connection id                            | true            |
| interval        | int                | unit is  seconds, aggregation interval                                          | true            |
| role            | string             | c means client, s means server                                                  | true            |
| version         | string             | protocol version                                                                | true            |
| protocol        | string             | L7 detect protocol                                                              | true            |
| query_cmd       | string             | DB query command，such as select, insert and etc.                                | true            |
| query           | string             | DB query statement.                                                             | true            |
| extra           | json               | extra message for feature.                                                      | true            |
| status          | int                | the query result, 0 means failure, 1 means success.                             | true            |
| latency_ns      | int                | the total invoke cost ns                                                        | true            |
| tdigest_latency | string             | base64 tdigest data for compute Pxx                                             | true            |
| count           | int                | the total invoke cost                                                           | true            |
| req_bytes       | int                | the total requst bytes                                                          | true            |
| resp_bytes      | int                | the total response bytes                                                        | true            |

### Request metric data structure

Request metric works in RPC, DNS or MQ transfer.

| Column          | Type               | Meaning                                                                         | Required column |
|-----------------|--------------------|---------------------------------------------------------------------------------|-----------------|
| `__time__`      | int                | timestamp for aggregation                                                       | true            |
| `_local_addr`_  | string             | local address                                                                   | true            |
| `_local_port`_  | int                | local port                                                                      | true            |
| `_remote_addr`_ | string             | remote address                                                                  | true            |
| `_remote_port`_ | int                | remote port                                                                     | true            |
| type            | string             | l7_db means L7 DB metrics, l7_req means L7 RPC metrics, and l4 means L4 metrics | true            |
| local_info      | `Local Info` Json  | local information                                                               | true            |
| remote_info     | `Remote Info` Json | remote information                                                              | true            |
| conn_id         | int                | each aggregation belongs to one unique connection id                            | true            |
| interval        | int                | unit is  seconds, aggregation interval                                          | true            |
| role            | string             | c means client, s means server                                                  | true            |
| version         | string             | protocol version                                                                | true            |
| protocol        | string             | L7 detect protocol                                                              | true            |
| req_type        | string             | request type, such as POST in HTTP.                                             | true            |
| req_domain      | string             | request dommain, such as domain in HTTP.                                        | true            |
| req_resource    | string             | request specific resource, such HTTP path or RPC method.                        | true            |
| resp_code       | int                | response code, currently only works in HTTP.                                    | true            |
| resp_status     | int                | response status, 0 means success, non-zero means failure.                       | true            |
| extra           | json               | extra message for feature.                                                      | true            |
| latency_ns      | int                | the total invoke cost ns                                                        | true            |
| tdigest_latency | string             | base64 tdigest data for compute Pxx                                             | true            |
| count           | int                | the total invoke cost                                                           | true            |
| req_bytes       | int                | the total requst bytes                                                          | true            |
| resp_bytes      | int                | the total response bytes                                                        | true            |

## Layer 4 metrics data structure

| Column          | Type               | Meaning                                                                         | Required column |
|-----------------|--------------------|---------------------------------------------------------------------------------|-----------------|
| `__time__`      | int                | timestamp for aggregation                                                       | true            |
| `_local_addr`_  | string             | local address                                                                   | true            |
| `_local_port`_  | int                | local port                                                                      | true            |
| `_remote_addr`_ | string             | remote address                                                                  | true            |
| `_remote_port`_ | int                | remote port                                                                     | true            |
| type            | string             | l7_db means L7 DB metrics, l7_req means L7 RPC metrics, and l4 means L4 metrics | true            |
| local_info      | `Local Info` Json  | local information                                                               | true            |
| remote_info     | `Remote Info` Json | remote information                                                              | true            |
| conn_id         | int                | each aggregation belongs to one unique connection id                            | true            |
| interval        | int                | unit is  seconds, aggregation interval                                          | true            |
| role            | string             | c means client, s means server                                                  | true            |
| conn_type       | int                | 0 means TCP, and 1 means UDP.                                                   | true            |
| extra           | json               | extra message for feature.                                                      | true            |
| recv_bytes      | int                | the total receive bytes                                                         | true            |
| send_bytes      | int                | the total send bytes                                                            | true            |
| recv_packets    | int                | the total receive packets                                                       | true            |
| send_packets    | int                | the total send packets                                                          | true            |
