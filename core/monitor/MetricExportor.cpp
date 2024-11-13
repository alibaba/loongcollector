// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "MetricExportor.h"

#include <filesystem>

#include "LogFileProfiler.h"
#include "LogtailMetric.h"
#include "MetricConstants.h"
#include "app_config/AppConfig.h"
#include "common/FileSystemUtil.h"
#include "common/RuntimeUtil.h"
#include "common/TimeUtil.h"
#include "go_pipeline/LogtailPlugin.h"
#include "pipeline/PipelineManager.h"
#include "protobuf/sls/sls_logs.pb.h"

using namespace sls_logs;
using namespace std;

DECLARE_FLAG_STRING(metrics_report_method);

namespace logtail {

const string METRIC_REGION_DEFAULT = "default";
const string METRIC_SLS_LOGSTORE_NAME = "shennong_log_profile";
const string METRIC_TOPIC_TYPE = "loong_collector_metric";

const std::string METRIC_EXPORT_TYPE_GO = "direct";
const std::string METRIC_EXPORT_TYPE_CPP = "cpp_provided";

MetricExportor::MetricExportor() : mSendInterval(60), mLastSendTime(time(NULL) - (rand() % (mSendInterval / 10)) * 10) {
}

void MetricExportor::PushMetrics(bool forceSend) {
    int32_t curTime = time(NULL);
    if (!forceSend && (curTime - mLastSendTime < mSendInterval)) {
        return;
    }

    // go指标在Cpp指标前获取，是为了在 Cpp 部分指标做 SnapShot
    // 前（即调用 ReadMetrics::GetInstance()->UpdateMetrics() 函数），把go部分的进程级指标填写到 Cpp
    // 的进程级指标中去，随Cpp的进程级指标一起输出
    if (LogtailPlugin::GetInstance()->IsPluginOpened()) {
        PushGoMetrics();
    }
    PushCppMetrics();
}

void MetricExportor::PushCppMetrics() {
    ReadMetrics::GetInstance()->UpdateMetrics();

    if ("sls" == STRING_FLAG(metrics_report_method)) {
        std::map<std::string, sls_logs::LogGroup*> logGroupMap;
        ReadMetrics::GetInstance()->ReadAsLogGroup(METRIC_LABEL_KEY_REGION, METRIC_REGION_DEFAULT, logGroupMap);
        SendToSLS(logGroupMap);
    } else if ("file" == STRING_FLAG(metrics_report_method)) {
        std::string metricsContent;
        ReadMetrics::GetInstance()->ReadAsFileBuffer(metricsContent);
        SendToLocalFile(metricsContent, "self-metrics-cpp");
    }
}

void MetricExportor::PushGoMetrics() {
    std::vector<std::map<std::string, std::string>> goDirectMetircsList;
    LogtailPlugin::GetInstance()->GetGoMetrics(goDirectMetircsList, METRIC_EXPORT_TYPE_GO);
    std::vector<std::map<std::string, std::string>> goCppProvidedMetircsList;
    LogtailPlugin::GetInstance()->GetGoMetrics(goCppProvidedMetircsList, METRIC_EXPORT_TYPE_CPP);

    PushGoCppProvidedMetrics(goCppProvidedMetircsList);
    PushGoDirectMetrics(goDirectMetircsList);
}

void MetricExportor::SendToSLS(std::map<std::string, sls_logs::LogGroup*>& logGroupMap) {
    std::map<std::string, sls_logs::LogGroup*>::iterator iter;
    for (iter = logGroupMap.begin(); iter != logGroupMap.end(); iter++) {
        sls_logs::LogGroup* logGroup = iter->second;
        logGroup->set_category(METRIC_SLS_LOGSTORE_NAME);
        logGroup->set_source(LogFileProfiler::mIpAddr);
        logGroup->set_topic(METRIC_TOPIC_TYPE);
        if (METRIC_REGION_DEFAULT == iter->first) {
            GetProfileSender()->SendToProfileProject(GetProfileSender()->GetDefaultProfileRegion(), *logGroup);
        } else {
            GetProfileSender()->SendToProfileProject(iter->first, *logGroup);
        }
        delete logGroup;
    }
}

void MetricExportor::SendToLocalFile(std::string& metricsContent, const std::string metricsFileNamePrefix) {
    const std::string metricsDirName = "self_metrics";
    const size_t maxFiles = 60; // 每分钟记录一次，最多保留1h的记录

    if (!metricsContent.empty()) {
        // 创建输出目录（如果不存在）
        std::string outputDirectory = GetAgentLogDir() + metricsDirName;
        Mkdirs(outputDirectory);

        std::vector<std::filesystem::path> metricFiles;

        for (const auto& entry : std::filesystem::directory_iterator(outputDirectory)) {
            if (entry.is_regular_file() && entry.path().filename().string().find(metricsFileNamePrefix) == 0) {
                metricFiles.push_back(entry.path());
            }
        }

        // 删除多余的文件
        if (metricFiles.size() > maxFiles) {
            std::sort(metricFiles.begin(),
                      metricFiles.end(),
                      [](const std::filesystem::path& a, const std::filesystem::path& b) {
                          return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
                      });

            for (size_t i = maxFiles; i < metricFiles.size(); ++i) {
                std::filesystem::remove(metricFiles[i]);
            }
        }

        // 生成文件名
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm = *std::localtime(&now_time);
        std::ostringstream oss;
        oss << metricsFileNamePrefix << std::put_time(&now_tm, "-%Y-%m-%d_%H-%M-%S") << ".json";
        std::string filePath = PathJoin(outputDirectory, oss.str());

        // 写入文件
        std::ofstream outFile(filePath);
        if (!outFile) {
            LOG_ERROR(sLogger, ("Open file fail when print metrics", filePath.c_str()));
        } else {
            outFile << metricsContent;
            outFile.close();
        }
    }
}

// metrics from Go that are directly outputted
void MetricExportor::PushGoDirectMetrics(std::vector<std::map<std::string, std::string>>& metricsList) {
    if (metricsList.size() == 0) {
        return;
    }

    if ("sls" == STRING_FLAG(metrics_report_method)) {
        std::map<std::string, sls_logs::LogGroup*> logGroupMap;
        SerializeGoDirectMetricsListToLogGroupMap(metricsList, logGroupMap);
        SendToSLS(logGroupMap);
    } else if ("file" == STRING_FLAG(metrics_report_method)) {
        std::string metricsContent;
        SerializeGoDirectMetricsListToString(metricsList, metricsContent);
        SendToLocalFile(metricsContent, "self-metrics-go");
    }
}

// metrics from Go that are provided by cpp
void MetricExportor::PushGoCppProvidedMetrics(std::vector<std::map<std::string, std::string>>& metricsList) {
    if (metricsList.size() == 0) {
        return;
    }

    for (auto metrics : metricsList) {
        for (auto metric : metrics) {
            if (metric.first == METRIC_KEY_VALUE + "." + METRIC_AGENT_MEMORY_GO) {
                LoongCollectorMonitor::GetInstance()->SetAgentGoMemory(std::stoi(metric.second));
            }
            if (metric.first == METRIC_KEY_VALUE + "." + METRIC_AGENT_GO_ROUTINES_TOTAL) {
                LoongCollectorMonitor::GetInstance()->SetAgentGoRoutinesTotal(std::stoi(metric.second));
            }
            LogtailMonitor::GetInstance()->UpdateMetric(metric.first, metric.second);
        }
    }
}

void MetricExportor::SerializeGoDirectMetricsListToLogGroupMap(
    std::vector<std::map<std::string, std::string>>& metricsList,
    std::map<std::string, sls_logs::LogGroup*>& logGroupMap) {
    for (auto& metrics : metricsList) {
        std::string configName = "";
        std::string region = METRIC_REGION_DEFAULT;
        {
            // get the pipeline_name label
            for (const auto& metric : metrics) {
                if (metric.first == METRIC_KEY_LABEL + "." + METRIC_LABEL_KEY_PIPELINE_NAME) {
                    configName = metric.second;
                    break;
                }
            }
            if (!configName.empty()) {
                // get region info by pipeline_name
                shared_ptr<Pipeline> p = PipelineManager::GetInstance()->FindConfigByName(configName);
                if (p) {
                    FlusherSLS* pConfig = NULL;
                    pConfig = const_cast<FlusherSLS*>(static_cast<const FlusherSLS*>(p->GetFlushers()[0]->GetPlugin()));
                    if (pConfig) {
                        region = pConfig->mRegion;
                    }
                }
            }
        }
        Log* logPtr = nullptr;
        auto LogGroupIter = logGroupMap.find(region);
        if (LogGroupIter != logGroupMap.end()) {
            sls_logs::LogGroup* logGroup = LogGroupIter->second;
            logPtr = logGroup->add_logs();
        } else {
            sls_logs::LogGroup* logGroup = new sls_logs::LogGroup();
            logPtr = logGroup->add_logs();
            logGroupMap.insert(std::pair<std::string, sls_logs::LogGroup*>(region, logGroup));
        }
        auto now = GetCurrentLogtailTime();
        SetLogTime(logPtr,
                   AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? now.tv_sec + GetTimeDelta() : now.tv_sec);

        Json::Value metricsRecordLabel;
        for (const auto& metric : metrics) {
            // category
            if (metric.first.compare("label.metric_category") == 0) {
                Log_Content* contentPtr = logPtr->add_contents();
                contentPtr->set_key(METRIC_KEY_CATEGORY);
                contentPtr->set_value(metric.second);
                continue;
            }
            // label
            if (metric.first.compare(0, METRIC_KEY_LABEL.length(), METRIC_KEY_LABEL)) {
                metricsRecordLabel[metric.first.substr(METRIC_KEY_LABEL.length() + 1)] = metric.second;
                continue;
            }
            // value
            Log_Content* contentPtr = logPtr->add_contents();
            contentPtr->set_key(metric.first);
            contentPtr->set_value(metric.second);
        }
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        std::string jsonString = Json::writeString(writer, metricsRecordLabel);
        Log_Content* contentPtr = logPtr->add_contents();
        contentPtr->set_key(METRIC_KEY_LABEL);
        contentPtr->set_value(jsonString);
    }
}

void MetricExportor::SerializeGoDirectMetricsListToString(std::vector<std::map<std::string, std::string>>& metricsList,
                                                          std::string& metricsContent) {
    std::ostringstream oss;

    for (auto& metrics : metricsList) {
        Json::Value metricsRecordJson, metricsRecordLabel;
        auto now = GetCurrentLogtailTime();
        metricsRecordJson["time"]
            = AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? now.tv_sec + GetTimeDelta() : now.tv_sec;
        for (const auto& metric : metrics) {
            if (metric.first.compare("label.metric_category") == 0) {
                metricsRecordJson[METRIC_KEY_CATEGORY] = metric.second;
                continue;
            }
            if (metric.first.compare(0, METRIC_KEY_LABEL.length(), METRIC_KEY_LABEL) == 0) {
                metricsRecordLabel[metric.first.substr(METRIC_KEY_LABEL.length() + 1)] = metric.second;
                continue;
            }
            metricsRecordJson[metric.first.substr(METRIC_KEY_VALUE.length() + 1)] = metric.second;
        }
        metricsRecordJson[METRIC_KEY_LABEL] = metricsRecordLabel;
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        std::string jsonString = Json::writeString(writer, metricsRecordJson);
        oss << jsonString << '\n';
    }
    metricsContent = oss.str();
}

} // namespace logtail