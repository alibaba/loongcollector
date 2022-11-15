// Copyright 2022 iLogtail Authors
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

#include <gtest/gtest.h>

#include "unittest/Unittest.h"
#include "metas/CGroupPathResolver.h"
#include "metas/ContainerProcessGroup.h"
#include "unittest/UnittestHelper.h"

namespace logtail {

static const std::string DOCKER_BESTEFFORT_PATH
    = "kubepods.slice/kubepods-besteffort.slice/kubepods-besteffort-podd068a7ab_ddb1_4873_a0a4_a4e3408fe1e8.slice/"
      "docker-32c0b6ca6cb7b94a1cfa605153ae920bf9b23335efd74f6e6e51ccc097f6e70b.scope/cgroup.procs";
static const std::string DOCKER_BURSTABLE_PATH
    = "kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod0dc61110_6b40_4a27_bd00_9edb8c454211.slice/"
      "docker-7f0e7ba0b1678264767cff71448f0dd6c5490f31f296ef0c9bf4d24ef0e9914e.scope/cgroup.procs";
static const std::string DOCKER_GUARANTEED_PATH
    = "kubepods.slice/kubepods-pod3f5397a2_76aa_4d66_bd6b_83bfcb6d6dc6.slice/"
      "docker-6587c323ce97dd3a91a131acd7d04cab88f23a5bcfeb21139ca5cb878bbae2c6.scope/cgroup.procs";


static const std::string CONTAINERD_BESTEFFORT_PATH
    = "kubepods.slice/kubepods-besteffort.slice/kubepods-besteffort-pod0d206349_0faf_445c_8c3f_2d2153784f15.slice/"
      "cri-containerd-ba48928b25af68cebd77e605c5c3b2474bcd47b32143e4d9125f4a8270ac07d3.scope/cgroup.procs";
static const std::string CONTAINERD_BURSTABLE_PATH
    = "kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod6e10d863_beec_40a8_bb4c_007f7c893ef1.slice/"
      "cri-containerd-5caf42bd82e5b4fa21fcde3322332c5d19ccbeadf52281cbb00f804b372e41d0.scope/cgroup.procs";
static const std::string CONTAINERD_GUARANTEED_PATH
    = "kubepods.slice/kubepods-pod2b801b7a_5266_4386_864e_45ed71136371.slice/"
      "cri-containerd-20e061fc708d3b66dfe257b19552b34a1307a7347ed6b5bd0d8c5e76afb1a870.scope/cgroup.procs";
static const std::string ERRORPATH = "12345";


static const std::string GKE_BESTEFFORT_PATH
    = "kubepods/besteffort/pod8dbc5577-d0e2-4706-8787-57d52c03ddf2/"
      "14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d/cgroup.procs";
static const std::string GKE_BURSTABLE_PATH
    = "kubepods/burstable/pod8dbc5577-d0e2-4706-8787-57d52c03ddf2/"
      "14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d/cgroup.procs";
static const std::string GKE_GUARANTEED_PATH
    = "kubepods/pod8dbc5577-d0e2-4706-8787-57d52c03ddf2/"
      "14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d/cgroup.procs";


class CGroupPathResolverUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() { system("tar -xvf cgroup.tar"); }

    static void TearDownTestCase() { system("rm -rf cgroup"); }


    void TestCGroupExtractContainerType() {
        EXPECT_EQ(CONTAINER_TYPE::CONTAINER_TYPE_DOCKER, ExtractContainerType(DOCKER_BESTEFFORT_PATH));
        EXPECT_EQ(CONTAINER_TYPE::CONTAINER_TYPE_DOCKER, ExtractContainerType(DOCKER_BURSTABLE_PATH));
        EXPECT_EQ(CONTAINER_TYPE::CONTAINER_TYPE_DOCKER, ExtractContainerType(DOCKER_GUARANTEED_PATH));
        EXPECT_EQ(CONTAINER_TYPE::CONTAINER_TYPE_CRI_CONTAINERD, ExtractContainerType(CONTAINERD_BESTEFFORT_PATH));
        EXPECT_EQ(CONTAINER_TYPE::CONTAINER_TYPE_CRI_CONTAINERD, ExtractContainerType(CONTAINERD_BURSTABLE_PATH));
        EXPECT_EQ(CONTAINER_TYPE::CONTAINER_TYPE_CRI_CONTAINERD, ExtractContainerType(CONTAINERD_GUARANTEED_PATH));
    }

