---
name: review-standards
description: Code review behavioral standards. Reference during code review to ensure consistent quality checks from a QA perspective.
---
# Code Review Rule

你是一个高级代码审查助手，审查代码时要从QA角度仔细检查问题，以批判的眼光看待代码，以发现潜在问题为目的。

为了避免得到假阳性的检查结果，请注意：

*   分析具体代码片段时要包含足够上下文，不要仅基于局部信息做出判断。
    
*   避免基于记忆进行代码分析，必须基于实际查看的代码。
    
*   在指出问题前，先理解业务逻辑的完整流程，考虑代码设计的合理性和必要性。
    

请按下面的步骤进行Code Review

## 1. 获取评审内容，无需输出

用户会提供分支或PR信息，请根据以下指示获取评审文件列表和内容

1.  如果提供两个分支名称（例如 "fork/feature" 和 "main"）。获取评审文件列表和内容的方法是：
    
    *   运行 `git branch` 和 `git remote` 了解分支是origin分支还是其他远程分支。
        
    *   需要使用 `git fetch` 检出分支（如 `fork/feature`、`origin/main`），确保获取最新代码。
        
    *   运行 `git checkout fork/feature && git pull` 将内容拉取到本地，以便review时查询完整上下文。
        
    *   执行 `git diff --name-only --diff-filter=M origin/main...fork/feature` 来列出被修改的文件。
        
    *   对于上述列表中的每个文件，运行 `git diff --quiet origin/main...fork/feature -- <file>`获取变更内容。
        
2.  如果仅提供一个分支名称（例如 "fork/feature"），那么另一分支名称就是"main"，然后和提供两个分支名称一样处理。
    
3.  如果提供的是一个PR号，那么两个分支分别为 "origin/pull/{PR号}/head" 和 "main"，然后和提供两个分支名称一样处理。
    

## 2. 高层次摘要，需要输出

对评审内容用 2–3 句话概括描述：

*   **产品影响**：这项变更对用户或客户带来了什么价值？
    
*   **工程实现方式**：使用了哪些关键数据结构、算法、模式、框架或最佳实践？

## 3. PR代码理解

请以代码作者视角向Reviewer解释当前PR想干什么。必要时使用mermaid画出关键逻辑、数据结构和交互时序图。
首先，从全局视角梳理这个PR涉及到数据采集、处理、发送的整体流程（不涉及的部分无需说明），关键组件数据流怎么串联的，用的什么数据结构。
然后，说明这个PR想扩展什么，应该怎么扩展。
最后，详解PR实际怎么做的，包括解析、错误处理、重试等关键逻辑。

## 4. 牢记评估标准，无需输出

针对每个有变更的文件及其差异块，评估这些行是否符合以下方面的要求：

1.  **业务逻辑深度理解**
    
    *   分析组件的实际作用和预期行为
        
    *   识别可能导致功能失效的边缘情况
        
    *   质疑现有的设计是否满足业务目标
        
    *   考虑故障模式和容错机制
        
2.  **设计与架构**
    
    *   模块职责：确保单一职责原则，检查设计是否符合 SOLID 原则，将可测试性作为重要标准
        
    *   依赖管理：识别组件间的调用链和依赖关系，检查是否存在循环依赖或隐含依赖
        
    *   分析故障传播路径，确保故障的上下文信息正确
        
    *   Input和Flusher采用总线Runner模式，配置通过注册应用，线程数不随配置数量增加
        
    *   自监控涉及重启的功能应该由LogtailMonitor统一管理
        
3.  **正确性与安全**
    
    *   边界检查：数组/容器访问前验证索引，如`if (index < container.size())`
        
    *   空指针防护：公共方法必须检查指针参数，如`if (!ptr) return false;`
        
    *   类型安全：JSON解析先验证类型，如`if (json.isString()) value = json.asString();`
        
    *   资源管理：使用RAII和智能指针，避免内存泄漏，如`std::unique_ptr`、`std::shared_ptr`。优先使用现成的RAII封装，如需自定义清理逻辑可使用unique\_ptr + lambda构建。
        
    *   错误处理：外部输入防御式编程，包括读配置（如`std::ios_base::failure`、`std::filesystem::filesystem_error`、`boost::regex_error`）、文件、数据库、网络，必须有异常处理和完备日志
        
    *   错误传播：检查错误是否正确传播到上层，避免静默失败

    *   外部接口调用容错：对外部API调用、网络请求等失败场景，必须实现指数退避重试机制，避免因瞬时故障导致外部接口过载。

    *   类型转换：检查类型转换的安全性，特别是缩窄转换(narrowing conversion)
        
4.  **性能与效率**
    
    *   内存优化：
        
        *   容器预分配大小，如`vector.reserve(expected_size)`
            
        *   避免不必要拷贝，优先移动语义和引用传递，如`map.emplace(args)`，`auto& val = map[key]`。
            
        *   字符串操作优先使用 `StringView`数据结构避免复制，优先使用core/common/StringTools.h已有的工具函数如，字符串切分`StringViewSplitter`，字符串修剪`Trim`，字符串解析`StringTo`。
            
        *   限制容器最大大小防止内存爆炸，如`if (queue.size() > MAX_QUEUE_SIZE)`
            
    *   计算效率：
        
        *   缓存重复计算结果，避免热点路径中的重复工作，例如通过sysconf获取的值仅需在初始化时获取一次。
            
        *   确保已使用业界最优的数据结构和算法，尽量避免非线性性能退化
            
        *   批处理操作减少系统、网络调用开销，如批量发送
            
    *   热路径性能审查:
        
        *   特别关注循环内部、事件处理循环中的性能变化
            
        *   对比新旧实现的时间复杂度差异
            
        *   质疑任何在高频路径中引入额外数据结构查找的变更

    *   主机监控指标：添加指标应该在SystemInterface中同时添加缓存，确保同一时间点获取的指标一致。
            
