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

#ifndef LOGTAIL_RAWLOGGROUP_H
#define LOGTAIL_RAWLOGGROUP_H

#include <RawLog.h>

namespace logtail
{

typedef std::pair<std::string, std::string> LogTag;

class RawLogGroup
{
public:
    RawLogGroup() = default;
    ~RawLogGroup();

    int logs_size() const;
    void clear_logs();
    const RawLog& logs(int index) const;
    RawLog* mutable_logs(int index);
    void add_logs(RawLog * log);
    std::vector<RawLog *>* mutable_logs();
    const std::vector<RawLog *> & logs();

    // repeated .sls_logs.LogTag LogTags = 6;
    int logtags_size() const;
    void clear_logtags();
    const LogTag& logtags(int index) const;
    LogTag* mutable_logtags(int index);
    void add_logtags(const std::string & key, const std::string & value);
    void add_logtags(const std::string & key, std::string && value);
    std::vector<LogTag>* mutable_logtags();
    const std::vector<LogTag>& logtags() const;

    // optional string Category = 2; 0x12
    bool has_category() const;
    void clear_category();
    const ::std::string& category() const;
    void set_category(const ::std::string& value);
    void set_category(::std::string&& value);
    void set_category(const char* value, size_t size);
    ::std::string* mutable_category();

    // optional string Topic = 3; 0x1A
    bool has_topic() const;
    void clear_topic();
    const ::std::string& topic() const;
    void set_topic(const ::std::string& value);
    void set_topic(::std::string&& value);
    void set_topic(const char* value, size_t size);
    ::std::string* mutable_topic();

    // optional string Source = 4; 0x22;
    bool has_source() const;
    void clear_source();
    const ::std::string& source() const;
    void set_source(const ::std::string& value);
    void set_source(::std::string&& value);
    void set_source(const char* value, size_t size);
    ::std::string* mutable_source();

    // optional string MachineUUID = 5; 0x2A
    bool has_machineuuid() const;
    void clear_machineuuid();
    static const int kMachineUUIDFieldNumber = 5;
    const ::std::string& machineuuid() const;
    void set_machineuuid(const ::std::string& value);
    void set_machineuuid(::std::string&& value);
    void set_machineuuid(const char* value, size_t size);
    ::std::string* mutable_machineuuid();

    // Serialize the message and store it in the given string.  All required
    // fields must be set.
    bool SerializeToString(std::string* output) const;
    // Like SerializeToString(), but appends to the data to the string's existing
    // contents.  All required fields must be set.
    bool AppendToString(std::string* output) const;

private:
    void set_has_category();
    void clear_has_category();
    void set_has_topic();
    void clear_has_topic();
    void set_has_source();
    void clear_has_source();
    void set_has_machineuuid();
    void clear_has_machineuuid();

    void pack_logs(std::string * output) const;
    void pack_logtags(std::string * output) const;
    void pack_others(std::string * output) const;


