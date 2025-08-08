// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <chrono>
#include <memory>
#include <thread>

#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/EBPFAdapter.h"
#include "ebpf/plugin/network_observer/NetworkObserverManager.h"
#include "metadata/K8sMetadata.h"
#include "unittest/Unittest.h"

namespace logtail::ebpf {

class HttpRetryableEventUnittest : public ::testing::Test {
public:
    void TestConnStatsEventRetryProcess();
    void TestK8sMetaRetryProcess();

protected:
    void SetUp() override {
        mEBPFAdapter = std::make_shared<EBPFAdapter>();
        mEBPFAdapter->Init();
    }

    void TearDown() override {}

    std::shared_ptr<NetworkObserverManager> CreateManager() {
        return NetworkObserverManager::Create(nullptr, mEBPFAdapter, mEventQueue);
    }

    int HandleEvents() {
        std::array<std::shared_ptr<CommonEvent>, 4096> items;
        size_t count = mEventQueue.wait_dequeue_bulk_timed(items.data(), items.size(), std::chrono::milliseconds(200));
        LOG_WARNING(sLogger, ("count", count));
        for (size_t i = 0; i < count; i++) {
            if (items[i] == nullptr) {
                LOG_WARNING(sLogger, ("event is null", ""));
                continue;
            }
            mManager->HandleEvent(items[i]);
        }
        return count;
    }

