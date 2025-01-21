#include <json/json.h>

#include <algorithm>
#include <iostream>
#include <random>
#include <chrono>
#include <atomic>

#include "logger/Logger.h"
#include "unittest/Unittest.h"

#include "ebpf/util/AggregateTree.h"
#include "common/timer/Timer.h"
#include "common/timer/TimerEvent.h"
#include "ebpf/type/PeriodicalEvent.h"
#include "ebpf/type/FileEvent.h"
#include "ebpf/type/NetworkEvent.h"
#include "ebpf/type/ProcessEvent.h"


DECLARE_FLAG_BOOL(logtail_mode);

class HT {
   public:
    std::unordered_map<int, std::string> tt;
    // val是数量
    int val = 0;

    explicit HT(int val) : val(val) {};
};

namespace logtail {
namespace ebpf {

class AggregatorUnittest : public testing::Test {
public:
    AggregatorUnittest() {
        mTimer.Init();
    }
    ~AggregatorUnittest() {
        mTimer.Stop();
    }

    void TestBasicAgg();
    void TestMove();
    void TestAggManager();
    void TestAggregator();

protected:
    void SetUp() override {
        agg = std::make_unique<SIZETAggTree<HT, std::vector<std::string>>>(
            10, 
            [](std::unique_ptr<HT> &base, const std::vector<std::string> &other) {
                APSARA_TEST_TRUE(base != nullptr);
                size_t i = 0;
                for (auto &key : other) {
                    base->tt[i++] = key;
                }
                base->val++;
            }, [](const std::vector<std::string> & in) {
                // LOG_INFO(sLogger, ("enter generate ... ", ""));
                return std::make_unique<HT>(0);
            });
        
    }
    void TearDown() override {}

    int GetSum() { return GetSum(agg); }

    static int GetSum(const std::unique_ptr<SIZETAggTree<HT, std::vector<std::string>>> &agg) {
        int result = 0;
        agg->ForEach([&result](const auto ht) { result += ht->val; });
        return result;
    }

    int GetDataNodeCount() { return GetDataNodeCount(agg); }