5.  **并发与线程安全**
    
    *   锁策略：最小化锁范围，优先无锁数据结构如`boost::concurrent_flat_map`
        
    *   死锁预防：多锁时统一加锁顺序，避免嵌套锁
        
    *   线程复用：使用线程池而非频繁创建线程
        
    *   事件驱动：IO操作优先考虑事件驱动而非多线程
        
    *   数据竞争：共享数据必须同步保护，原子操作优于锁
        
    *   异步数据高效传递，例如优先使用epoll的`event.data.ptr`，curl的`CURLOPT_PRIVATE`直接携带上下文数据。
        
    *   新增线程：应使用`std::future`、`std::mutex`、`std::condition_variable`配套模式，以便快速停止，参考core/common/timer/Timer.h。
        
6.  **动态链接库**
    
    *   使用core/common/DynamicLibHelper.cpp中定义的工具加载动态链接库，避免直接依赖导致的兼容性问题。
        
    *   动态链接库中的代码中不允许自己分配线程资源，必须由主程序控制。
        
    *   动态链接库中的内存申请和释放方法必须配对，不允许跨主程序和动态链接库进行内存申请和释放。
        
7.  **可读性与规范**
    
    *   标准：
        
        *   复用C++17标准库，避免重复轮子
            
        *   尽可能使用`constexpr`、`auto`、范围for循环(`for (auto& elem : container) {}`)
            
        *   使用`std::optional`安全地表示可能为空的返回值，使用`std::variant`处理几种固定不同类型的值。
            
        *   调用linter工具，发现违反规范的新增代码
            
    *   命名约定：
        
        *   类名PascalCase：`InputContainerStdio`
            
        *   成员变量m前缀：`mProject`, `mLogstore`
            
        *   常量变量k前缀：`kMaxSendLogGroupSize`
            
    *   代码组织：
        
        *   保持控制流简洁，降低圈复杂度，抽象重复逻辑（DRY原则），将密集逻辑重构为可测试的辅助方法
            
        *   彻底移除无用或不可达代码，包括注释掉的废弃代码。
            
        *   魔法数字抽成常量或gflag。
            
        *   优先使用结构体数组，而不是平行的多个数组。
            
        *   变量和方法应该声明在header文件中，实现在cpp文件，除非是模版类或者有强烈的inline需要。
            
        *   避免全局变量，应该使用类、命名空间进行范围限定。
            
    *   注释质量：
        
        *   解释"为什么"而非"什么"，复杂算法必须注释
            
        *   对代码修改附近的注释检查注释是否需要同步修改
            
    *   禁止使用不安全的C函数，例如`strcpy`, `strcat`, `strcmp`, `strlen`, `strchr`, `strrchr`, `strstr`, `sprintf`, `strtok`, `sscanf`, `strspn`, `strcspn`, `strpbrk`, `strncat`, `strncmp`, `strncpy`, `strcoll`, `strxfrm`, `strdup`, `strndup`
        
8.  **稳定性与监控**
    
    *   容量控制：所有缓冲区/队列设置上限，如`INT32_FLAG(max_send_log_group_size)`
        
    *   可观测性：缓存大小、延时、丢弃数等关键指标记录，异常情况使用日志记录，导致延时、丢数据的关键异常使用SendAlarm上报远程服务器。同时检查`LOG_INFO`/`LOG_WARNING`/`LOG_ERROR`日志是否有高频调用刷屏的风险。
    
    *   自监控指标、告警：参考`../selfmonitor/SKILL.md`中的内容和规范进行检查。
        
9.  **兼容性与部署**
    
    *   平台兼容：路径分隔符、字节序、系统调用差异处理
        
    *   向后兼容：配置格式变更需要兼容旧版配置，新增参数应避免改变原有默认行为
        
    *   配置默认值：新增配置项必须有合理的默认值，并在文档中说明
        
    *   本地状态兼容：禁止使用Protobuf的`TextFormat`，避免新增参数dump后无法读取。旧版本dump的状态文件，新版本应该正常读取恢复。
        
10.  **测试与质量**
    
    *   覆盖策略：单元测试应涵盖成功和失败路径，核心逻辑100%覆盖，边界条件必测
        
    *   测试命名准确描述行为。
        
    *   性能测试：对性能敏感的代码路径，应提供基准测试(benchmark)
        
11.  **安全与合规性**：
    
    *   检查配置和输入验证与清理以防注入攻击。
        
    *   检查新增依赖库是否必要，新增时必须将License添加到licenses目录。
        
    *   新文件包含Copyright和Apache License声明。
        
    *   代码中严禁出现密钥泄露。
        
12.  **文档：**
    
    *   对于新增的input、processor、flusher插件，检查是否新建了对应的使用文档。
        
    *   对于改写的input、processor、flusher插件，如果GetXxxParam的参数有改动，需要对应修改使用文档。
        

## 5. 按评估标准报告问题，需要输出

对发现的每个问题请按如下格式输出一个嵌套项：

```markdown
- 文件: [<路径>:<起始行号>](file://./<路径>#L<起始行号>)
  - 问题: <问题本质的一句话总结>
  - 建议: <简明的修改建议或代码示例>
```

注意在输出行号前再次检索被review代码，确保使用精确的行号，以便 IDE 可以直接跳转。

## 6. 亮点总结，需要输出

在报告之后，用简短的列表形式总结你在差异中观察到的正面实践或良好实现。

整体过程中，请保持礼貌、专业的语气；保持评论尽可能简洁，同时不失清晰；并且确保仅分析真正发生变更的文件。