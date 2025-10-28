/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "JournalServer.h"

#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <utility>

#include "app_config/AppConfig.h"
#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "connection/JournalConnectionManager.h"
#include "logger/Logger.h"
#include "processor/JournalEntryProcessor.h"
#include "reader/JournalReader.h"


using namespace std;

namespace logtail {

void JournalServer::Init() {
    bool expected = false;
    if (!mIsInitialized.compare_exchange_strong(expected, true)) {
        LOG_INFO(sLogger, ("journal server already initialized", "skipping duplicate Init() call"));
        return;
    }

    mThreadRes = async(launch::async, &JournalServer::run, this);

    // 初始化连接管理器
    JournalConnectionManager::GetInstance().Initialize();

    LOG_INFO(sLogger, ("journal server initialized", ""));
}

void JournalServer::Stop() {
    bool expected = true;
    if (!mIsInitialized.compare_exchange_strong(expected, false)) {
        return;
    }

    if (!mThreadRes.valid()) {
        return;
    }
    // 设置停止标志·
    mIsThreadRunning.store(false);
    // 等待线程退出
    mThreadRes.get();

    // 清理连接管理器
    JournalConnectionManager::GetInstance().Cleanup();

    LOG_INFO(sLogger, ("journal server stopped", ""));
}

bool JournalServer::HasRegisteredPlugins() const {
    return JournalConnectionManager::GetInstance().GetConnectionCount() > 0;
}

void JournalServer::AddJournalInput(const string& configName, const JournalConfig& config) {
    // 首先验证配置并获取队列键
    QueueKey queueKey = 0;
    if (!validateQueueKey(configName, config, queueKey)) {
        LOG_ERROR(sLogger, ("journal server input validation failed", "config not added")("config", configName));
        return;
    }

    // 验证成功后，缓存queueKey并添加配置
    JournalConfig validatedConfig = config;
    validatedConfig.mQueueKey = queueKey;

    LOG_INFO(sLogger,
             ("journal server input validated",
              "")("config", configName)("ctx_valid", config.mCtx != nullptr)("queue_key", queueKey));

    // 使用配置管理器添加配置
    auto connectionManager = &JournalConnectionManager::GetInstance();

    if (connectionManager->AddConfig(configName, validatedConfig)) {
        LOG_INFO(sLogger, ("journal server config added to manager", "")("config", configName));

        // 记录统计信息
        auto stats = connectionManager->GetStats();
        LOG_INFO(sLogger,
                 ("journal server manager stats", "")("total_configs", stats.totalConfigs)("active_connections",
                                                                                           stats.activeConnections));
    } else {
        LOG_ERROR(sLogger, ("journal server failed to add config to manager", "")("config", configName));
    }
}

void JournalServer::RemoveJournalInput(const string& configName) {
    // 清理 epoll 监控
    CleanupEpollMonitoring(configName);

    // 移除config对应的连接（同时会移除配置）
    JournalConnectionManager::GetInstance().RemoveConfig(configName);

    LOG_INFO(sLogger, ("journal server input removed with automatic connection cleanup", "")("config", configName));
}

void JournalServer::RemoveConfigOnly(const string& configName) {
    // 移除config对应的连接（不清理 epoll）
    JournalConnectionManager::GetInstance().RemoveConfig(configName);

    LOG_INFO(sLogger, ("journal server input removed without cleanup", "")("config", configName));
}

std::map<std::string, JournalConfig> JournalServer::GetAllJournalConfigs() const {
    return JournalConnectionManager::GetInstance().GetAllConfigs();
}

JournalServer::ConnectionPoolStats JournalServer::GetConnectionPoolStats() const {
    auto stats = JournalConnectionManager::GetInstance().GetStats();
    ConnectionPoolStats result;
    result.totalConnections = stats.totalConnections;
    result.activeConnections = stats.activeConnections;
    result.invalidConnections = stats.invalidConnections;
    result.connectionKeys = stats.connectionKeys;
    return result;
}

std::shared_ptr<SystemdJournalReader> JournalServer::GetConnectionInfo(const std::string& configName) const {
    return JournalConnectionManager::GetInstance().GetConnection(configName);
}

size_t JournalServer::GetConnectionCount() const {
    return JournalConnectionManager::GetInstance().GetConnectionCount();
}

int JournalServer::GetGlobalEpollFD() const {
    std::lock_guard<std::mutex> lock(mEpollMutex);
    return mGlobalEpollFD;
}

void JournalServer::CleanupEpollMonitoring(const std::string& configName) {
    // 检查初始化状态：如果 JournalServer 已经停止，说明 epoll 已在 run() 中统一清理
    if (!mIsInitialized.load()) {
        LOG_DEBUG(sLogger,
                  ("journal server epoll monitoring already cleaned up", "server stopped")("config", configName));
        return;
    }

    int epollFD = GetGlobalEpollFD();
    if (epollFD < 0) {
        LOG_WARNING(sLogger,
                    ("journal server cannot cleanup epoll monitoring", "epoll not initialized")("config", configName));
        return;
    }

    auto reader = JournalConnectionManager::GetInstance().GetConnection(configName);
    if (reader && reader->IsOpen()) {
        // Cleaning up epoll monitoring for config: configName[idx]
        reader->RemoveFromEpoll(epollFD);
    }
}

void JournalServer::run() {
    LOG_INFO(sLogger, ("journal server event-driven thread", "started"));

#ifdef __linux__
    // 检测是否在容器环境中采集主机的journal
    bool isContainerMode = AppConfig::GetInstance()->IsPurageContainerMode();
    LOG_INFO(sLogger, ("journal server container mode", isContainerMode));

    // 存储已监听的 reader 及其对应的配置信息
    std::map<int, MonitoredReader> monitoredReaders;

    // 如果是在容器环境中采集主机journal，由于inotify绑定到systemd namespace，
    // epoll会一直返回0，因此直接走轮询模式
    if (isContainerMode) {
        LOG_INFO(sLogger, ("journal server using polling mode", "container environment detected"));
        runInPollingMode(monitoredReaders);
        LOG_INFO(sLogger, ("journal server polling thread", "stopped"));
        return;
    }

    // 非容器环境：使用epoll模式
    // 创建全局 epoll 实例
    {
        std::lock_guard<std::mutex> lock(mEpollMutex);
        mGlobalEpollFD = epoll_create1(EPOLL_CLOEXEC);
        if (mGlobalEpollFD == -1) {
            LOG_ERROR(sLogger, ("journal server failed to create epoll", "")("error", strerror(errno))("errno", errno));
            return;
        }
    }

    constexpr int kMaxEvents = 64;
    LOG_INFO(
        sLogger,
        ("journal server global epoll instance created", "")("epoll_fd", mGlobalEpollFD)("max_events", kMaxEvents));

    struct epoll_event events[kMaxEvents];

    while (mIsThreadRunning.load()) {
        try {
            // 更新连接监听状态
            refreshMonitors(mGlobalEpollFD, monitoredReaders);

            // 等待事件
            int nfds = epoll_wait(mGlobalEpollFD, events, kMaxEvents, kJournalEpollTimeoutMS);

            if (nfds == -1) {
                if (errno == EINTR) {
                    continue; // 被信号中断，继续等待
                }
                LOG_ERROR(sLogger, ("journal server epoll_wait failed", "")("error", strerror(errno)));
                break;
            }

            LOG_INFO(sLogger, ("journal server epoll_wait events", nfds));

            // 🔥 兜底逻辑：用于处理hasMoreData且epoll=0的批处理没处理完的场景
            // 当epoll_wait超时返回0事件，但某些reader仍有hasMoreData标志时，
            // 说明上次批处理可能还有数据未读完，需要再次尝试读取
            if (nfds == 0 && handlePendingDataReaders(monitoredReaders)) {
                continue; // 继续下一次epoll_wait
            }

            // 处理文件描述符事件
            for (int i = 0; i < nfds; i++) {
                int fd = events[i].data.fd;
                // Received epoll event for fd
                auto it = monitoredReaders.find(fd);
                if (it != monitoredReaders.end()) {
                    auto& monitoredReader = it->second;

                    if (!monitoredReader.reader) {
                        continue;
                    }

                    JournalEventType eventType = monitoredReader.reader->ProcessJournalEvent();

                    // 优化：如果是 NOP 且上次已经读到 EndOfJournal，就跳过读取
                    if (eventType == JournalEventType::kNop && !monitoredReader.hasMoreData) {
                        continue; // 跳过无效读取
                    }

                    if (eventType != JournalEventType::kError) {
                        // 正常事件（NOP/APPEND/INVALIDATE），处理该配置的journal事件
                        bool hasMoreData = false;
                        processJournal(monitoredReader.configName, &hasMoreData);

                        // 更新状态：记录是否还有更多数据可读
                        monitoredReader.hasMoreData = hasMoreData;
                    } else {
                        // 错误情况
                        LOG_WARNING(sLogger,
                                    ("journal server ProcessJournalEvent failed",
                                     "sd_journal_process returned error, skipping this event")(
                                        "config", monitoredReader.configName)("fd", fd));
                        monitoredReader.hasMoreData = false;
                    }
                }
                // Note: Unknown fd in epoll event (might be already cleaned up)
            }

        } catch (const exception& e) {
            LOG_ERROR(sLogger, ("journal server exception in event loop", e.what()));
            this_thread::sleep_for(chrono::milliseconds(1000)); // 异常时等待1秒
        } catch (...) {
            LOG_ERROR(sLogger, ("journal server unknown exception in event loop", ""));
            this_thread::sleep_for(chrono::milliseconds(1000)); // 异常时等待1秒
        }
    }

    // 清理所有 reader 的 epoll 监控
    LOG_INFO(sLogger,
             ("journal server cleaning up epoll monitoring", "")("monitored_readers", monitoredReaders.size()));
    for (auto& pair : monitoredReaders) {
        if (pair.second.reader) {
            // Removing reader from epoll: config[idx], fd
            pair.second.reader->RemoveFromEpoll(mGlobalEpollFD);
        }
    }

    {
        std::lock_guard<std::mutex> lock(mEpollMutex);
        close(mGlobalEpollFD);
        mGlobalEpollFD = -1;
    }
    LOG_INFO(sLogger, ("journal server event-driven thread", "stopped"));
#endif
}

void JournalServer::refreshMonitors(int epollFD, std::map<int, MonitoredReader>& monitoredReaders) {
    // 获取当前配置
    auto allConfigs = GetAllJournalConfigs();

    // 判断是否为epoll模式
    bool isEpollMode = (epollFD >= 0);

    // 检查需要添加监听的配置
    for (const auto& [configName, config] : allConfigs) {
        auto connection = JournalConnectionManager::GetInstance().GetConnection(configName);
        if (connection && connection->IsOpen()) {
            // 检查是否已经监听
            bool alreadyMonitored = false;
            for (const auto& pair : monitoredReaders) {
                if (pair.second.reader == connection) {
                    alreadyMonitored = true;
                    break;
                }
            }

            if (!alreadyMonitored) {
                // 检查 reader 状态
                if (!connection->IsOpen()) {
                    LOG_WARNING(sLogger, ("journal server reader is not open", "")("config", configName));
                    continue;
                }

                // 检查 journal fd
                int journalFD = connection->GetJournalFD();
                if (journalFD < 0) {
                    LOG_WARNING(
                        sLogger,
                        ("journal server fd is invalid", "")("config", configName)("fd", journalFD)("errno", errno));
                    continue;
                }

                // epoll模式：添加到epoll；轮询模式：直接添加
                bool success = true;
                if (isEpollMode) {
                    success = connection->AddToEpoll(epollFD);
                    if (!success) {
                        LOG_WARNING(sLogger,
                                    ("journal server failed to add reader to epoll",
                                     "")("config", configName)("fd", journalFD)("epoll_fd", epollFD));
                    }
                }

                if (success) {
                    MonitoredReader monitoredReader;
                    monitoredReader.reader = connection;
                    monitoredReader.configName = configName;
                    monitoredReaders[journalFD] = monitoredReader;

                    if (isEpollMode) {
                        LOG_INFO(sLogger,
                                 ("journal server reader added to epoll monitoring", "")("config",
                                                                                         configName)("fd", journalFD));
                    } else {
                        LOG_INFO(sLogger,
                                 ("journal server reader added to monitoring (polling mode)",
                                  "")("config", configName)("fd", journalFD));
                    }
                }
            }
        }
    }
}

void logtail::JournalServer::processJournal(const std::string& configName, bool* hasMoreDataOut) {
    // 从 JournalConnectionManager 获取配置
    JournalConfig config = JournalConnectionManager::GetInstance().GetConfig(configName);

    if (config.mQueueKey == -1) {
        LOG_ERROR(sLogger, ("journal server invalid config for specific processing", "")("config", configName));
        if (hasMoreDataOut)
            *hasMoreDataOut = false;
        return;
    }

    // 读取和处理journal条目
    auto connection = JournalConnectionManager::GetInstance().GetConnection(configName);
    if (!connection || !connection->IsOpen()) {
        LOG_ERROR(sLogger, ("journal server connection not available for event processing", "")("config", configName));
        if (hasMoreDataOut)
            *hasMoreDataOut = false;
        return;
    }

    auto reader = connection;
    if (!reader || !reader->IsOpen()) {
        LOG_ERROR(sLogger, ("journal server reader not available for event processing", "")("config", configName));
        if (hasMoreDataOut)
            *hasMoreDataOut = false;
        return;
    }

    // 直接读取和处理journal条目，并输出是否有更多数据
    ReadJournalEntries(configName, config, reader, config.mQueueKey, hasMoreDataOut);
}

void JournalServer::runInPollingMode(std::map<int, MonitoredReader>& monitoredReaders) {
    while (mIsThreadRunning.load()) {
        try {
            // 更新连接监听状态（轮询模式，传入 -1 表示不使用 epoll）
            refreshMonitors(-1, monitoredReaders);

            // 轮询所有reader
            bool hasAnyReaderWithData = false;
            for (auto it = monitoredReaders.begin(); it != monitoredReaders.end(); ++it) {
                auto& monitoredReader = it->second;
                if (!monitoredReader.reader) {
                    continue;
                }

                // 尝试读取数据
                bool hasMoreData = false;
                processJournal(monitoredReader.configName, &hasMoreData);
                monitoredReader.hasMoreData = hasMoreData;
                if (hasMoreData) {
                    hasAnyReaderWithData = true;
                }
            }

            // 如果没有数据，等待新的journal事件（类似Golang实现）
            if (!hasAnyReaderWithData && !monitoredReaders.empty()) {
                // 等待200ms，如果有新事件则立即返回
                // 只需要对第一个有效的reader调用WaitForNewEvent
                uint64_t timeout_usec = kJournalEpollTimeoutMS * 1000; // 转换为微秒
                JournalEventType eventType = JournalEventType::kNop;

                for (auto it = monitoredReaders.begin(); it != monitoredReaders.end(); ++it) {
                    auto& monitoredReader = it->second;
                    if (!monitoredReader.reader) {
                        continue;
                    }
                    // 只等待第一个有效的reader
                    eventType = monitoredReader.reader->WaitForNewEvent(timeout_usec);
                    break;
                }

                // 如果超时或没有事件，继续下一次循环
                if (eventType != JournalEventType::kAppend && eventType != JournalEventType::kInvalidate) {
                    continue;
                }
            }

        } catch (const exception& e) {
            LOG_ERROR(sLogger, ("journal server exception in polling loop", e.what()));
            this_thread::sleep_for(chrono::milliseconds(1000));
        } catch (...) {
            LOG_ERROR(sLogger, ("journal server unknown exception in polling loop", ""));
            this_thread::sleep_for(chrono::milliseconds(1000));
        }
    }
}

bool JournalServer::handlePendingDataReaders(std::map<int, MonitoredReader>& monitoredReaders) {
    // 检查是否存在有hasMoreData标志的reader
    bool hasReadersWithMoreData = false;
    for (const auto& pair : monitoredReaders) {
        if (pair.second.hasMoreData) {
            hasReadersWithMoreData = true;
            break;
        }
    }

    // 只有当存在hasMoreData的reader时，才进行兜底读取
    if (!hasReadersWithMoreData) {
        return false; // 没有待处理的数据
    }

    LOG_DEBUG(sLogger,
              ("journal server epoll timeout with pending data, fallback reading", "")("monitored_readers",
                                                                                       monitoredReaders.size()));

    for (auto it = monitoredReaders.begin(); it != monitoredReaders.end(); ++it) {
        auto& monitoredReader = it->second;
        if (!monitoredReader.reader) {
            continue;
        }

        // 只对有hasMoreData标志的reader进行读取
        if (monitoredReader.hasMoreData) {
            bool hasMoreData = false;
            processJournal(monitoredReader.configName, &hasMoreData);
            monitoredReader.hasMoreData = hasMoreData;
        }
    }

    return true; // 已处理待处理的数据
}

bool logtail::JournalServer::validateQueueKey(const std::string& configName,
                                              const JournalConfig& config,
                                              QueueKey& queueKey) {
    // 基本验证
    if (!config.mCtx) {
        LOG_ERROR(sLogger,
                  ("journal server CRITICAL: no context available for config",
                   "this indicates initialization problem")("config", configName));
        return false;
    }

    // 如果配置中已经有queueKey，直接使用（用于测试环境）
    if (config.mQueueKey != -1) {
        queueKey = config.mQueueKey;
        LOG_INFO(sLogger, ("journal server using pre-set queue key", "")("config", configName)("queue_key", queueKey));
        return true;
    }

    // 从pipeline context获取queue key
    queueKey = config.mCtx->GetProcessQueueKey();
    if (queueKey == -1) {
        LOG_WARNING(sLogger, ("journal server no queue key available for config", "skip")("config", configName));
        return false;
    }

    // 检查队列是否有效
    if (!ProcessQueueManager::GetInstance()->IsValidToPush(queueKey)) {
        // 队列无效，跳过该journal配置的处理
        return false;
    }

    return true;
}

// =============================================================================
// 测试和调试支持 - Test and Debug Support
// =============================================================================

#ifdef APSARA_UNIT_TEST_MAIN
void JournalServer::Clear() {
    // 配置已移至 JournalConnectionManager，通过 Cleanup() 清理
    JournalConnectionManager::GetInstance().Cleanup();
}
#endif

} // namespace logtail