    uint64_t _has_bits_ = 0;
    std::string category_;
    std::string topic_;
    std::string source_;
    std::string machineuuid_;
    std::vector<LogTag> logtags_;
    std::vector<RawLog *> rawlogs_;
};

// optional string Category = 2;
inline bool RawLogGroup::has_category() const {
    return (_has_bits_ & 0x00000001u) != 0;
}
inline void RawLogGroup::set_has_category() {
    _has_bits_ |= 0x00000001u;
}
inline void RawLogGroup::clear_has_category() {
    _has_bits_ &= ~0x00000001u;
}
inline void RawLogGroup::clear_category() {
    category_.clear();
    clear_has_category();
}
inline const ::std::string& RawLogGroup::category() const {
    // @@protoc_insertion_point(field_get:sls_logs.LogGroup.Category)
    return category_;
}
inline void RawLogGroup::set_category(const ::std::string& value) {
    set_has_category();
    category_=value;
    // @@protoc_insertion_point(field_set:sls_logs.LogGroup.Category)
}
inline void RawLogGroup::set_category(::std::string&& value) {
    set_has_category();
    category_=::std::move(value);
    // @@protoc_insertion_point(field_set_rvalue:sls_logs.LogGroup.Category)
}
inline void RawLogGroup::set_category(const char* value, size_t size) {
    set_has_category();
    category_=::std::string(reinterpret_cast<const char*>(value), size);;
    // @@protoc_insertion_point(field_set_pointer:sls_logs.LogGroup.Category)
}
inline ::std::string* RawLogGroup::mutable_category() {
    set_has_category();
    // @@protoc_insertion_point(field_mutable:sls_logs.LogGroup.Category)
    return &category_;;
}

// optional string Topic = 3;
inline bool RawLogGroup::has_topic() const {
    return (_has_bits_ & 0x00000002u) != 0;
}
inline void RawLogGroup::set_has_topic() {
    _has_bits_ |= 0x00000002u;
}
inline void RawLogGroup::clear_has_topic() {
    _has_bits_ &= ~0x00000002u;
}
inline void RawLogGroup::clear_topic() {
    topic_.clear();
    clear_has_topic();
}
inline const ::std::string& RawLogGroup::topic() const {
    // @@protoc_insertion_point(field_get:sls_logs.LogGroup.Topic)
    return topic_;
}
inline void RawLogGroup::set_topic(const ::std::string& value) {
    set_has_topic();
    topic_=value;
    // @@protoc_insertion_point(field_set:sls_logs.LogGroup.Topic)
}
inline void RawLogGroup::set_topic(::std::string&& value) {
    set_has_topic();
    topic_=::std::move(value);
    // @@protoc_insertion_point(field_set_rvalue:sls_logs.LogGroup.Topic)
}
inline void RawLogGroup::set_topic(const char* value, size_t size) {
    set_has_topic();
    topic_=::std::string(reinterpret_cast<const char*>(value), size);
    // @@protoc_insertion_point(field_set_pointer:sls_logs.LogGroup.Topic)
}
inline ::std::string* RawLogGroup::mutable_topic() {
    set_has_topic();
    // @@protoc_insertion_point(field_mutable:sls_logs.LogGroup.Topic)
    return &topic_;;
}

// optional string Source = 4;
inline bool RawLogGroup::has_source() const {
    return (_has_bits_ & 0x00000004u) != 0;
}
inline void RawLogGroup::set_has_source() {
    _has_bits_ |= 0x00000004u;
}
inline void RawLogGroup::clear_has_source() {
    _has_bits_ &= ~0x00000004u;
}
inline void RawLogGroup::clear_source() {
    source_.clear();
    clear_has_source();
}
inline const ::std::string& RawLogGroup::source() const {
    // @@protoc_insertion_point(field_get:sls_logs.LogGroup.Source)
    return source_;
}
inline void RawLogGroup::set_source(const ::std::string& value) {
    set_has_source();
    source_=value;
    // @@protoc_insertion_point(field_set:sls_logs.LogGroup.Source)
}
inline void RawLogGroup::set_source(::std::string&& value) {
    set_has_source();
    source_=::std::move(value);
    // @@protoc_insertion_point(field_set_rvalue:sls_logs.LogGroup.Source)
}
inline void RawLogGroup::set_source(const char* value, size_t size) {
    set_has_source();
    source_=::std::string(reinterpret_cast<const char*>(value), size);
    // @@protoc_insertion_point(field_set_pointer:sls_logs.LogGroup.Source)
}
inline ::std::string* RawLogGroup::mutable_source() {
    set_has_source();
    // @@protoc_insertion_point(field_mutable:sls_logs.LogGroup.Source)
    return &source_;;
}

// optional string MachineUUID = 5;
inline bool RawLogGroup::has_machineuuid() const {
    return (_has_bits_ & 0x00000008u) != 0;
}
inline void RawLogGroup::set_has_machineuuid() {
    _has_bits_ |= 0x00000008u;
}
inline void RawLogGroup::clear_has_machineuuid() {
    _has_bits_ &= ~0x00000008u;
}
inline void RawLogGroup::clear_machineuuid() {
    machineuuid_.clear();
    clear_has_machineuuid();
}
inline const ::std::string& RawLogGroup::machineuuid() const {
    // @@protoc_insertion_point(field_get:sls_logs.LogGroup.MachineUUID)
    return machineuuid_;
}
inline void RawLogGroup::set_machineuuid(const ::std::string& value) {
    set_has_machineuuid();
    machineuuid_=value;
    // @@protoc_insertion_point(field_set:sls_logs.LogGroup.MachineUUID)
}
inline void RawLogGroup::set_machineuuid(::std::string&& value) {
    set_has_machineuuid();
    machineuuid_=::std::move(value);
    // @@protoc_insertion_point(field_set_rvalue:sls_logs.LogGroup.MachineUUID)
}
inline void RawLogGroup::set_machineuuid(const char* value, size_t size) {
    set_has_machineuuid();
    machineuuid_=::std::string(reinterpret_cast<const char*>(value), size);
    // @@protoc_insertion_point(field_set_pointer:sls_logs.LogGroup.MachineUUID)
}
inline ::std::string* RawLogGroup::mutable_machineuuid() {
    set_has_machineuuid();
    // @@protoc_insertion_point(field_mutable:sls_logs.LogGroup.MachineUUID)
    return &machineuuid_;;
}

}

#endif //LOGTAIL_RAWLOGGROUP_H
