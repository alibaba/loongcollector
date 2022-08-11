/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LCC_SYSCALL_NET_H
#define LCC_SYSCALL_NET_H

#define CONN_DATA_MAX_SIZE 16384
#define DATA_SAMPLE_ALL 100

enum support_proto_e {
    ProtoUnknown = 0,
    ProtoHTTP = 1,
    ProtoMySQL = 2,
    ProtoDNS = 3,
    ProtoRedis = 4,
    ProtoKafka = 5,
    ProtoPGSQL = 6,
    ProtoMongo = 7,
    ProtoDubbo = 8,
    ProtoHSF = 9,
    NumProto,
};
enum support_role_e {
    IsUnknown = 0x01,
    IsClient = 0x02,
    IsServer = 0x04,
};

enum tgid_config_e {
    TgidIndex = 0,
    TgidNum,
};

enum support_conn_status_e {
    StatusOpen,
    StatusClose,
};

enum support_syscall_e {
    FuncUnknown,
    FuncWrite,
    FuncRead,
    FuncSend,
    FuncRecv,
    FuncSendTo,
    FuncRecvFrom,
    FuncSendMsg,
    FuncRecvMsg,
    FuncMmap,
    FuncSockAlloc,
    FuncAccept,
    FuncAccept4,
    FuncSecuritySendMsg,
    FuncSecurityRecvMsg,
};

enum support_direction_e {
    DirUnknown,
    DirIngress,
    DirEgress,
};

enum support_event_e {
    EventConnect,
    EventClose,
};

enum support_tgid_e {
    TgidUndefine,
    TgidAll,
    TgidMatch,
    TgidUnmatch,
};

enum support_type_e { TypeUnknown, TypeRequest, TypeResponse };

struct addr_pair_t {
    uint32_t saddr;
    uint32_t daddr;
    uint16_t sport;
    uint16_t dport;
};

struct map_syscall_t {
    int funcid;
    char* funcname;
};

struct mproto_t {
    int protocol;
    char* proto_name;
};

struct test_data {
    struct addr_pair_t ap;
    uint64_t size;
    int fd;
    char com[16];
    char func[16];
    int pid;
    int family;
    int funcid;
    int ret_val;
};

union sockaddr_t {
    struct sockaddr sa;
    struct sockaddr_in in4;
    struct sockaddr_in6 in6;
};

struct connect_id_t {
    int32_t fd;
    uint32_t tgid;
    uint64_t start;
};

struct conn_event_t {
    union sockaddr_t addr;
    enum support_role_e role;
};

struct close_event_t {
    int64_t wr_bytes;
    int64_t rd_bytes;
};

struct conn_ctrl_event_t {
    enum support_event_e type;
    uint64_t ts;
    struct connect_id_t conn_id;
    union {
        struct conn_event_t connect;
        struct close_event_t close;
    };
};

struct conn_data_event_t {
    struct attr_t {
        uint64_t ts;
        struct connect_id_t conn_id;
        union sockaddr_t addr;
        enum support_proto_e protocol;
        enum support_role_e role;
        enum support_type_e type;
        enum support_direction_e direction;
        enum support_syscall_e syscall_func;
        uint64_t pos;
        uint32_t org_msg_size;
        uint32_t msg_buf_size;
        bool try_to_prepend;
        uint32_t length_header;
    } attr;
    char msg[CONN_DATA_MAX_SIZE];
};

struct conn_stats_event_t {
    uint64_t ts;
    struct connect_id_t conn_id;
    union sockaddr_t addr;
    enum support_role_e role;
    int64_t wr_bytes;
    int64_t rd_bytes;
    int32_t wr_pkts;
    int32_t rd_pkts;
    int64_t last_output_wr_bytes;
    int64_t last_output_rd_bytes;
    int32_t last_output_wr_pkts;
    int32_t last_output_rd_pkts;
    uint32_t conn_events;
};

struct connect_info_t {
    struct connect_id_t conn_id;
    union sockaddr_t addr;
    enum support_proto_e protocol;
    enum support_role_e role;
    enum support_type_e type;
    int64_t wr_bytes;
    int64_t rd_bytes;
    int32_t wr_pkts;
    int32_t rd_pkts;
    int64_t last_output_wr_bytes;
    int64_t last_output_rd_bytes;
    int32_t last_output_wr_pkts;
    int32_t last_output_rd_pkts;
    int32_t total_bytes_for_proto;
    uint64_t last_output_time;
    size_t prev_count;
    char prev_buf[4];
    bool try_to_prepend;
    bool is_sample;
};

struct protocol_type_t {
    enum support_proto_e protocol;
    enum support_type_e type;
};

struct tg_info_t {
    uint32_t tgid;
    int32_t fd;
    enum support_role_e role;
};

struct conn_param_t {
    const struct sockaddr* addr;
    int32_t fd;
};

struct accept_param_t {
    struct sockaddr* addr;
    struct socket* accept_socket;
};

struct close_param_t {
    int32_t fd;
};

struct data_param_t {
    enum support_syscall_e syscall_func;
    bool real_conn;
    int32_t fd;
    const char* buf;
    const struct iovec* iov;
    size_t iovlen;
    unsigned int* msg_len;
};

struct config_info_t {
    int32_t port;
    int32_t self_pid;
    int32_t data_sample;
};
#endif