    void TestExtractDockerMeta() {
        auto matcher = GetCGroupMatcher(DOCKER_BESTEFFORT_PATH, CONTAINER_TYPE::CONTAINER_TYPE_DOCKER);
        EXPECT_TRUE(matcher != nullptr);
        delete matcher;
        matcher = GetCGroupMatcher(DOCKER_BURSTABLE_PATH, CONTAINER_TYPE::CONTAINER_TYPE_DOCKER);
        EXPECT_TRUE(matcher != nullptr);
        delete matcher;
        matcher = GetCGroupMatcher(DOCKER_GUARANTEED_PATH, CONTAINER_TYPE::CONTAINER_TYPE_DOCKER);
        EXPECT_TRUE(matcher != nullptr);

        std::string containerID, podID;
        matcher->ExtractProcessMeta(DOCKER_BESTEFFORT_PATH, containerID, podID);
        EXPECT_EQ("d068a7ab_ddb1_4873_a0a4_a4e3408fe1e8", podID);
        EXPECT_EQ("32c0b6ca6cb7b94a1cfa605153ae920bf9b23335efd74f6e6e51ccc097f6e70b", containerID);
        matcher->ExtractProcessMeta(DOCKER_BURSTABLE_PATH, containerID, podID);
        EXPECT_EQ("0dc61110_6b40_4a27_bd00_9edb8c454211", podID);
        EXPECT_EQ("7f0e7ba0b1678264767cff71448f0dd6c5490f31f296ef0c9bf4d24ef0e9914e", containerID);
        matcher->ExtractProcessMeta(DOCKER_GUARANTEED_PATH, containerID, podID);
        EXPECT_EQ("3f5397a2_76aa_4d66_bd6b_83bfcb6d6dc6", podID);
        EXPECT_EQ("6587c323ce97dd3a91a131acd7d04cab88f23a5bcfeb21139ca5cb878bbae2c6", containerID);
        podID.clear();
        containerID.clear();
        matcher->ExtractProcessMeta(ERRORPATH, containerID, podID);
        EXPECT_EQ("", podID);
        EXPECT_EQ("", containerID);
    }

    void TestExtractContainerdMeta() {
        auto matcher = GetCGroupMatcher(CONTAINERD_BESTEFFORT_PATH, CONTAINER_TYPE::CONTAINER_TYPE_CRI_CONTAINERD);
        EXPECT_TRUE(matcher != nullptr);
        delete matcher;
        matcher = GetCGroupMatcher(CONTAINERD_BURSTABLE_PATH, CONTAINER_TYPE::CONTAINER_TYPE_CRI_CONTAINERD);
        EXPECT_TRUE(matcher != nullptr);
        delete matcher;

        matcher = GetCGroupMatcher(CONTAINERD_GUARANTEED_PATH, CONTAINER_TYPE::CONTAINER_TYPE_CRI_CONTAINERD);
        EXPECT_TRUE(matcher != nullptr);


        std::string containerID, podID;
        matcher->ExtractProcessMeta(CONTAINERD_BESTEFFORT_PATH, containerID, podID);
        EXPECT_EQ("0d206349_0faf_445c_8c3f_2d2153784f15", podID);
        EXPECT_EQ("ba48928b25af68cebd77e605c5c3b2474bcd47b32143e4d9125f4a8270ac07d3", containerID);
        matcher->ExtractProcessMeta(CONTAINERD_BURSTABLE_PATH, containerID, podID);
        EXPECT_EQ("6e10d863_beec_40a8_bb4c_007f7c893ef1", podID);
        EXPECT_EQ("5caf42bd82e5b4fa21fcde3322332c5d19ccbeadf52281cbb00f804b372e41d0", containerID);
        matcher->ExtractProcessMeta(CONTAINERD_GUARANTEED_PATH, containerID, podID);
        EXPECT_EQ("2b801b7a_5266_4386_864e_45ed71136371", podID);
        EXPECT_EQ("20e061fc708d3b66dfe257b19552b34a1307a7347ed6b5bd0d8c5e76afb1a870", containerID);
        podID.clear();
        containerID.clear();
        matcher->ExtractProcessMeta(ERRORPATH, containerID, podID);
        EXPECT_EQ("", podID);
        EXPECT_EQ("", containerID);
    }


