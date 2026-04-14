---
name: security-check
description: Whenever commit or push is mentioned
---
# 安全检查规则

在提交和推送代码前，必须检查是否包含敏感信息，特别是 API Keys 和访问令牌。

## 检查内容

### API Keys 和访问令牌

- `sk-` 开头的 API Keys（如 OpenAI、Anthropic、阿里云等）
- `AIzaSy` 开头的 Google API Keys
- `pk_` 开头的公钥
- 其他常见的 API 令牌格式

## 如果要求Commit/提交，按以下步骤执行

### 先进行检查

使用 `bash scripts/security_check.sh commit` 检查暂存区中是否包含敏感信息。如果没有输出 `staging area is clear` 那说明存在敏感信息。

### 如果存在敏感信息

1. **立即删除或替换**：将真实的 API Key 替换为占位符
2. **使用环境变量**：将敏感信息移到环境变量中
3. **添加到 .gitignore**：确保包含敏感信息的文件不被提交
4. **必须拒绝提交和推送动作**

## 如果要求Push/提交，按以下步骤执行

### 先进行检查

使用 `bash scripts/security_check.sh push` 检查每个 commit 中是否包含敏感信息。如果没有输出 `all commits are clear` 那说明存在敏感信息，包含敏感信息的 commit hash 已被写入 `task/sensitive_commits.txt`。

### 如果存在敏感信息

1. **立即删除或替换**：将真实的 API Key 替换为占位符
2. **使用环境变量**：将敏感信息移到环境变量中
3. **添加到 .gitignore**：确保包含敏感信息的文件不被提交
4. **必须使用下面的脚本清理历史记录**

```bash
# 根据task/sensitive_commits.txt中的检查结果reset，避免commit记录泄密
bash scripts/security_reset.sh
```

5. **必须拒绝提交和推送动作**
