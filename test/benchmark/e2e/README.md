# LoongCollector Benchmark Report

## Rigorous Test Methodology
- **Hardware**: Alibaba Cloud ECS g7 (32 vCPU, 64GB RAM) - enterprise-grade specification
- **OS**: Ubuntu 20.02 with ext4 filesystem  
- **Reproducibility**: Full benchmark suite available on GitHub with automated scripts
- **Disk**: ESSD PL3 1500GiB (76800 IOPS)

## Test Data

**Single Line**
```
203.0.113.45 - - [25/Jun/2024:23:59:59 +0000] "GET /wp-admin/admin-ajax.php?action=revslider_ajax_action&client_action=get_facebook HTTP/1.1" 200 1847 "https://www.google.com/search?q=free+piano+sheet+music+pdf+download+site%3Aexample.com&ref=lnms&sa=X&biw=1920&bih=1080" "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko)" rt=0.312 uct="0.001" uht="0.125" urt="0.311" sid=a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0 uagent_hash=7d8f9a0b1c2d3e4f5a6b7c8d9e0f1a2b3c4d5e6f7 request_id=req-20240625-235959-001
```

**Multi-line**
```
[2024-01-04T14:36:10.942119596]     [ERROR] java.lang.Exception: exception happened
  at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f5(RegexMultiLog.java:143)
  at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f4(RegexMultiLog.java:139)
  at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f3(RegexMultiLog.java:139)
  at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f2(RegexMultiLog.java:139)
  at com.aliyun.sls.devops.logGenerator.type.RegexMultiLog.f1(RegexMultiLog.java:139)
```

**Regex Parsing**

Same as **Single Line**

## Maximum Throughput

| Log Type | LoongCollector | FluentBit | Vector | Filebeat |
|----------|----------------|-----------|--------|----------|
| Single Line | **546 MB/s** | 36 MB/s | 38 MB/s | 9 MB/s |
| Multi-line | **238 MB/s** | 24 MB/s | 22 MB/s | 6 MB/s |
| Regex Parsing | **68 MB/s** | 19 MB/s | 12 MB/s | Not Supported |

## Resource Efficiency

![Resource Efficiency Comparison](https://ilogtail-community-edition.oss-cn-shanghai.aliyuncs.com/images/benchmark/resources.png)

| Scenario | LoongCollector | FluentBit | Vector | Filebeat |
|----------|----------------|-----------|--------|----------|
| **Simple Line (512B)** | 3.40% CPU<br>29.01 MB RAM | 12.29% CPU (+261%)<br>46.84 MB RAM (+61%) | 35.80% CPU (+952%)<br>83.24 MB RAM (+186%) | **Performance Insufficient** |
| **Multi-line (512B)** | 5.82% CPU<br>29.39 MB RAM | 28.35% CPU (+387%)<br>46.39 MB RAM (+57%) | 55.99% CPU (+862%)<br>85.17 MB RAM (+189%) | **Performance Insufficient** |
| **Regex (512B)** | 14.20% CPU<br>34.02 MB RAM | 37.32% CPU (+162%)<br>46.44 MB RAM (+36%) | 43.90% CPU (+209%)<br>90.51 MB RAM (+166%) | **Not Supported** |