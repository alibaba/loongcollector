#include "ebpf/driver/BPFMapTraits.h"
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {

class BPFMapTraitsUnittest : public ::testing::Test {
public:
    void TestAddr4MapTraits();
    void TestAddr6MapTraits();
    void TestPortMapTraits();
    void TestStringPrefixMapTraits();
    void TestStringPostfixMapTraits();
    void TestIntMapTraits();

protected:
    void SetUp() override {}
    void TearDown() override {}
};

void BPFMapTraitsUnittest::TestAddr4MapTraits() {
    // 测试 Addr4Map 的特征
    APSARA_TEST_EQUAL_DESC(
        sizeof(BPFMapTraits<Addr4Map>::outter_key_type), sizeof(uint32_t), "Addr4Map outer key size should be 4 bytes");
    APSARA_TEST_EQUAL_DESC(
        sizeof(BPFMapTraits<Addr4Map>::inner_key_type), 8UL, "Addr4Map inner key size should be 8 bytes");
    APSARA_TEST_EQUAL_DESC(
        sizeof(BPFMapTraits<Addr4Map>::inner_val_type), sizeof(uint8_t), "Addr4Map inner value size should be 1 byte");
    APSARA_TEST_EQUAL_DESC(BPFMapTraits<Addr4Map>::outter_map_type,
                           BPF_MAP_TYPE_ARRAY_OF_MAPS,
                           "Addr4Map outer map type should be ARRAY_OF_MAPS");
    APSARA_TEST_EQUAL_DESC(
        BPFMapTraits<Addr4Map>::inner_map_type, BPF_MAP_TYPE_LPM_TRIE, "Addr4Map inner map type should be LPM_TRIE");
    APSARA_TEST_EQUAL_DESC(
        BPFMapTraits<Addr4Map>::map_flag, BPF_F_NO_PREALLOC, "Addr4Map map flag should be NO_PREALLOC");
}

void BPFMapTraitsUnittest::TestAddr6MapTraits() {
    // 测试 Addr6Map 的特征
    APSARA_TEST_EQUAL_DESC(
        sizeof(BPFMapTraits<Addr6Map>::outter_key_type), sizeof(uint32_t), "Addr6Map outer key size should be 4 bytes");
    APSARA_TEST_EQUAL_DESC(
        sizeof(BPFMapTraits<Addr6Map>::inner_key_type), 20UL, "Addr6Map inner key size should be 20 bytes");
    APSARA_TEST_EQUAL_DESC(
        sizeof(BPFMapTraits<Addr6Map>::inner_val_type), sizeof(uint8_t), "Addr6Map inner value size should be 1 byte");
    APSARA_TEST_EQUAL_DESC(BPFMapTraits<Addr6Map>::outter_map_type,
                           BPF_MAP_TYPE_ARRAY_OF_MAPS,
                           "Addr6Map outer map type should be ARRAY_OF_MAPS");
    APSARA_TEST_EQUAL_DESC(
        BPFMapTraits<Addr6Map>::inner_map_type, BPF_MAP_TYPE_LPM_TRIE, "Addr6Map inner map type should be LPM_TRIE");
    APSARA_TEST_EQUAL_DESC(
        BPFMapTraits<Addr6Map>::map_flag, BPF_F_NO_PREALLOC, "Addr6Map map flag should be NO_PREALLOC");
}

void BPFMapTraitsUnittest::TestPortMapTraits() {
    // 测试 PortMap 的特征
    APSARA_TEST_EQUAL_DESC(
        sizeof(BPFMapTraits<PortMap>::outter_key_type), sizeof(uint32_t), "PortMap outer key size should be 4 bytes");
    APSARA_TEST_EQUAL_DESC(
        sizeof(BPFMapTraits<PortMap>::inner_key_type), sizeof(uint32_t), "PortMap inner key size should be 4 bytes");
    APSARA_TEST_EQUAL_DESC(
        sizeof(BPFMapTraits<PortMap>::inner_val_type), sizeof(uint8_t), "PortMap inner value size should be 1 byte");
    APSARA_TEST_EQUAL_DESC(BPFMapTraits<PortMap>::outter_map_type,
                           BPF_MAP_TYPE_ARRAY_OF_MAPS,
                           "PortMap outer map type should be ARRAY_OF_MAPS");
    APSARA_TEST_EQUAL_DESC(
        BPFMapTraits<PortMap>::inner_map_type, BPF_MAP_TYPE_HASH, "PortMap inner map type should be HASH");
    APSARA_TEST_EQUAL_DESC(BPFMapTraits<PortMap>::map_flag, -1, "PortMap map flag should be -1");
}

void BPFMapTraitsUnittest::TestStringPrefixMapTraits() {
    // 测试 StringPrefixMap 的特征
    APSARA_TEST_EQUAL_DESC(sizeof(BPFMapTraits<StringPrefixMap>::outter_key_type),
                           sizeof(uint32_t),
                           "StringPrefixMap outer key size should be 4 bytes");
    APSARA_TEST_EQUAL_DESC(BPFMapTraits<StringPrefixMap>::outter_map_type,
                           BPF_MAP_TYPE_ARRAY_OF_MAPS,
                           "StringPrefixMap outer map type should be ARRAY_OF_MAPS");
    APSARA_TEST_EQUAL_DESC(BPFMapTraits<StringPrefixMap>::inner_map_type,
                           BPF_MAP_TYPE_LPM_TRIE,
                           "StringPrefixMap inner map type should be LPM_TRIE");
    APSARA_TEST_EQUAL_DESC(
        BPFMapTraits<StringPrefixMap>::map_flag, BPF_F_NO_PREALLOC, "StringPrefixMap map flag should be NO_PREALLOC");
}

void BPFMapTraitsUnittest::TestStringPostfixMapTraits() {
    // 测试 StringPostfixMap 的特征
    APSARA_TEST_EQUAL_DESC(sizeof(BPFMapTraits<StringPostfixMap>::outter_key_type),
                           sizeof(uint32_t),
                           "StringPostfixMap outer key size should be 4 bytes");
    APSARA_TEST_EQUAL_DESC(BPFMapTraits<StringPostfixMap>::outter_map_type,
                           BPF_MAP_TYPE_ARRAY_OF_MAPS,
                           "StringPostfixMap outer map type should be ARRAY_OF_MAPS");
    APSARA_TEST_EQUAL_DESC(BPFMapTraits<StringPostfixMap>::inner_map_type,
                           BPF_MAP_TYPE_LPM_TRIE,
                           "StringPostfixMap inner map type should be LPM_TRIE");
    APSARA_TEST_EQUAL_DESC(
        BPFMapTraits<StringPostfixMap>::map_flag, BPF_F_NO_PREALLOC, "StringPostfixMap map flag should be NO_PREALLOC");
}

void BPFMapTraitsUnittest::TestIntMapTraits() {
    // 测试 IntMap 的特征
    APSARA_TEST_EQUAL_DESC(
        sizeof(BPFMapTraits<IntMap>::outter_key_type), sizeof(uint32_t), "IntMap outer key size should be 4 bytes");
    APSARA_TEST_EQUAL_DESC(
        sizeof(BPFMapTraits<IntMap>::inner_key_type), sizeof(uint32_t), "IntMap inner key size should be 4 bytes");
    APSARA_TEST_EQUAL_DESC(
        sizeof(BPFMapTraits<IntMap>::inner_val_type), sizeof(uint8_t), "IntMap inner value size should be 1 byte");
    APSARA_TEST_EQUAL_DESC(BPFMapTraits<IntMap>::outter_map_type,
                           BPF_MAP_TYPE_ARRAY_OF_MAPS,
                           "IntMap outer map type should be ARRAY_OF_MAPS");
    APSARA_TEST_EQUAL_DESC(
        BPFMapTraits<IntMap>::inner_map_type, BPF_MAP_TYPE_HASH, "IntMap inner map type should be HASH");
    APSARA_TEST_EQUAL_DESC(BPFMapTraits<IntMap>::map_flag, -1, "IntMap map flag should be -1");
}

UNIT_TEST_CASE(BPFMapTraitsUnittest, TestAddr4MapTraits);
UNIT_TEST_CASE(BPFMapTraitsUnittest, TestAddr6MapTraits);
UNIT_TEST_CASE(BPFMapTraitsUnittest, TestPortMapTraits);
UNIT_TEST_CASE(BPFMapTraitsUnittest, TestStringPrefixMapTraits);
UNIT_TEST_CASE(BPFMapTraitsUnittest, TestStringPostfixMapTraits);
UNIT_TEST_CASE(BPFMapTraitsUnittest, TestIntMapTraits);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