    void TestExtractGKEMeta() {
        auto matcher = GetCGroupMatcher(GKE_BESTEFFORT_PATH, CONTAINER_TYPE::CONTAINER_TYPE_CRI_CONTAINERD);
        EXPECT_TRUE(matcher != nullptr);
        delete matcher;
        matcher = GetCGroupMatcher(GKE_BURSTABLE_PATH, CONTAINER_TYPE::CONTAINER_TYPE_CRI_CONTAINERD);
        EXPECT_TRUE(matcher != nullptr);
        delete matcher;

        matcher = GetCGroupMatcher(GKE_GUARANTEED_PATH, CONTAINER_TYPE::CONTAINER_TYPE_CRI_CONTAINERD);
        EXPECT_TRUE(matcher != nullptr);


        std::string containerID, podID;
        matcher->ExtractProcessMeta(GKE_BESTEFFORT_PATH, containerID, podID);
        EXPECT_EQ("8dbc5577-d0e2-4706-8787-57d52c03ddf2", podID);
        EXPECT_EQ("14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d", containerID);
        matcher->ExtractProcessMeta(GKE_BURSTABLE_PATH, containerID, podID);
        EXPECT_EQ("8dbc5577-d0e2-4706-8787-57d52c03ddf2", podID);
        EXPECT_EQ("14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d", containerID);
        matcher->ExtractProcessMeta(GKE_GUARANTEED_PATH, containerID, podID);
        EXPECT_EQ("8dbc5577-d0e2-4706-8787-57d52c03ddf2", podID);
        EXPECT_EQ("14011c7d92a9e513dfd69211da0413dbf319a5e45a02b354ba6e98e10272542d", containerID);
        podID.clear();
        containerID.clear();
        matcher->ExtractProcessMeta(ERRORPATH, containerID, podID);
        EXPECT_EQ("", podID);
        EXPECT_EQ("", containerID);
    }


    void TestExtractPodName() {
        EXPECT_EQ("kube-state-metrics", ExtractPodWorkloadName("kube-state-metrics-86679c945-5rmck"));
        EXPECT_EQ("kube-state-metrics", ExtractPodWorkloadName("kube-state-metrics-86679c9454-5rmck"));
        EXPECT_EQ("kube-state-metrics", ExtractPodWorkloadName("kube-state-metrics-5rmck"));
        EXPECT_EQ("kube-state-metrics", ExtractPodWorkloadName("kube-state-metrics"));
        EXPECT_EQ("", ExtractPodWorkloadName(""));
    }

    void TestExtractMetaPath() {
        std::vector<std::string> res;
        ResolveAllCGroupProcsPaths(CGroupBasePath("."), res);
        for (const auto& item : res) {
            std::cout << item << std::endl;
        }
        auto iter = std::find(
            res.begin(),
            res.end(),
            "./cgroup/cpu,cpuacct/kubepods.slice/kubepods-burstable.slice/"
            "kubepods-burstable-pod28010fb2_7fe0_424c_a131_ad8a25a0c80a.slice/"
            "cri-containerd-c89017ad4ef7fd2b029d8d21452d17d9d9ed7443f48f02ef94b4815df6aebe47.scope/cgroup.procs");
        EXPECT_TRUE(iter != res.end());
    }

