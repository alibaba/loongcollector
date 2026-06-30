---
name: security-check
description: 提交或推送前的安全检查，扫描 API Key、访问令牌等敏感信息。
---
# 安全检查

在提交和推送代码前，必须检查是否包含敏感信息，特别是 API Keys 和访问令牌。

## 检查内容

### API Keys 和访问令牌

- `sk-` 开头的 API Keys（如 OpenAI、Anthropic、阿里云等）
- `AIzaSy` 开头的 Google API Keys
- `pk_` 开头的公钥
- 其他常见的 API 令牌格式

## 提交前（Commit）

### 先进行检查

```bash
bash skills/security-check/scripts/security_check.sh commit
```

若未输出 `staging area is clear`，说明暂存区存在敏感信息。

### 若存在敏感信息

1. **立即删除或替换**：将真实 API Key 替换为占位符
2. **使用环境变量**：将敏感信息移到环境变量中
3. **添加到 .gitignore**：确保包含敏感信息的文件不被提交
4. **必须拒绝提交和推送动作**

## 推送前（Push）

### 先进行检查

```bash
bash skills/security-check/scripts/security_check.sh push
```

若未输出 `all commits are clear`，说明存在敏感信息，相关 commit hash 已写入 `task/sensitive_commits.txt`。

### 若存在敏感信息

1. **立即删除或替换**：将真实 API Key 替换为占位符
2. **使用环境变量**：将敏感信息移到环境变量中
3. **添加到 .gitignore**：确保包含敏感信息的文件不被提交
4. **必须使用下面的脚本清理历史记录**

```bash
# 根据 task/sensitive_commits.txt 中的检查结果 reset，避免 commit 记录泄密
bash skills/security-check/scripts/security_reset.sh
```

5. **必须拒绝提交和推送动作**
