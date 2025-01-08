#pragma once

extern "C" {
#include <coolbpf.h>
#include <bpf/libbpf.h>
};
#include <coolbpf/security/type.h>

namespace logtail {
namespace ebpf {
/**
 * Get information about a particular ValueType. For example: mapping back to the
 * enum type.
 * @tparam T the ValueType.
 */
template <typename T>
struct BPFMapTraits {
};

/**
 * 
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY_OF_MAPS);
	__uint(max_entries, ADDR_LPM_MAPS_OUTER_MAX_ENTRIES);
	__uint(key_size, sizeof(__u32));
	__array(
		values, struct {
			__uint(type, BPF_MAP_TYPE_LPM_TRIE);
			__uint(max_entries, 1);
			__type(key, __u8[8]); // Need to specify as byte array as wouldn't take struct as key type
			__type(value, __u8);
			__uint(map_flags, BPF_F_NO_PREALLOC);
		});
} addr4lpm_maps SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY_OF_MAPS);
	__uint(max_entries, ADDR_LPM_MAPS_OUTER_MAX_ENTRIES);
	__uint(key_size, sizeof(__u32));
	__array(
		values, struct {
			__uint(type, BPF_MAP_TYPE_LPM_TRIE);
			__uint(max_entries, 1);
			__type(key, __u8[20]); // Need to specify as byte array as wouldn't take struct as key type
			__type(value, __u8);
			__uint(map_flags, BPF_F_NO_PREALLOC);
		});
} addr6lpm_maps SEC(".maps");
 */
struct Addr6Map {};

template <>
struct BPFMapTraits<Addr6Map> {
  using outter_key_type = uint32_t;
  using inner_key_type = uint8_t[20];
  using inner_val_type = uint8_t;
  static constexpr uint32_t outter_key_size = sizeof(uint32_t);
  static constexpr uint32_t inner_key_size = sizeof(uint8_t[20]);
  static constexpr uint32_t inner_val_size = sizeof(uint8_t);
  static constexpr int outter_max_entries = ADDR_LPM_MAPS_OUTER_MAX_ENTRIES;
  static constexpr int inner_max_entries = ADDR_LPM_MAPS_INNER_MAX_ENTRIES;
  static constexpr enum bpf_map_type outter_map_type = BPF_MAP_TYPE_ARRAY_OF_MAPS;
  // BPF_MAP_TYPE_LPM_TRIE
  static constexpr enum bpf_map_type inner_map_type = BPF_MAP_TYPE_LPM_TRIE;
  static constexpr int map_flag = BPF_F_NO_PREALLOC;
};

struct Addr4Map {};

template <>
struct BPFMapTraits<Addr4Map> {
  using outter_key_type = uint32_t;
  using inner_key_type = uint8_t[8];
  using inner_val_type = uint8_t;
  static constexpr uint32_t outter_key_size = sizeof(uint32_t);
  static constexpr uint32_t inner_key_size = sizeof(uint8_t[8]);
  static constexpr uint32_t inner_val_size = sizeof(uint8_t);
  static constexpr int outter_max_entries = ADDR_LPM_MAPS_OUTER_MAX_ENTRIES;
  static constexpr int inner_max_entries = ADDR_LPM_MAPS_INNER_MAX_ENTRIES;
  static constexpr enum bpf_map_type outter_map_type = BPF_MAP_TYPE_ARRAY_OF_MAPS;
  // BPF_MAP_TYPE_LPM_TRIE
  static constexpr enum bpf_map_type inner_map_type = BPF_MAP_TYPE_LPM_TRIE;
  static constexpr int map_flag = BPF_F_NO_PREALLOC;
};

struct PortMap{};

template <>
struct BPFMapTraits<PortMap> {
  using outter_key_type = uint32_t;
  using inner_key_type = uint32_t;
  using inner_val_type = uint8_t;
  static constexpr uint32_t outter_key_size = sizeof(uint32_t);
  static constexpr uint32_t inner_key_size = sizeof(uint32_t);
  static constexpr uint32_t inner_val_size = sizeof(uint8_t);
  static constexpr int outter_max_entries = INT_MAPS_OUTER_MAX_ENTRIES;
  static constexpr int inner_max_entries = INT_MAPS_INNER_MAX_ENTRIES;
  static constexpr enum bpf_map_type outter_map_type = BPF_MAP_TYPE_ARRAY_OF_MAPS;
  // BPF_MAP_TYPE_LPM_TRIE
  static constexpr enum bpf_map_type inner_map_type = BPF_MAP_TYPE_HASH;
  static constexpr int map_flag = -1;
};

struct StringPrefixMap {};

template <>
struct BPFMapTraits<StringPrefixMap> {
  using outter_key_type = uint32_t;
  using inner_key_type = string_prefix_lpm_trie;
  using inner_val_type = uint8_t;
  static constexpr uint32_t outter_key_size = sizeof(uint32_t);
  static constexpr uint32_t inner_key_size = sizeof(uint8_t[sizeof(string_prefix_lpm_trie)]);
  static constexpr uint32_t inner_val_size = sizeof(uint8_t);
  static constexpr int outter_max_entries = STRING_MAPS_OUTER_MAX_ENTRIES;
  static constexpr int inner_max_entries = STRING_MAPS_INNER_MAX_ENTRIES;
  static constexpr enum bpf_map_type outter_map_type = BPF_MAP_TYPE_ARRAY_OF_MAPS;
  // BPF_MAP_TYPE_LPM_TRIE
  static constexpr enum bpf_map_type inner_map_type = BPF_MAP_TYPE_LPM_TRIE;
  static constexpr int map_flag = BPF_F_NO_PREALLOC;
};

struct StringPostfixMap {};

template <>
struct BPFMapTraits<StringPostfixMap> {
  using outter_key_type = uint32_t;
  using inner_key_type = string_postfix_lpm_trie;
  using inner_val_type = uint8_t;
  static constexpr uint32_t outter_key_size = sizeof(uint32_t);
  static constexpr uint32_t inner_key_size = sizeof(uint8_t[sizeof(string_postfix_lpm_trie)]);
  static constexpr uint32_t inner_val_size = sizeof(uint8_t);
  static constexpr int outter_max_entries = STRING_MAPS_OUTER_MAX_ENTRIES;
  static constexpr int inner_max_entries = STRING_MAPS_INNER_MAX_ENTRIES;
  static constexpr enum bpf_map_type outter_map_type = BPF_MAP_TYPE_ARRAY_OF_MAPS;
  // BPF_MAP_TYPE_LPM_TRIE
  static constexpr enum bpf_map_type inner_map_type = BPF_MAP_TYPE_LPM_TRIE;
  static constexpr int map_flag = BPF_F_NO_PREALLOC;
};

struct IntMap {};
template <>
struct BPFMapTraits<IntMap> {
  using outter_key_type = uint32_t;
  using inner_key_type = uint32_t;
  using inner_val_type = uint8_t;
  static constexpr uint32_t outter_key_size = sizeof(uint32_t);
  static constexpr uint32_t inner_key_size = sizeof(inner_key_type);
  static constexpr uint32_t inner_val_size = sizeof(inner_val_type);
  static constexpr int outter_max_entries = ADDR_LPM_MAPS_OUTER_MAX_ENTRIES;
  static constexpr int inner_max_entries = 20;
  static constexpr enum bpf_map_type outter_map_type = BPF_MAP_TYPE_ARRAY_OF_MAPS;
  // BPF_MAP_TYPE_LPM_TRIE
  static constexpr enum bpf_map_type inner_map_type = BPF_MAP_TYPE_HASH;
  static constexpr int map_flag = -1;
};

}
}