    void TestOnlyProcessCGroupManager() {
        EXPECT_FALSE(instance->Init("./notExist"));
        instance->GetProcessMeta(1);
        instance->GetProcessMeta(99999991);
        auto data = instance->GetProcessMeta(1)->GetFormattedMeta();
        EXPECT_EQ(data.size(), 3);
        EXPECT_EQ(data[0].first, "_process_pid_");
        EXPECT_EQ(data[0].second, "1");
        EXPECT_EQ(data[1].first, "_process_cmd_");
        EXPECT_TRUE(!data[1].second.empty());

        data = instance->GetProcessMeta(99999991)->GetFormattedMeta();
        EXPECT_EQ(data[0].first, "_process_pid_");
        EXPECT_EQ(data[0].second, "99999991");
        EXPECT_EQ(data[1].first, "_process_cmd_");
        EXPECT_TRUE(data[1].second.empty());

        instance->FlushMetas();

        auto groupPtr1 = instance->GetContainerProcessGroupPtr(instance->GetProcessMeta(1), 1);
        MySQLProtocolEvent MySQLEvent = getEvent(1);
        groupPtr1->mAggregator.GetMySQLAggregator()->AddEvent(std::move(MySQLEvent));

        std::vector<sls_logs::Log> logs;
        auto tags = std::vector<std::pair<std::string, std::string>>{};
        instance->FlushOutMetrics(logs, tags, 1);

        APSARA_TEST_TRUE(logs.size() == 1);
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(
            &logs[0], "local_info", "{\"_process_cmd_\":\"/usr/lib/systemd/systemd\",\"_process_pid_\":\"1\",\"_running_mode_\":\"host\"}"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&logs[0], "query", "select 1"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&logs[0], "status", "0"));
    }

    void TestContainerCGroupManager() {
        EXPECT_TRUE(instance->Init("."));
        instance->FlushMetas();

        auto meta = instance->GetProcessMeta(9999997);
        EXPECT_EQ(meta->PID, 9999997);
        EXPECT_EQ(meta->Pod.PodUUID, "28010fb2_7fe0_424c_a131_ad8a25a0c80a");
        EXPECT_EQ(meta->Container.ContainerID, "c89017ad4ef7fd2b029d8d21452d17d9d9ed7443f48f02ef94b4815df6aebe47");


        meta = instance->GetProcessMeta(9999999);
        EXPECT_EQ(meta->PID, 9999999);
        EXPECT_EQ(meta->Pod.PodUUID, "2b801b7a_5266_4386_864e_45ed71136371");
        EXPECT_EQ(meta->Container.ContainerID, "20e061fc708d3b66dfe257b19552b34a1307a7347ed6b5bd0d8c5e76afb1a870");


        meta = instance->GetProcessMeta(9999998);
        EXPECT_EQ(meta->PID, 9999998);
        EXPECT_EQ(meta->Pod.PodUUID, "2b801b7a_5266_4386_864e_45ed71136371");
        EXPECT_EQ(meta->Container.ContainerID, "20e061fc708d3b66dfe257b19552b34a1307a7347ed6b5bd0d8c5e76afb1a870");

        // test flush logs by container group

        auto groupPtr2 = instance->GetContainerProcessGroupPtr(instance->GetProcessMeta(9999998), 9999998);
        auto groupPtr3 = instance->GetContainerProcessGroupPtr(instance->GetProcessMeta(9999999), 9999999);
        auto groupPtr4 = instance->GetContainerProcessGroupPtr(instance->GetProcessMeta(9999997), 9999997);
        ASSERT_TRUE(groupPtr2 == groupPtr3);

        // mock k8s info
        groupPtr2->mMetaPtr->Pod.PodName = "mock2-pod";
        groupPtr2->mMetaPtr->Pod.NameSpace = "mock2-ns";
        groupPtr2->mMetaPtr->Pod.WorkloadName = "mock2-wn";
        groupPtr2->mMetaPtr->Container.Image = "mock2-image";
        groupPtr2->mMetaPtr->Container.ContainerName = "mock2-container";

        // mock only container info
        groupPtr4->mMetaPtr->Container.Image = "mock4-image";
        groupPtr4->mMetaPtr->Container.ContainerName = "mock4-container";


        MySQLProtocolEvent MySQLEvent2 = getEvent(2);
        groupPtr2->mAggregator.GetMySQLAggregator()->AddEvent(std::move(MySQLEvent2));

        MySQLProtocolEvent MySQLEvent4 = getEvent(4);
        groupPtr4->mAggregator.GetMySQLAggregator()->AddEvent(std::move(MySQLEvent4));

        std::vector<sls_logs::Log> logs;
        auto tags = std::vector<std::pair<std::string, std::string>>{};
        instance->FlushOutMetrics(logs, tags, 1);
        APSARA_TEST_TRUE(logs.size() == 2);

        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&logs[0], "local_info", "{\"_container_name_\":\"mock4-container\",\"_running_mode_\":\"container\"}"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&logs[0], "query", "select 4"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&logs[0], "status", "0"));

        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&logs[1], "local_info", "{\"_container_name_\":\"mock2-container\",\"_namespace_\":\"mock2-ns\",\"_pod_name_\":\"mock2-pod\",\"_running_mode_\":\"kubernetes\",\"_workload_name_\":\"mock2-wn\"}"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&logs[1], "query", "select 2"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&logs[1], "status", "0"));
    }

    static MySQLProtocolEvent getEvent(int32_t i) {
        MySQLProtocolEvent MySQLEvent;
        MySQLEvent.Info.LatencyNs = i;
        MySQLEvent.Info.ReqBytes = i;
        MySQLEvent.Info.RespBytes = i;
        MySQLEvent.Key.Query = "select " + std::to_string(i);
        MySQLEvent.Key.Status = 0;
        MySQLEvent.Key.ConnKey.Role = PacketRoleType::Server;
        return MySQLEvent;
    }


    void TestPassFilterRules() {
        auto cfg = NetworkConfig::GetInstance();

        cfg->mIncludeCmdRegex = boost::regex("^(test|abc)$");
        cfg->mExcludeCmdRegex = boost::regex("^abc");


        // valid cmd
        ProcessMeta meta;
        meta.PID = 1;
        meta.ProcessCMD = "test";
        APSARA_TEST_TRUE(meta.PassFilterRules());
        meta.mPassFilterRules = 0;

        meta.ProcessCMD = "abc";
        APSARA_TEST_TRUE(!meta.PassFilterRules());
        meta.mPassFilterRules = 0;

        meta.ProcessCMD = "abc1";
        APSARA_TEST_TRUE(!meta.PassFilterRules());
        meta.mPassFilterRules = 0;

        // valid container
        meta.Container.ContainerName = "container1";
        meta.Container.Labels.insert(std::make_pair("app", "container"));

        cfg->mIncludeContainerNameRegex = boost::regex("^(container1|container2)$");
        APSARA_TEST_TRUE(meta.PassFilterRules());
        meta.mPassFilterRules = 0;
        cfg->mIncludeContainerNameRegex = boost::regex();

        cfg->mExcludeContainerNameRegex = boost::regex("^(container1|container)$");
        APSARA_TEST_TRUE(!meta.PassFilterRules());
        meta.mPassFilterRules = 0;
        cfg->mExcludeContainerNameRegex = boost::regex();

        cfg->mIncludeContainerLabels.insert(std::make_pair("app", boost::regex("^(container|abc)$")));
        APSARA_TEST_TRUE(meta.PassFilterRules());
        meta.mPassFilterRules = 0;
        cfg->mIncludeContainerLabels.clear();

        cfg->mExcludeContainerLabels.insert(std::make_pair("app", boost::regex("^(container)$")));
        APSARA_TEST_TRUE(!meta.PassFilterRules());
        meta.mPassFilterRules = 0;
        cfg->mExcludeContainerLabels.clear();

        // valid pod
        meta.Pod.PodName = "podname1";
        meta.Pod.NameSpace = "namespace1";
        meta.Pod.Labels.insert(std::make_pair("app", "pod"));

        cfg->mIncludePodNameRegex = boost::regex("^(podname1|namespace2)$");
        APSARA_TEST_TRUE(meta.PassFilterRules());
        meta.mPassFilterRules = 0;
        cfg->mIncludePodNameRegex = boost::regex();

        cfg->mExcludePodNameRegex = boost::regex("^(podname1|namespace2)$");
        APSARA_TEST_TRUE(!meta.PassFilterRules());
        meta.mPassFilterRules = 0;
        cfg->mExcludePodNameRegex = boost::regex();

        cfg->mIncludeNamespaceNameRegex = boost::regex("^(namespace1|namespace2)$");
        APSARA_TEST_TRUE(meta.PassFilterRules());
        meta.mPassFilterRules = 0;
        cfg->mIncludeNamespaceNameRegex = boost::regex();

        cfg->mExcludeNamespaceNameRegex = boost::regex("^(namespace1|namespace2)$");
        APSARA_TEST_TRUE(!meta.PassFilterRules());
        meta.mPassFilterRules = 0;
        cfg->mExcludeNamespaceNameRegex = boost::regex();

        cfg->mIncludeK8sLabels.insert(std::make_pair("app", boost::regex("^(pod|abc)$")));
        APSARA_TEST_TRUE(meta.PassFilterRules());
        meta.mPassFilterRules = 0;
        cfg->mIncludeK8sLabels.clear();

        cfg->mExcludeK8sLabels.insert(std::make_pair("app", boost::regex("^(pod)$")));
        APSARA_TEST_TRUE(!meta.PassFilterRules());
        meta.mPassFilterRules = 0;
        cfg->mExcludeK8sLabels.clear();
    }

    ContainerProcessGroupManager* instance = ContainerProcessGroupManager::GetInstance();
};

UNIT_TEST_CASE(CGroupPathResolverUnittest, TestPassFilterRules)

UNIT_TEST_CASE(CGroupPathResolverUnittest, TestCGroupExtractContainerType)

UNIT_TEST_CASE(CGroupPathResolverUnittest, TestExtractDockerMeta)

UNIT_TEST_CASE(CGroupPathResolverUnittest, TestExtractContainerdMeta)

UNIT_TEST_CASE(CGroupPathResolverUnittest, TestExtractPodName)

UNIT_TEST_CASE(CGroupPathResolverUnittest, TestExtractGKEMeta)

UNIT_TEST_CASE(CGroupPathResolverUnittest, TestExtractMetaPath)

UNIT_TEST_CASE(CGroupPathResolverUnittest, TestOnlyProcessCGroupManager)

UNIT_TEST_CASE(CGroupPathResolverUnittest, TestContainerCGroupManager)

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}