# 容器过滤配置参数说明

本文档描述了容器管理器中用于过滤容器的各种配置参数。

## K8s 相关过滤参数

### K8sNamespaceRegex

对于部署于 K8s 环境的容器，指定待采集容器所在 Pod 所属的命名空间条件。如果未添加该参数，则表示采集所有容器。支持正则匹配。

### K8sPodRegex

对于部署于 K8s 环境的容器，指定待采集容器所在 Pod 的名称条件。如果未添加该参数，则表示采集所有容器。支持正则匹配。

### K8sContainerRegex

对于部署于 K8s 环境的容器，指定待采集容器的名称条件。如果未添加该参数，则表示采集所有容器。支持正则匹配。

## K8s 标签过滤参数

### IncludeK8sLabel

对于部署于 K8s 环境的容器，指定待采集容器所在 pod 的标签条件。多个条件之间为"或"的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。

map 中的 key 为 Pod 标签名，value 为 Pod 标签的值，说明如下：

- 如果 map 中的 value 为空，则 pod 标签中包含以 key 为键的 pod 都会被匹配；
- 如果 map 中的 value 不为空，则：
  - 若 value 以 `^` 开头，则当 pod 标签中存在以 key 为标签名且对应标签值能正则匹配 value 的情况时，相应的 pod 会被匹配；
  - 其他情况下，当 pod 标签中存在以 key 为标签名且以 value 为标签值的情况时，相应的 pod 会被匹配。

### ExcludeK8sLabel

对于部署于 K8s 环境的容器，指定需要排除采集容器所在 pod 的标签条件。多个条件之间为"或"的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。

map 中的 key 为 pod 标签名，value 为 pod 标签的值，说明如下：

- 如果 map 中的 value 为空，则 pod 标签中包含以 key 为键的 pod 都会被匹配；
- 如果 map 中的 value 不为空，则：
  - 若 value 以 `^` 开头，则当 pod 标签中存在以 key 为标签名且对应标签值能正则匹配 value 的情况时，相应的 pod 会被匹配；
  - 其他情况下，当 pod 标签中存在以 key 为标签名且以 value 为标签值的情况时，相应的 pod 会被匹配。

## 环境变量过滤参数

### IncludeEnv

指定待采集容器的环境变量条件。多个条件之间为"或"的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。

map 中的 key 为环境变量名，value 为环境变量的值，说明如下：

- 如果 map 中的 value 为空，则容器环境变量中包含以 key 为键的容器都会被匹配；
- 如果 map 中的 value 不为空，则：
  - 若 value 以 `^` 开头，则当容器环境变量中存在以 key 为环境变量名且对应环境变量值能正则匹配 value 的情况时，相应的容器会被匹配；
  - 其他情况下，当容器环境变量中存在以 key 为环境变量名且以 value 为环境变量值的情况时，相应的容器会被匹配。

### ExcludeEnv

指定需要排除采集容器的环境变量条件。多个条件之间为"或"的关系，如果未添加该参数，则表示采集所有容器。支持正则匹配。

map 中的 key 为环境变量名，value 为环境变量的值，说明如下：

- 如果 map 中的 value 为空，则容器环境变量中包含以 key 为键的容器都会被匹配；
- 如果 map 中的 value 不为空，则：
  - 若 value 以 `^` 开头，则当容器环境变量中存在以 key 为环境变量名且对应环境变量值能正则匹配 value 的情况时，相应的容器会被匹配；
  - 其他情况下，当容器环境变量中存在以 key 为环境变量名且以 value 为环境变量值的情况时，相应的容器会被匹配。

## 容器标签过滤参数

### IncludeContainerLabel

指定待采集容器的标签条件。多个条件之间为"或"的关系，如果未添加该参数，则默认为空，表示采集所有容器。支持正则匹配。

map 中的 key 为容器标签名，value 为容器标签的值，说明如下：

- 如果 map 中的 value 为空，则容器标签中包含以 key 为键的容器都会被匹配；
- 如果 map 中的 value 不为空，则：
  - 若 value 以 `^` 开头，则当容器标签中存在以 key 为标签名且对应标签值能正则匹配 value 的情况时，相应的容器会被匹配；
  - 其他情况下，当容器标签中存在以 key 为标签名且以 value 为标签值的情况时，相应的容器会被匹配。

### ExcludeContainerLabel

