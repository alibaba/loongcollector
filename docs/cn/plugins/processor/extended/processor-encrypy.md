# 字段加密

## 简介

`processor_encrypt`插件可以使用 AES-128、AES-192、AES-256 算法对指定的字段加密，采用 PKCS7 填充算法。**由于密文可能包含不可见字符，插件会对其进行十六进制编码。**

## 版本

[Stable](../../stability-level.md)

### 参数说明

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Type                      | String，无默认值(必填)                | 插件类型，固定为`processor_encrypt` |
| SourceKeys                | String[]，无默认值(必填)              | 指定需要加密的字段名，支持指定多个。 |
| EncryptionParameters      | EncryptionParameter，无默认值(必填)   | 指定加密参数。 |
| KeepSourceValueIfError    | Boolean，`false`                    | 如果发生加密失败，是否保留原始值，如果不保留的话，字段值会被替换为 `ENCRYPT_ERROR`。为了保护您的数据安全，默认值为 `false`，即失败时进行替换。|

* EncryptionParameter 类型说明

| 参数 | 类型，默认值 | 说明 |
| - | - | - |
| Key           | String，无默认值         | 指定密钥（十六进制），未配置 KeyFilePath 时必选。AES-128、AES-192、AES-256 分别要求密钥为 16、24、32 字节，所以直接以此参数指定密钥时，需要填入 32、48、64 个十六进制字符。 |
| IV            | String，默认全`0`        | 指定加密的初始向量（十六进制），若配置了KeyFilePath可不填写。此参数值大小等于内部 block 大小，AES-128、AES-192、AES-256 下分别为 8、12、16 字节，所以需要填入 16、24、32 个十六进制字符。 |
| KeyFilePath   | String，无默认值         | 指定保存加密参数的文件路径，未配置 Key 时必选。支持将前述参数（Key、IV）保存至文件（JSON 格式），文件中指定的值会覆盖通过前述参数指定的值。 |

## 样例

### 示例 1：直接指定 Key 且使用默认 IV

对字段 `important` 进行加密，假设密钥（Key）为二进制全 0 组成（256 bits）、不设置初始向量（IV），使用插件默认值，则配置详情及处理结果如下：

* 采集配置

```Yaml
processors:
  - Type: processor_encrypt
    SourceKeys:
      - "important"
    EncryptionParameters: 
      Key: "0000000000000000000000000000000000000000000000000000000000000000"
```

* 输入

```Json
"normal_key": "1",
"another_normal_key": "2",
"important": "0123456"
```

* 输出（由于加密后内容包含不可见字符，加密后会将其转换为十六进制）

```Json
"normal_key": "1",
"another_normal_key": "2",
"important": "bc3acdbd40c283d91f7dc7010fd7d2b1"
```

* 利用 openssl 进行解密

```shell
$ printf "%b" '\xbc\x3a\xcd\xbd\x40\xc2\x83\xd9\x1f\x7d\xc7\x01\x0f\xd7\xd2\xb1' > ciphertext
$ openssl enc -d -aes-256-cbc -iv 00000000000000000000000000000000 \
    -K 0000000000000000000000000000000000000000000000000000000000000000 \
    -in ciphertext -out plaintext
$ cat plaintext
0123456
```

### 示例 2：直接指定 Key 和 IV

对字段 `important` 进行加密，假设密钥（Key）为二进制全 0 组成（256 bits）、初始向量（IV）为二进制全 1 组成（128 bits），则配置详情及处理结果如下：

* 输入

```Json
"normal_key": "1",
"another_normal_key": "2",
"important": "0123456"
```

* 配置详情（注意 Key/IV 都是十六进制表示）

```Yaml
processors:
  - Type: processor_encrypt
    SourceKeys:
      - "important"
    EncryptionParameters: 
      Key: "0000000000000000000000000000000000000000000000000000000000000000"
      IV: "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
```

* 输出（由于加密后内容包含不可见字符，加密后会将其转换为十六进制）

```Json
"normal_key": "1",
"another_normal_key": "2",
"important": "af6428af62d698be617b82cffd9e109b"
```

* 利用 openssl 进行解密

```shell
$ printf "%b" '\xaf\x64\x28\xaf\x62\xd6\x98\xbe\x61\x7b\x82\xcf\xfd\x9e\x10\x9b' > ciphertext
$ openssl enc -d -aes-256-cbc -iv FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF \
    -K 0000000000000000000000000000000000000000000000000000000000000000 \
    -in ciphertext -out plaintext
$ cat plaintext
0123456
```

### 示例 3：通过文件指定 Key

对字段 `important` 进行加密，假设密钥（Key）为二进制全 1 组成（256 bits），密钥保存在机器的本地文件 `/home/admin/aes_key` 中，文件内容中未指定 IV 参数，使用插件默认值，配置详情及处理结果如下：

* 前置条件

运行 logtail 的机器上需要创建文件 `/home/admin/aes_key.json`（JSON 格式），并将密钥以十六进制存放在其中，命令如下：

```shell
printf "{\"Key\": \"FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\"}" > /home/admin/aes_key.json
```

* 输入

```Json
"normal_key": "1",
"another_normal_key": "2",
"important": "0123456"
```

* 配置详情

```Yaml
processors:
  - Type: processor_encrypt
    SourceKeys:
      - "important"
    EncryptionParameters: 
      KeyFilePath: "/home/admin/aes_key.json"
```

* 输出

```Json
"normal_key": "1",
"another_normal_key": "2",
"important": "bc3acdbd40c283d91f7dc7010fd7d2b1"
```

* 利用 openssl 进行解密

```shell
$ printf "%b" '\xbc\x3a\xcd\xbd\x40\xc2\x83\xd9\x1f\x7d\xc7\x01\x0f\xd7\xd2\xb1' > ciphertext
$ openssl enc -d -aes-256-cbc -iv 00000000000000000000000000000000 \
    -K 0000000000000000000000000000000000000000000000000000000000000000 \
    -in ciphertext -out plaintext
$ cat plaintext
0123456
```