    inline size_t GetHashByDepth(const std::vector<std::string> & data, int depth) {
        size_t seed = 0UL;
        for(int i=0; i<depth; i++) {
            seed ^= std::hash<std::string>{}(data[i])+ 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
    bool Aggregate(const std::vector<std::string> & data, int depth) {
        return agg->Aggregate(data, std::array<size_t, 1>{GetHashByDepth(data, depth)});
    }

    static int GetDataNodeCount(const std::unique_ptr<SIZETAggTree<HT, std::vector<std::string>>> &agg) {
        int node_count = 0;
        agg->ForEach([&node_count](const auto ht) { node_count++; });
        return node_count;
    }

private:
    Timer mTimer;
    std::atomic_bool mFlag = true;
    int mStartUid = 0;
    std::vector<int> mVec;
    int mIntervalSec = 1;
    std::unique_ptr<SIZETAggTree<HT, std::vector<std::string>>> agg;
    std::unique_ptr<SIZETAggTree<FileEventGroup, std::shared_ptr<FileEvent>>> mAggregateTree;
    std::unique_ptr<SIZETAggTree<NetworkEventGroup, std::shared_ptr<NetworkEvent>>> mNetAggregateTree;
};

std::array<size_t, 2> GenerateAggKey(const std::shared_ptr<FileEvent> event) {
    std::array<size_t, 2> hash_result;
    hash_result.fill(0UL);
    std::hash<std::string> hasher;
    std::hash<uint64_t> hasher0;
    
    hash_result[0] = uint64_t(event->mPid) ^ (event->mKtime >> 32) ^ (event->mKtime << 32);
    // LOG_INFO(sLogger, ("ktime", event->mKtime) ("hash result", hash_result[0]));
    // aggregate_tree_.Aggregate();
    hash_result[1] ^= hasher(event->mPath) +
                                        0x9e3779b9 +
                                        (hash_result[1] << 6) +
                                        (hash_result[1] >> 2);
    return hash_result;
}

void AggregatorUnittest::TestAggregator() {

    mAggregateTree = std::make_unique<SIZETAggTree<FileEventGroup, std::shared_ptr<FileEvent>>> (
        4096, 
        [this](std::unique_ptr<FileEventGroup> &base, const std::shared_ptr<FileEvent>& other) {
            base->mInnerEvents.emplace_back(std::move(other));
        }, 
        [this](const std::shared_ptr<FileEvent>& in) {
            LOG_INFO(sLogger, ("generate node", ""));
            return std::make_unique<FileEventGroup>(in->mPid, in->mKtime, in->mPath);
        }
    );

    std::vector<std::shared_ptr<FileEvent>> events;
    // process ===> 2 file
    events.push_back(std::make_shared<FileEvent>(100, 100, KernelEventType::FILE_MMAP, 0, "path-0"));
    events.push_back(std::make_shared<FileEvent>(100, 100, KernelEventType::FILE_PATH_TRUNCATE, 1, "path-0"));
    events.push_back(std::make_shared<FileEvent>(100, 100, KernelEventType::FILE_PATH_TRUNCATE, 2, "path-1"));
    events.push_back(std::make_shared<FileEvent>(100, 100, KernelEventType::FILE_PATH_TRUNCATE, 3, "path-1"));

    // process ===> 3 file
    events.push_back(std::make_shared<FileEvent>(1, 101, KernelEventType::FILE_MMAP, 4, "path-0"));
    events.push_back(std::make_shared<FileEvent>(1, 101, KernelEventType::FILE_PATH_TRUNCATE, 5, "path-1"));
    events.push_back(std::make_shared<FileEvent>(1, 101, KernelEventType::FILE_MMAP, 6, "path-2"));
    events.push_back(std::make_shared<FileEvent>(1, 101, KernelEventType::FILE_MMAP, 7, "path-0"));
    events.push_back(std::make_shared<FileEvent>(1, 101, KernelEventType::FILE_PATH_TRUNCATE, 8, "path-1"));
    events.push_back(std::make_shared<FileEvent>(1, 101, KernelEventType::FILE_MMAP, 9, "path-2"));

    for (auto evt : events) {
        auto key = GenerateAggKey(evt);
        // LOG_INFO(sLogger, ("key0", key[0]) ("key1", key[1]) ("path", evt->mPath) ("pid", evt->mPid) ("time", evt->mKtime));
        mAggregateTree->Aggregate(evt, key);
    }

    auto nodes = mAggregateTree->GetNodesWithAggDepth(1);
    APSARA_TEST_EQUAL(2, nodes.size());

    int globalNodeCnt = 0;
    int globalEventCnt = 0;

    for (auto& node : nodes) {
        // convert to a item and push to process queue
        // represent a pid, ktime
        auto pid = node->child.begin()->second->data->mPid;
        auto ktime = node->child.begin()->second->data->mKtime;
        PipelineEventGroup eventGroup(std::make_shared<SourceBuffer>());
        this->mAggregateTree->ForEach(node, [&](const FileEventGroup* group) {
            // path level
            APSARA_TEST_EQUAL(group->mPid, pid);
            APSARA_TEST_EQUAL(group->mKtime, ktime);
            globalNodeCnt++;
            LOG_WARNING(sLogger, ("pid", group->mPid) ("ktime", group->mKtime) ("path", group->mPath));
            for (auto innerEvent : group->mInnerEvents) {
                globalEventCnt++;
                if (innerEvent->mTimestamp == 9) {
                    APSARA_TEST_EQUAL(group->mPid, 1);
                    APSARA_TEST_EQUAL(innerEvent->mPath, "path-2");
                }
                auto* logEvent = eventGroup.AddLogEvent();
                auto ts = innerEvent->mTimestamp; // + this->mTimeDiff.count();
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::nanoseconds(ts));
                logEvent->SetTimestamp(seconds.count(), ts);
                if (innerEvent->mTimestamp) {
                }
                switch (innerEvent->mEventType)
                {
                case KernelEventType::FILE_PATH_TRUNCATE:{
                    logEvent->SetContent("call_name", std::string("security_path_truncate"));
                    logEvent->SetContent("event_type", std::string("kprobe"));
                    break;
                }
                case KernelEventType::FILE_MMAP:{
                    logEvent->SetContent("call_name", std::string("security_mmap_file"));
                    logEvent->SetContent("event_type", std::string("kprobe"));
                    break;
                }
                case KernelEventType::FILE_PERMISSION_EVENT:{
                    logEvent->SetContent("call_name", std::string("security_file_permission"));
                    logEvent->SetContent("event_type", std::string("kprobe"));
                    break;
                }
                default:
                    break;
                }
            }
        });
    }
    APSARA_TEST_EQUAL(globalEventCnt, 10);
    APSARA_TEST_EQUAL(globalNodeCnt, 5);
    this->mAggregateTree->Clear();
}

void AggregatorUnittest::TestAggManager() {
    auto now = std::chrono::steady_clock::now();
    auto nextTime = now + std::chrono::seconds(1);

    std::unique_ptr<AggregateEvent> event = std::make_unique<AggregateEvent>(1, 
        [this](const std::chrono::steady_clock::time_point& execTime){ // handler
            if (!this->mFlag) {
                return false;
            }
            this->mVec.push_back(1);
            return true;
        }, [this](int currentUid) { // validator
            auto isStop = !this->mFlag.load() || currentUid != this->mStartUid;
            if (isStop) {
                LOG_WARNING(sLogger, ("stop schedule, invalid, mflag", this->mFlag) ("currentUid", currentUid) ("pluginUid", this->mStartUid));
            }
            return isStop;
        },
        mStartUid
    );

    mTimer.PushEvent(std::move(event));

    std::this_thread::sleep_for(std::chrono::seconds(4));
    mFlag = false;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    APSARA_TEST_EQUAL(mVec.size(), 3);
    mFlag = true;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    APSARA_TEST_EQUAL(mVec.size(), 3);
}

void AggregatorUnittest::TestBasicAgg() {
    Aggregate({"a", "b", "c", "d"}, 4);
    Aggregate({"a", "b", "c", "d", "e"}, 4);
    Aggregate({"a", "b", "d", "r"}, 4);
    Aggregate({"a", "b", "c", "e"}, 4);
    Aggregate({"a", "b", "c"}, 3);
    APSARA_TEST_EQUAL(GetDataNodeCount(), 4);
    APSARA_TEST_EQUAL(agg->NodeCount(), 4);
    APSARA_TEST_EQUAL(GetSum(), 5);
    agg->Clear();
    APSARA_TEST_EQUAL(GetDataNodeCount(), 0);
    APSARA_TEST_EQUAL(agg->NodeCount(), 0);
    APSARA_TEST_EQUAL(GetSum(), 0);
}

void AggregatorUnittest::TestMove() {
    Aggregate({"a", "b", "c", "d"}, 4);
    Aggregate({"a", "b", "c", "d", "e"}, 4);
    Aggregate({"a", "b", "d", "r"}, 4);
    Aggregate({"a", "b", "c", "e"}, 4);
    Aggregate({"a", "b", "c"}, 3);
    APSARA_TEST_EQUAL(GetDataNodeCount(), 4);
    APSARA_TEST_EQUAL(agg->NodeCount(), 4);
    APSARA_TEST_EQUAL(GetSum(), 5);
    auto moved_map = std::make_unique<SIZETAggTree<HT, std::vector<std::string>>>(std::move(*agg));
    APSARA_TEST_EQUAL(GetDataNodeCount(), 0);
    APSARA_TEST_EQUAL(agg->NodeCount(), 0);
    APSARA_TEST_EQUAL(GetSum(), 0);

    APSARA_TEST_EQUAL(GetDataNodeCount(moved_map), 4);
    APSARA_TEST_EQUAL(moved_map->NodeCount(), 4);
    APSARA_TEST_EQUAL(GetSum(moved_map), 5);
}

UNIT_TEST_CASE(AggregatorUnittest, TestBasicAgg);
UNIT_TEST_CASE(AggregatorUnittest, TestMove);
// UNIT_TEST_CASE(AggregatorUnittest, TestAggManager);
UNIT_TEST_CASE(AggregatorUnittest, TestAggregator);


} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