指定需要排除采集容器的标签条件。多个条件之间为"或"的关系，如果未添加该参数，则默认为空，表示采集所有容器。支持正则匹配。

map 中的 key 为容器标签名，value 为容器标签的值，说明如下：

- 如果 map 中的 value 为空，则容器标签中包含以 key 为键的容器都会被匹配；
- 如果 map 中的 value 不为空，则：
  - 若 value 以 `^` 开头，则当容器标签中存在以 key 为标签名且对应标签值能正则匹配 value 的情况时，相应的容器会被匹配；
  - 其他情况下，当容器标签中存在以 key 为标签名且以 value 为标签值的情况时，相应的容器会被匹配。

## 测试用例数据格式

容器过滤功能的测试使用 JSON 格式的测试数据文件，包含容器列表和测试用例配置。

### 测试数据文件结构

```json
{
  "containers": [
    // 容器对象数组
  ],
  "test_cases": [
    // 测试用例数组
  ]
}
```

### 容器对象格式

每个容器对象包含以下字段：

- `id`: 字符串，容器的唯一标识符
- `labels`: 对象，容器的标签键值对，例如：`{"app": "nginx", "tier": "frontend"}`
- `env`: 字符串数组，容器的环境变量，格式为 `"KEY=VALUE"`，例如：`["APP_NAME=nginx", "TIER=frontend"]`
- `k8s`: 对象，K8s 相关信息（可选）
  - `namespace`: 字符串，Pod 所属的命名空间
  - `pod`: 字符串，Pod 的名称
  - `container_name`: 字符串，容器名称
  - `labels`: 对象，Pod 的标签键值对，例如：`{"app.kubernetes.io/name": "nginx"}`
  - `paused_container`: 布尔值，是否为暂停容器

### 测试用例对象格式

每个测试用例对象包含以下字段：

- `name`: 字符串，测试用例的名称，用于标识测试场景
- `description`: 字符串，测试用例的详细描述，说明测试的目的和过滤条件
- `filters`: 对象，过滤器配置，使用本文档中描述的过滤参数，例如：

  ```json
  {
    "IncludeContainerLabel": {
      "app": "nginx",
      "tier": "frontend"
    }
  }
  ```

- `expected_matched_ids`: 字符串数组，期望匹配的容器 ID 列表，用于验证过滤结果

### 示例

```json
{
  "containers": [
    {
      "id": "nginx-frontend",
      "labels": {
        "app": "nginx",
        "tier": "frontend"
      },
      "env": ["APP_NAME=nginx", "TIER=frontend"],
      "k8s": {
        "namespace": "default",
        "pod": "nginx-pod-123",
        "container_name": "nginx",
        "labels": {
          "app.kubernetes.io/name": "nginx"
        },
        "paused_container": false
      }
    }
  ],
  "test_cases": [
    {
      "name": "combined_container_filters",
      "description": "Combined container filters: app=nginx AND tier=frontend",
      "filters": {
        "IncludeContainerLabel": {
          "app": "nginx",
          "tier": "frontend"
        }
      },
      "expected_matched_ids": ["nginx-frontend"]
    }
  ]
}
```

## 完整的测试用例列表

为确保容器过滤功能的完整测试覆盖，以下是需要实现的测试用例列表。这些测试用例覆盖了文档中描述的所有过滤参数类型，包括基本匹配、正则匹配、空值匹配、多个条件组合以及各种边界情况。

### K8s 相关过滤参数测试用例

#### K8sNamespaceRegex 测试用例

- `k8s_namespace_exact_match`: 测试命名空间精确匹配
- `k8s_namespace_regex_match`: 测试命名空间正则匹配（如 `^prod-.*`）
- `k8s_namespace_no_match`: 测试命名空间不匹配的情况
- `k8s_namespace_empty_filter`: 测试未配置命名空间过滤时的默认行为

#### K8sPodRegex 测试用例

- `k8s_pod_exact_match`: 测试 Pod 名称精确匹配
- `k8s_pod_regex_match`: 测试 Pod 名称正则匹配（如 `^app-.*-deployment`）
- `k8s_pod_partial_regex`: 测试 Pod 名称部分匹配正则
- `k8s_pod_no_match`: 测试 Pod 名称不匹配的情况

#### K8sContainerRegex 测试用例