    std::shared_ptr<NetworkObserverManager> mManager;
    std::shared_ptr<EBPFAdapter> mEBPFAdapter;
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>> mEventQueue;
};

conn_data_event_t* CreateHttpDataEvent() {
    const std::string resp = "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html\r\n"
                             "Content-Length: 13\r\n"
                             "\r\n"
                             "Hello, World!";
    const std::string req = "GET /index.html HTTP/1.1\r\nHost: www.cmonitor.ai\r\nAccept: image/gif, image/jpeg, "
                            "*/*\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n\r\n";
    std::string msg = req + resp;
    conn_data_event_t* evt = (conn_data_event_t*)malloc(offsetof(conn_data_event_t, msg) + msg.size());
    memcpy(evt->msg, msg.data(), msg.size());
    evt->conn_id.fd = 0;
    evt->conn_id.start = 1;
    evt->conn_id.tgid = 2;
    evt->role = support_role_e::IsClient;
    evt->request_len = req.size();
    evt->response_len = resp.size();
    evt->protocol = support_proto_e::ProtoHTTP;
    evt->start_ts = 1;
    evt->end_ts = 2;
    return evt;
}

conn_data_event_t* CreateHttpDataEvent(int i) {
    const std::string resp = "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html\r\n"
                             "Content-Length: 13\r\n"
                             "\r\n"
                             "Hello, World!";
    const std::string req = "GET /index.html/" + std::to_string(i)
        + " HTTP/1.1\r\nHost: www.cmonitor.ai\r\nAccept: image/gif, image/jpeg, "
          "*/*\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n\r\n";
    std::string msg = req + resp;
    conn_data_event_t* evt = (conn_data_event_t*)malloc(offsetof(conn_data_event_t, msg) + msg.size());
    memcpy(evt->msg, msg.data(), msg.size());
    evt->conn_id.fd = 0;
    evt->conn_id.start = 1;
    evt->conn_id.tgid = 2;
    evt->role = support_role_e::IsClient;
    evt->request_len = req.size();
    evt->response_len = resp.size();
    evt->protocol = support_proto_e::ProtoHTTP;
    evt->start_ts = 1;
    evt->end_ts = 2;
    return evt;
}

conn_stats_event_t CreateConnStatsEvent() {
    struct conn_stats_event_t statsEvent = {};
    statsEvent.protocol = support_proto_e::ProtoUnknown;
    statsEvent.role = support_role_e::IsClient;
    statsEvent.si.family = AF_INET;
    statsEvent.si.ap.saddr = 0x0100007F; // 127.0.0.1
    statsEvent.si.ap.daddr = 0x0101A8C0; // 192.168.1.1
    statsEvent.si.ap.sport = htons(8080);
    statsEvent.si.ap.dport = htons(80);
    statsEvent.ts = 1;
    // set docker id
    statsEvent.wr_bytes = 1;
    statsEvent.conn_id.fd = 0;
    statsEvent.conn_id.start = 1;
    statsEvent.conn_id.tgid = 2;
    // docker id
    std::string testCid
        = "/machine.slice/libpod-80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106.scope";
    memcpy(statsEvent.docker_id, testCid.c_str(), testCid.size());
    return statsEvent;
}

void HttpRetryableEventUnittest::TestConnStatsEventRetryProcess() {
    LOG_INFO(sLogger, ("TestRollbackProcessing", "start"));
    {
        mManager = CreateManager();
        ObserverNetworkOption options;
        options.mL7Config.mEnable = true;
        options.mL7Config.mEnableLog = true;
        options.mL7Config.mEnableMetric = true;
        options.mL7Config.mEnableSpan = true;

        options.mApmConfig.mAppId = "test-app-id";
        options.mApmConfig.mAppName = "test-app-name";
        options.mApmConfig.mWorkspace = "test-workspace";
        options.mApmConfig.mServiceId = "test-service-id";

        options.mSelectors = {{"test-workloadname", "Deployment", "test-namespace"}};

        mManager->Init();
        CollectionPipelineContext ctx;
        ctx.SetConfigName("test-config-networkobserver");
        ctx.SetProcessQueueKey(1);
        mManager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

        auto podInfo = std::make_shared<K8sPodInfo>();
        podInfo->mContainerIds = {"1", "2"};
        podInfo->mPodIp = "test-pod-ip";
        podInfo->mPodName = "test-pod-name";
        podInfo->mNamespace = "test-namespace";
        podInfo->mWorkloadKind = "Deployment";
        podInfo->mWorkloadName = "test-workloadname";

        LOG_INFO(sLogger, ("step", "0-0"));
        K8sMetadata::GetInstance().mContainerCache.insert(
            "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106", podInfo);

        auto peerPodInfo = std::make_shared<K8sPodInfo>();
        peerPodInfo->mContainerIds = {"3", "4"};
        peerPodInfo->mPodIp = "peer-pod-ip";
        peerPodInfo->mPodName = "peer-pod-name";
        peerPodInfo->mNamespace = "peer-namespace";
        K8sMetadata::GetInstance().mIpCache.insert("192.168.1.1", peerPodInfo);
        mManager->HandleHostMetadataUpdate({"80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106"});

        APSARA_TEST_EQUAL(mManager->mConnectionManager->ConnectionTotal(), 0);
        // copy current
        mManager->mContainerConfigsReplica = mManager->mContainerConfigs;

        // Generate 10 records (without conn stats)
        for (size_t i = 0; i < 100; i++) {
            auto* dataEvent = CreateHttpDataEvent(i);
            auto cnn = mManager->mConnectionManager->AcceptNetDataEvent(dataEvent);
            cnn->mCidKey = GenerateContainerKey("80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106");
            LOG_INFO(sLogger, ("cidKey", cnn->mCidKey));
            APSARA_TEST_FALSE(cnn->IsMetaAttachReadyForAppRecord());
            APSARA_TEST_TRUE(cnn->IsL7MetaAttachReady());
            APSARA_TEST_FALSE(cnn->IsPeerMetaAttachReady());
            APSARA_TEST_FALSE(cnn->IsSelfMetaAttachReady());
            APSARA_TEST_FALSE(cnn->IsL4MetaAttachReady());
            APSARA_TEST_EQUAL(mManager->mConnectionManager->ConnectionTotal(), 1);
            mManager->AcceptDataEvent(dataEvent);
            free(dataEvent);
        }

        APSARA_TEST_EQUAL(mManager->mConnectionManager->ConnectionTotal(), 1);
        APSARA_TEST_EQUAL(mManager->mRetryableEventCache.Size(), 100);

        auto cnn = mManager->mConnectionManager->getConnection({0, 2, 1});
        APSARA_TEST_FALSE(cnn->IsMetaAttachReadyForAppRecord());
        APSARA_TEST_TRUE(cnn->IsL7MetaAttachReady());
        APSARA_TEST_FALSE(cnn->IsPeerMetaAttachReady());
        APSARA_TEST_FALSE(cnn->IsSelfMetaAttachReady());
        APSARA_TEST_FALSE(cnn->IsL4MetaAttachReady());

        APSARA_TEST_EQUAL(0, HandleEvents());


        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // conn stats arrive
        auto statsEvent = CreateConnStatsEvent();
        mManager->AcceptNetStatsEvent(&statsEvent);
        APSARA_TEST_TRUE(cnn != nullptr);
        APSARA_TEST_TRUE(cnn->IsL7MetaAttachReady());
        APSARA_TEST_TRUE(cnn->IsPeerMetaAttachReady());
        APSARA_TEST_TRUE(cnn->IsSelfMetaAttachReady());
        APSARA_TEST_TRUE(cnn->IsL4MetaAttachReady());

        APSARA_TEST_TRUE(cnn->IsMetaAttachReadyForAppRecord());
        APSARA_TEST_EQUAL(mManager->mRetryableEventCache.Size(), 100);


        // APSARA_TEST_EQUAL(mManager->mRollbackRecordTotal, 100);

        LOG_INFO(sLogger, ("before handle cache", ""));
        mManager->mRetryableEventCache.HandleEvents();
        APSARA_TEST_EQUAL(100, HandleEvents());
        LOG_INFO(sLogger, ("after handle cache", ""));

        // std::this_thread::sleep_for(std::chrono::seconds(5));
        // APSARA_TEST_EQUAL(mManager->mDropRecordTotal, 0);
        APSARA_TEST_EQUAL(mManager->mRetryableEventCache.Size(), 0);

        // Generate 10 records
        for (size_t i = 0; i < 100; i++) {
            auto* dataEvent = CreateHttpDataEvent(i);
            mManager->AcceptDataEvent(dataEvent);
            free(dataEvent);
        }

        APSARA_TEST_EQUAL(100, HandleEvents());
        // std::this_thread::sleep_for(std::chrono::milliseconds(500));
        APSARA_TEST_EQUAL(mManager->mRetryableEventCache.Size(), 0);
    }
    LOG_INFO(sLogger, ("TestRollbackProcessing", "stop"));
}

void HttpRetryableEventUnittest::TestK8sMetaRetryProcess() {
    LOG_INFO(sLogger, ("TestRollbackProcessing", "start"));
    {
        mManager = CreateManager();
        ObserverNetworkOption options;
        options.mL7Config.mEnable = true;
        options.mL7Config.mEnableLog = true;
        options.mL7Config.mEnableMetric = true;
        options.mL7Config.mEnableSpan = true;

        options.mApmConfig.mAppId = "test-app-id";
        options.mApmConfig.mAppName = "test-app-name";
        options.mApmConfig.mWorkspace = "test-workspace";
        options.mApmConfig.mServiceId = "test-service-id";

        options.mSelectors = {{"test-workloadname", "Deployment", "test-namespace"}};

        mManager->Init();
        CollectionPipelineContext ctx;
        ctx.SetConfigName("test-config-networkobserver");
        ctx.SetProcessQueueKey(1);
        mManager->AddOrUpdateConfig(&ctx, 0, nullptr, std::variant<SecurityOptions*, ObserverNetworkOption*>(&options));

        K8sMetadata::GetInstance().mContainerCache.clear();
        K8sMetadata::GetInstance().mIpCache.clear();
        K8sMetadata::GetInstance().mExternalIpCache.clear();

        auto podInfo = std::make_shared<K8sPodInfo>();
        podInfo->mContainerIds = {"1", "2"};
        podInfo->mPodIp = "test-pod-ip";
        podInfo->mPodName = "test-pod-name";
        podInfo->mNamespace = "test-namespace";
        podInfo->mWorkloadKind = "Deployment";
        podInfo->mWorkloadName = "test-workloadname";

        LOG_INFO(sLogger, ("step", "0-0"));
        K8sMetadata::GetInstance().mContainerCache.insert(
            "80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106", podInfo);

        mManager->HandleHostMetadataUpdate({"80b2ea13472c0d75a71af598ae2c01909bb5880151951bf194a3b24a44613106"});

        APSARA_TEST_EQUAL(mManager->mConnectionManager->ConnectionTotal(), 0);
        // copy current
        mManager->mContainerConfigsReplica = mManager->mContainerConfigs;

        // conn stats arrive
        auto statsEvent = CreateConnStatsEvent();
        mManager->AcceptNetStatsEvent(&statsEvent);
        auto cnn = mManager->mConnectionManager->getConnection({0, 2, 1});
        APSARA_TEST_TRUE(cnn != nullptr);
        APSARA_TEST_FALSE(cnn->IsL7MetaAttachReady());
        LOG_INFO(sLogger, ("ql_check conn", cnn->DumpConnection())("flags", cnn->GetMetaFlags()));
        APSARA_TEST_FALSE(cnn->IsPeerMetaAttachReady());
        APSARA_TEST_TRUE(cnn->IsSelfMetaAttachReady());
        APSARA_TEST_TRUE(cnn->IsL4MetaAttachReady());

        // Generate 10 records (with conn stats, but peer pod metadata not exists ...)
        for (size_t i = 0; i < 100; i++) {
            auto* dataEvent = CreateHttpDataEvent(i);
            auto cnn = mManager->mConnectionManager->AcceptNetDataEvent(dataEvent);
            // LOG_INFO(sLogger, ("ql_check cidKey", cnn->mCidKey));
            APSARA_TEST_FALSE(cnn->IsMetaAttachReadyForAppRecord());
            APSARA_TEST_TRUE(cnn->IsL7MetaAttachReady());
            APSARA_TEST_FALSE(cnn->IsPeerMetaAttachReady());
            APSARA_TEST_TRUE(cnn->IsSelfMetaAttachReady());
            APSARA_TEST_TRUE(cnn->IsL4MetaAttachReady());
            APSARA_TEST_EQUAL(mManager->mConnectionManager->ConnectionTotal(), 1);
            mManager->AcceptDataEvent(dataEvent);
            free(dataEvent);
        }

        APSARA_TEST_EQUAL(mManager->mConnectionManager->ConnectionTotal(), 1);
        APSARA_TEST_EQUAL(mManager->mRetryableEventCache.Size(), 100);

        cnn = mManager->mConnectionManager->getConnection({0, 2, 1});
        APSARA_TEST_FALSE(cnn->IsMetaAttachReadyForAppRecord());
        APSARA_TEST_TRUE(cnn->IsL7MetaAttachReady());
        APSARA_TEST_FALSE(cnn->IsPeerMetaAttachReady());
        APSARA_TEST_TRUE(cnn->IsSelfMetaAttachReady());
        APSARA_TEST_TRUE(cnn->IsL4MetaAttachReady());

        APSARA_TEST_EQUAL(0, HandleEvents());

        APSARA_TEST_EQUAL(mManager->mRetryableEventCache.Size(), 100);

        { // peer pod metadata arrival
            auto peerPodInfo = std::make_shared<K8sPodInfo>();
            peerPodInfo->mContainerIds = {"3", "4"};
            peerPodInfo->mPodIp = "peer-pod-ip";
            peerPodInfo->mPodName = "peer-pod-name";
            peerPodInfo->mNamespace = "peer-namespace";
            K8sMetadata::GetInstance().mIpCache.insert("192.168.1.1", peerPodInfo);
        }

        LOG_INFO(sLogger, ("before handle cache", ""));
        mManager->mRetryableEventCache.HandleEvents();
        APSARA_TEST_EQUAL(100, HandleEvents());
        LOG_INFO(sLogger, ("after handle cache", ""));

        // std::this_thread::sleep_for(std::chrono::seconds(5));
        // APSARA_TEST_EQUAL(mManager->mDropRecordTotal, 0);
        APSARA_TEST_EQUAL(mManager->mRetryableEventCache.Size(), 0);

        // Generate 10 records
        for (size_t i = 0; i < 100; i++) {
            auto* dataEvent = CreateHttpDataEvent(i);
            mManager->AcceptDataEvent(dataEvent);
            free(dataEvent);
        }
        APSARA_TEST_EQUAL(mManager->mRetryableEventCache.Size(), 0);
        APSARA_TEST_EQUAL(100, HandleEvents());
        // std::this_thread::sleep_for(std::chrono::milliseconds(500));
        APSARA_TEST_EQUAL(mManager->mRetryableEventCache.Size(), 0);
    }
    LOG_INFO(sLogger, ("TestRollbackProcessing", "stop"));
}

UNIT_TEST_CASE(HttpRetryableEventUnittest, TestConnStatsEventRetryProcess);
UNIT_TEST_CASE(HttpRetryableEventUnittest, TestK8sMetaRetryProcess);

} // namespace logtail::ebpf

UNIT_TEST_MAIN