- `k8s_container_exact_match`: 测试容器名称精确匹配
- `k8s_container_regex_match`: 测试容器名称正则匹配（如 `^nginx-.*`）
- `k8s_container_multiple_match`: 测试多个容器名称的正则匹配

### K8s 标签过滤参数测试用例

#### IncludeK8sLabel 测试用例

- `include_k8s_label_exact_match`: 测试 Pod 标签精确匹配
- `include_k8s_label_key_only`: 测试只检查标签键存在（value 为空）
- `include_k8s_label_regex_value`: 测试标签值正则匹配（以 ^ 开头）
- `include_k8s_label_multiple_conditions`: 测试多个标签条件（OR 关系）
- `include_k8s_label_partial_match`: 测试部分标签匹配
- `include_k8s_label_no_labels`: 测试容器无标签的情况

#### ExcludeK8sLabel 测试用例

- `exclude_k8s_label_exact_match`: 测试排除特定标签的 Pod
- `exclude_k8s_label_key_only`: 测试排除包含特定键的 Pod（value 为空）
- `exclude_k8s_label_regex_value`: 测试排除标签值正则匹配的 Pod
- `exclude_k8s_label_multiple_conditions`: 测试多个排除条件（OR 关系）
- `exclude_k8s_label_combined_with_include`: 测试排除和包含标签的组合使用

### 环境变量过滤参数测试用例

#### IncludeEnv 测试用例

- `include_env_exact_match`: 测试环境变量精确匹配
- `include_env_key_only`: 测试只检查环境变量键存在（value 为空）
- `include_env_regex_value`: 测试环境变量值正则匹配（以 ^ 开头）
- `include_env_multiple_variables`: 测试多个环境变量条件（OR 关系）
- `include_env_case_sensitive`: 测试环境变量大小写敏感性
- `include_env_special_characters`: 测试包含特殊字符的环境变量

#### ExcludeEnv 测试用例

- `exclude_env_exact_match`: 测试排除特定环境变量的容器
- `exclude_env_key_only`: 测试排除包含特定环境变量键的容器
- `exclude_env_regex_value`: 测试排除环境变量值正则匹配的容器
- `exclude_env_multiple_conditions`: 测试多个排除环境变量条件
- `exclude_env_combined_with_include`: 测试排除和包含环境变量的组合

### 容器标签过滤参数测试用例

#### IncludeContainerLabel 测试用例

- `include_container_label_exact_match`: 测试容器标签精确匹配
- `include_container_label_key_only`: 测试只检查容器标签键存在（value 为空）
- `include_container_label_regex_value`: 测试容器标签值正则匹配（以 ^ 开头）
- `include_container_label_multiple_labels`: 测试多个容器标签条件（OR 关系）
- `include_container_label_numeric_values`: 测试数值类型的标签值
- `include_container_label_empty_labels`: 测试容器无标签的情况

#### ExcludeContainerLabel 测试用例

- `exclude_container_label_exact_match`: 测试排除特定容器标签
- `exclude_container_label_key_only`: 测试排除包含特定标签键的容器
- `exclude_container_label_regex_value`: 测试排除标签值正则匹配的容器
- `exclude_container_label_multiple_conditions`: 测试多个排除容器标签条件
- `exclude_container_label_combined_with_include`: 测试排除和包含容器标签的组合

### 组合过滤测试用例

- `combined_k8s_filters`: 测试 K8s 命名空间、Pod、容器名称的组合过滤
- `combined_label_filters`: 测试 K8s 标签和容器标签的组合过滤
- `combined_env_and_label_filters`: 测试环境变量和标签的组合过滤
- `all_filters_combined`: 测试所有类型过滤参数的完整组合
- `complex_regex_filters`: 测试多个正则表达式的复杂组合
- `mixed_include_exclude_filters`: 测试包含和排除过滤器的混合使用

### 边界情况和异常测试用例

- `empty_value_matching`: 测试空值匹配的各种情况
- `invalid_regex_patterns`: 测试无效正则表达式的情况
- `case_sensitivity_tests`: 测试大小写敏感性
- `special_characters_in_filters`: 测试过滤器中的特殊字符处理
- `maximum_conditions_test`: 测试大量过滤条件的性能和正确性
- `conflicting_filters_test`: 测试相互冲突的过滤条件
- `null_undefined_values`: 测试 null 和 undefined 值的情况
