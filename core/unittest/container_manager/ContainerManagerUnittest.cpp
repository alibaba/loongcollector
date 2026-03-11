// Copyright 2023 iLogtail Authors
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


#include <cstdlib>

#include <algorithm>
#include <atomic>
#include <boost/regex.hpp>
#include <chrono>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "gtest/gtest.h"
#include "json/json.h"

#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "common/RuntimeUtil.h"
#include "container_manager/ContainerDiscoveryOptions.h"
#include "container_manager/ContainerManager.h"
#include "file_server/FileDiscoveryOptions.h"
#include "file_server/FileServer.h"
#include "unittest/Unittest.h"
#include "unittest/pipeline/LogtailPluginMock.h"

using namespace std;

namespace logtail {

class ContainerManagerUnittest : public testing::Test {
public:
    void TestcomputeMatchedContainersDiff() const;
    void TestrefreshAllContainersSnapshot() const;
    void TestincrementallyUpdateContainersSnapshot() const;
    void TestSaveLoadContainerInfo() const;
    void TestLoadContainerInfoFromDetailFormat() const;
    void TestLoadContainerInfoFromDetailFormatWithTags() const;
    void TestLoadContainerInfoFromDetailFormatWithMounts() const;
    void TestLoadContainerInfoFromDetailFormatMixed() const;
    void TestLoadContainerInfoFromDetailFormatPathHandling() const;
    void TestLoadContainerInfoFromContainersFormat() const;
    void TestLoadContainerInfoVersionHandling() const;
    void TestSaveContainerInfoWithVersion() const;
    void TestContainerMatchingConsistency() const;
    void TestcomputeMatchedContainersDiffWithSnapshot() const;
    void TestNullRawContainerInfoHandling() const;
    void TestConcurrentContainerMapAccess_T1T2();
    void TestConcurrentContainerMapAccess_T1T3();
    void TestConcurrentContainerMapAccess_T1T2T3();
    void TestSequentialContainerDiffAndApply();
    void runTestFile(const std::string& testFilePath) const;

private:
    FileDiscoveryOptions mDiscoveryOpts;
    CollectionPipelineContext ctx;

    void parseLabelFilters(const Json::Value& filtersJson, MatchCriteriaFilter& filter) const;
    bool parseContainerFilterConfigFromTestJson(const Json::Value& filterJson, ContainerFilterConfig& config) const;
    std::string findTestDataDirectory() const;
    void runConcurrentContainerMapAccessTest(bool enableT2, bool enableT3, const std::string& testName);
};

void ContainerManagerUnittest::TestcomputeMatchedContainersDiff() const {
    ContainerManager containerManager;

    std::set<std::string> fullList;
    std::vector<std::string> removedList;
    std::vector<std::string> matchAddedList;
    std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> matchList;
    {
        // test empty filter
        ContainerFilters filters;
        ContainerDiff diff;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, false, diff);
        EXPECT_EQ(fullList.size(), 0);
        EXPECT_EQ(matchList.size(), 0);
    }

    {
        // test modified containers: existing in matchList and fullList, info changed -> mModified
        containerManager.mContainerMap.clear();
        std::set<std::string> fullList2;
        std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> matchList2;
        ContainerFilters filters;

        RawContainerInfo oldInfo;
        oldInfo.mID = "mod1";
        oldInfo.mLogPath = "/var/lib/docker/containers/mod1/logs";
        matchList2["mod1"] = std::make_shared<RawContainerInfo>(oldInfo);
        fullList2.insert("mod1");

        RawContainerInfo newInfo;
        newInfo.mID = "mod1";
        newInfo.mLogPath = "/var/lib/docker/containers/mod1/new-logs"; // changed
        containerManager.mContainerMap["mod1"] = std::make_shared<RawContainerInfo>(newInfo);

        ContainerDiff diff;
        containerManager.computeMatchedContainersDiff(fullList2, matchList2, filters, false, false, diff);
        EXPECT_EQ(diff.mModified.size(), 1);
        EXPECT_EQ(diff.mModified[0]->mLogPath, std::string("/var/lib/docker/containers/mod1/new-logs"));
    }

    {
        // test removed containers: id not in mContainerMap but present in fullList & matchList -> mRemoved
        containerManager.mContainerMap.clear();
        std::set<std::string> fullList3;
        std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> matchList3;
        ContainerFilters filters;

        RawContainerInfo info;
        info.mID = "gone1";
        info.mLogPath = "/var/lib/docker/containers/gone1/logs";
        matchList3["gone1"] = std::make_shared<RawContainerInfo>(info);
        fullList3.insert("gone1");
        // ensure it's not in mContainerMap
        containerManager.mContainerMap.erase("gone1");

        ContainerDiff diff;
        containerManager.computeMatchedContainersDiff(fullList3, matchList3, filters, false, false, diff);
        EXPECT_EQ(std::count(diff.mRemoved.begin(), diff.mRemoved.end(), std::string("gone1")), 1);
        EXPECT_EQ(fullList3.count("gone1"), 0);
    }

    {
        // test env filter
        containerManager.mContainerMap.clear();
        ContainerFilters filters;
        RawContainerInfo containerInfo1;
        containerInfo1.mID = "123";
        containerInfo1.mLogPath = "/var/lib/docker/containers/123/logs";
        containerInfo1.mEnv["test"] = "test";
        containerInfo1.mStatus = "running";
        containerManager.mContainerMap["123"] = std::make_shared<RawContainerInfo>(containerInfo1);

        RawContainerInfo containerInfo2;
        containerInfo2.mID = "1234";
        containerInfo2.mLogPath = "/var/lib/docker/containers/1234/logs";
        containerInfo2.mEnv["test"] = "test2";
        containerInfo2.mStatus = "running";
        containerManager.mContainerMap["1234"] = std::make_shared<RawContainerInfo>(containerInfo2);

        matchList.clear();
        fullList.clear();

        filters.mEnvFilter.mIncludeFields.mFieldsMap["test"] = "test";
        ContainerDiff diff;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, false, diff);
        EXPECT_EQ(fullList.size(), 2);
        EXPECT_EQ(diff.mAdded.size(), 1);
        if (diff.mAdded.size() > 0) {
            EXPECT_EQ(diff.mAdded[0]->mID, "123");
        }
    }

    {
        // test env exclude filter (exclude key=value)
        ContainerFilters filters;
        matchList.clear();
        fullList.clear();

        // exclude key "test" with value "test" so only 1234 is added
        filters.mEnvFilter.mExcludeFields.mFieldsMap["test"] = "test";
        ContainerDiff diff;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, false, diff);
        EXPECT_EQ(fullList.size(), 2);
        EXPECT_EQ(diff.mAdded.size(), 1);
        if (diff.mAdded.size() > 0) {
            EXPECT_EQ(diff.mAdded[0]->mID, "1234");
        }
    }

    {
        // test env include regex
        ContainerFilters filters;
        matchList.clear();
        fullList.clear();

        filters.mEnvFilter.mIncludeFields.mFieldsRegMap["test"] = std::make_shared<boost::regex>("^test2$");
        ContainerDiff diff;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, false, diff);
        EXPECT_EQ(fullList.size(), 2);
        EXPECT_EQ(diff.mAdded.size(), 1);
        if (diff.mAdded.size() > 0) {
            EXPECT_EQ(diff.mAdded[0]->mID, "1234");
        }
    }

    {
        // test env exclude regex
        ContainerFilters filters;
        matchList.clear();
        fullList.clear();

        filters.mEnvFilter.mExcludeFields.mFieldsRegMap["test"] = std::make_shared<boost::regex>("^test2$");
        ContainerDiff diff;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, false, diff);
        EXPECT_EQ(fullList.size(), 2);
        EXPECT_EQ(diff.mAdded.size(), 1);
        if (diff.mAdded.size() > 0) {
            EXPECT_EQ(diff.mAdded[0]->mID, "123");
        }
    }

    {
        // test k8s filter
        containerManager.mContainerMap.clear();
        ContainerFilters filters;
        RawContainerInfo containerInfo1;
        containerInfo1.mID = "123";
        containerInfo1.mLogPath = "/var/lib/docker/containers/123/logs";
        containerInfo1.mK8sInfo.mPod = "pod1";
        containerInfo1.mK8sInfo.mNamespace = "namespace1";
        containerInfo1.mK8sInfo.mContainerName = "container1";
        containerInfo1.mStatus = "running";
        containerManager.mContainerMap["123"] = std::make_shared<RawContainerInfo>(containerInfo1);

        RawContainerInfo containerInfo2;
        containerInfo2.mID = "1234";
        containerInfo2.mLogPath = "/var/lib/docker/containers/1234/logs";
        containerInfo2.mK8sInfo.mPod = "pod2";
        containerInfo2.mK8sInfo.mNamespace = "namespace2";
        containerInfo2.mK8sInfo.mContainerName = "container2";
        containerInfo2.mStatus = "running";
        containerManager.mContainerMap["1234"] = std::make_shared<RawContainerInfo>(containerInfo2);

        matchList.clear();
        fullList.clear();

        filters.mK8SFilter.mPodReg = std::make_shared<boost::regex>("^pod1$");
        filters.mK8SFilter.mNamespaceReg = std::make_shared<boost::regex>("^namespace1$");
        filters.mK8SFilter.mContainerReg = std::make_shared<boost::regex>("^container1$");
        ContainerDiff diff;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, false, diff);
        EXPECT_EQ(fullList.size(), 2);
        EXPECT_EQ(diff.mAdded.size(), 1);
        if (diff.mAdded.size() > 0) {
            EXPECT_EQ(diff.mAdded[0]->mID, "123");
        }
    }

    {
        // test k8s label include/exclude
        containerManager.mContainerMap.clear();
        ContainerFilters filters;
        RawContainerInfo k8s1;
        k8s1.mID = "k8s1";
        k8s1.mLogPath = "/var/lib/docker/containers/k8s1/logs";
        k8s1.mK8sInfo.mLabels["tier"] = "frontend";
        containerManager.mContainerMap["k8s1"] = std::make_shared<RawContainerInfo>(k8s1);

        RawContainerInfo k8s2;
        k8s2.mID = "k8s2";
        k8s2.mLogPath = "/var/lib/docker/containers/k8s2/logs";
        k8s2.mK8sInfo.mLabels["tier"] = "backend";
        containerManager.mContainerMap["k8s2"] = std::make_shared<RawContainerInfo>(k8s2);

        matchList.clear();
        fullList.clear();

        // include label
        filters.mK8SFilter.mK8sLabelFilter.mIncludeFields.mFieldsMap["tier"] = "frontend";
        ContainerDiff diff1;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, true, false, diff1);
        EXPECT_EQ(fullList.count("k8s1") + fullList.count("k8s2"), 2);
        EXPECT_EQ(diff1.mAdded.size(), 1);
        if (diff1.mAdded.size() > 0) {
            EXPECT_EQ(diff1.mAdded[0]->mID, "k8s1");
        }

        // exclude label
        matchList.clear();
        fullList.clear();
        ContainerFilters filters2;
        filters2.mK8SFilter.mK8sLabelFilter.mExcludeFields.mFieldsMap["tier"] = "backend";
        ContainerDiff diff2;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters2, true, false, diff2);
        EXPECT_EQ(fullList.count("k8s1") + fullList.count("k8s2"), 2);
        EXPECT_EQ(diff2.mAdded.size(), 1);
        if (diff2.mAdded.size() > 0) {
            EXPECT_EQ(diff2.mAdded[0]->mID, "k8s1");
        }

        // include regex
        matchList.clear();
        fullList.clear();
        ContainerFilters filters3;
        filters3.mK8SFilter.mK8sLabelFilter.mIncludeFields.mFieldsRegMap["tier"]
            = std::make_shared<boost::regex>("^front.*$");
        ContainerDiff diff3;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters3, true, false, diff3);
        EXPECT_EQ(diff3.mAdded.size(), 1);
        if (diff3.mAdded.size() > 0) {
            EXPECT_EQ(diff3.mAdded[0]->mID, "k8s1");
        }
    }

    {
        // test container label filters include/exclude and regex
        containerManager.mContainerMap.clear();
        ContainerFilters filters;
        RawContainerInfo cl1;
        cl1.mID = "cl1";
        cl1.mLogPath = "/var/lib/docker/containers/cl1/logs";
        cl1.mContainerLabels["app"] = "nginx";
        containerManager.mContainerMap["cl1"] = std::make_shared<RawContainerInfo>(cl1);

        RawContainerInfo cl2;
        cl2.mID = "cl2";
        cl2.mLogPath = "/var/lib/docker/containers/cl2/logs";
        cl2.mContainerLabels["app"] = "redis";
        containerManager.mContainerMap["cl2"] = std::make_shared<RawContainerInfo>(cl2);

        matchList.clear();
        fullList.clear();

        // include map
        filters.mContainerLabelFilter.mIncludeFields.mFieldsMap["app"] = "nginx";
        ContainerDiff diff1;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, true, false, diff1);
        EXPECT_EQ(diff1.mAdded.size(), 1);
        if (diff1.mAdded.size() > 0) {
            EXPECT_EQ(diff1.mAdded[0]->mID, "cl1");
        }

        // exclude map
        matchList.clear();
        fullList.clear();
        ContainerFilters filters2;
        filters2.mContainerLabelFilter.mExcludeFields.mFieldsMap["app"] = "nginx";
        ContainerDiff diff2;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters2, true, false, diff2);
        EXPECT_EQ(diff2.mAdded.size(), 1);
        if (diff2.mAdded.size() > 0) {
            EXPECT_EQ(diff2.mAdded[0]->mID, "cl2");
        }

        // include regex
        matchList.clear();
        fullList.clear();
        ContainerFilters filters3;
        filters3.mContainerLabelFilter.mIncludeFields.mFieldsRegMap["app"] = std::make_shared<boost::regex>("^ng.*$");
        ContainerDiff diff3;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters3, true, false, diff3);
        EXPECT_EQ(diff3.mAdded.size(), 1);
        if (diff3.mAdded.size() > 0) {
            EXPECT_EQ(diff3.mAdded[0]->mID, "cl1");
        }

        // exclude regex
        matchList.clear();
        fullList.clear();
        ContainerFilters filters4;
        filters4.mContainerLabelFilter.mExcludeFields.mFieldsRegMap["app"] = std::make_shared<boost::regex>("^re.*$");
        ContainerDiff diff4;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters4, true, false, diff4);
        EXPECT_EQ(diff4.mAdded.size(), 1);
        if (diff4.mAdded.size() > 0) {
            EXPECT_EQ(diff4.mAdded[0]->mID, "cl1");
        }
    }

    {
        // test combined filters: env + container label + k8s (namespace/pod/container + labels)
        containerManager.mContainerMap.clear();
        ContainerFilters filters;
        RawContainerInfo combo1;
        combo1.mID = "combo1";
        combo1.mLogPath = "/var/lib/docker/containers/combo1/logs";
        combo1.mEnv["env"] = "prod";
        combo1.mContainerLabels["app"] = "nginx";
        combo1.mK8sInfo.mNamespace = "ns1";
        combo1.mK8sInfo.mPod = "pod1";
        combo1.mK8sInfo.mContainerName = "c1";
        combo1.mK8sInfo.mLabels["tier"] = "frontend";
        containerManager.mContainerMap["combo1"] = std::make_shared<RawContainerInfo>(combo1);

        RawContainerInfo combo2;
        combo2.mID = "combo2";
        combo2.mLogPath = "/var/lib/docker/containers/combo2/logs";
        combo2.mEnv["env"] = "dev";
        combo2.mContainerLabels["app"] = "nginx";
        combo2.mK8sInfo.mNamespace = "ns1";
        combo2.mK8sInfo.mPod = "pod1";
        combo2.mK8sInfo.mContainerName = "c1";
        combo2.mK8sInfo.mLabels["tier"] = "frontend";
        containerManager.mContainerMap["combo2"] = std::make_shared<RawContainerInfo>(combo2);

        matchList.clear();
        fullList.clear();

        filters.mEnvFilter.mIncludeFields.mFieldsMap["env"] = "prod";
        filters.mContainerLabelFilter.mIncludeFields.mFieldsMap["app"] = "nginx";
        filters.mK8SFilter.mNamespaceReg = std::make_shared<boost::regex>("^ns1$");
        filters.mK8SFilter.mPodReg = std::make_shared<boost::regex>("^pod1$");
        filters.mK8SFilter.mContainerReg = std::make_shared<boost::regex>("^c1$");
        filters.mK8SFilter.mK8sLabelFilter.mIncludeFields.mFieldsMap["tier"] = "frontend";

        ContainerDiff diff;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, true, false, diff);
        EXPECT_EQ(diff.mAdded.size(), 1);
        if (diff.mAdded.size() > 0) {
            EXPECT_EQ(diff.mAdded[0]->mID, "combo1");
        }
    }
}

void ContainerManagerUnittest::TestrefreshAllContainersSnapshot() const {
    {
        // test empty containers meta
        LogtailPluginMock::GetInstance()->SetUpContainersMeta("");
        ContainerManager containerManager;
        containerManager.refreshAllContainersSnapshot();
        EXPECT_EQ(containerManager.mContainerMap.size(), 0);
    }
    {
        // test empty containers meta
        LogtailPluginMock::GetInstance()->SetUpContainersMeta(R"({
	"All": [{
		"ID": "9c7da0bc25f57de99283456960072b7f5ebc069599c6e5efec567c3e6e70ca93",
		"LogPath": "/var/lib/docker/containers/9c7da0bc25f57de99283456960072b7f5ebc069599c6e5efec567c3e6e70ca93/9c7da0bc25f57de99283456960072b7f5ebc069599c6e5efec567c3e6e70ca93-json.log",
		"MetaDatas": ["_namespace_", "kube-system", "_pod_uid_", "4991ae55-8a3c-4228-9668-4d4feb748ad1", "_image_name_", "aliyun-observability-release-registry.cn-shanghai.cr.aliyuncs.com/loongcollector-dev/logtail:v3.1.0.0-f57a0e2-aliyun-0612", "_container_name_", "loongcollector", "_pod_name_", "loongcollector-ds-4glk5"],
		"Mounts": [{
			"Destination": "/logtail_host",
			"Source": "/"
		}, {
			"Destination": "/sys",
			"Source": "/sys"
		}, {
			"Destination": "/etc/ilogtail/checkpoint",
			"Source": "/var/lib/kube-system-logtail-ds/checkpoint"
		}, {
			"Destination": "/etc/ilogtail/config",
			"Source": "/var/lib/kube-system-logtail-ds/config"
		}, {
			"Destination": "/etc/ilogtail/instance_config",
			"Source": "/var/lib/kube-system-logtail-ds/instance_config"
		}, {
			"Destination": "/dev/termination-log",
			"Source": "/var/lib/kubelet/pods/4991ae55-8a3c-4228-9668-4d4feb748ad1/containers/loongcollector/f6f3d3b1"
		}, {
			"Destination": "/etc/hosts",
			"Source": "/var/lib/kubelet/pods/4991ae55-8a3c-4228-9668-4d4feb748ad1/etc-hosts"
		}, {
			"Destination": "/usr/local/ilogtail/apsara_log_conf.json",
			"Source": "/var/lib/kubelet/pods/4991ae55-8a3c-4228-9668-4d4feb748ad1/volume-subpaths/log-config/loongcollector/8"
		}, {
			"Destination": "/usr/local/ilogtail/ilogtail_config.json",
			"Source": "/var/lib/kubelet/pods/4991ae55-8a3c-4228-9668-4d4feb748ad1/volume-subpaths/loongcollector-config/loongcollector/5"
		}, {
			"Destination": "/etc/init.d/loongcollectord",
			"Source": "/var/lib/kubelet/pods/4991ae55-8a3c-4228-9668-4d4feb748ad1/volume-subpaths/loongcollectord/loongcollector/9"
		}, {
			"Destination": "/var/run/secrets/kubernetes.io/serviceaccount",
			"Source": "/var/lib/kubelet/pods/4991ae55-8a3c-4228-9668-4d4feb748ad1/volumes/kubernetes.io~projected/kube-api-access-zp49x"
		}, {
			"Destination": "/var/addon",
			"Source": "/var/lib/kubelet/pods/4991ae55-8a3c-4228-9668-4d4feb748ad1/volumes/kubernetes.io~secret/addon-token"
		}, {
			"Destination": "/var/run",
			"Source": "/var/run"
		}],
		"Path": "/logtail_host/var/lib/docker/overlay2/04eb45c706a8136171b3fb68e242c8fd19a7e053e2262fada30890d1b326cf3c/diff/usr/local/ilogtail/self_metrics",
		"Tags": [],
		"UpperDir": "/var/lib/docker/overlay2/04eb45c706a8136171b3fb68e242c8fd19a7e053e2262fada30890d1b326cf3c/diff"
	}]
})");
        ContainerManager containerManager;
        containerManager.refreshAllContainersSnapshot();
        EXPECT_EQ(containerManager.mContainerMap.size(), 1);
    }
}

void ContainerManagerUnittest::TestincrementallyUpdateContainersSnapshot() const {
    {
        // test empty diff containers meta
        LogtailPluginMock::GetInstance()->SetUpDiffContainersMeta("");
        ContainerManager containerManager;
        containerManager.incrementallyUpdateContainersSnapshot();
        EXPECT_EQ(containerManager.mContainerMap.size(), 0);
        EXPECT_EQ(containerManager.mStoppedContainerIDs.size(), 0);
    }
    {
        // test diff containers meta
        LogtailPluginMock::GetInstance()->SetUpDiffContainersMeta(R"({
                "Update": [
                    {
                        "ID": "123",
                        "UpperDir": "/var/lib/docker/containers/123",
                        "LogPath": "/var/lib/docker/containers/123/logs"
                    },
                    {
                        "ID": "1234",
                        "UpperDir": "/var/lib/docker/containers/1234",
                        "LogPath": "/var/lib/docker/containers/1234/logs"
                    }
                ],
                "Delete": [123],
                "Stop": ["123"]
            }
        )");
        ContainerManager containerManager;
        containerManager.incrementallyUpdateContainersSnapshot();
        EXPECT_EQ(containerManager.mContainerMap.size(), 1);
        EXPECT_EQ(containerManager.mStoppedContainerIDs.size(), 1);
        EXPECT_EQ(containerManager.mStoppedContainerIDs[0], "123");
    }
}

void ContainerManagerUnittest::TestSaveLoadContainerInfo() const {
    ContainerManager containerManager;

    // prepare two containers
    RawContainerInfo containerInfo1;
    containerInfo1.mID = "save1";
    containerInfo1.mUpperDir = "/upper/save1";
    containerInfo1.mLogPath = "/log/save1";
    containerManager.mContainerMap["save1"] = std::make_shared<RawContainerInfo>(containerInfo1);

    RawContainerInfo containerInfo2;
    containerInfo2.mID = "save2";
    containerInfo2.mUpperDir = "/upper/save2";
    containerInfo2.mLogPath = "/log/save2";
    containerManager.mContainerMap["save2"] = std::make_shared<RawContainerInfo>(containerInfo2);

    // save to file
    containerManager.SaveContainerInfo();

    // clear and reload
    containerManager.mContainerMap.clear();
    EXPECT_EQ(containerManager.mContainerMap.size(), 0);

    containerManager.LoadContainerInfo();

    EXPECT_EQ(containerManager.mContainerMap.size(), 2);
    auto it1 = containerManager.mContainerMap.find("save1");
    auto it2 = containerManager.mContainerMap.find("save2");
    EXPECT_EQ(it1 != containerManager.mContainerMap.end(), true);
    EXPECT_EQ(it2 != containerManager.mContainerMap.end(), true);
    EXPECT_EQ(it1->second->mLogPath, std::string("/log/save1"));
    EXPECT_EQ(it2->second->mUpperDir, std::string("/upper/save2"));
}

void ContainerManagerUnittest::TestLoadContainerInfoFromDetailFormat() const {
    ContainerManager containerManager;

    // Prepare test data in v0.1.0 format (detail array with params)
    Json::Value root;
    root["version"] = "0.1.0";

    Json::Value detailArray(Json::arrayValue);
    Json::Value item1(Json::objectValue);
    item1["config_name"] = "##1.0##config1";
    item1["container_id"] = "test1";
    item1["params"] = R"({
        "ID": "test1",
        "LogPath": "/var/log/containers/test1.log",
        "UpperDir": "/var/lib/docker/overlay2/test1",
        "MetaDatas": ["_namespace_", "default", "_pod_name_", "test-pod", "_container_name_", "test-container"]
    })";

    Json::Value item2(Json::objectValue);
    item2["config_name"] = "##1.0##config2";
    item2["container_id"] = "test2";
    item2["params"] = R"({
        "ID": "test2",
        "LogPath": "/var/log/containers/test2.log",
        "UpperDir": "/var/lib/docker/overlay2/test2"
    })";

    detailArray.append(item1);
    detailArray.append(item2);
    root["detail"] = detailArray;

    // Test loading from detail format
    containerManager.loadContainerInfoFromDetailFormat(root, "test_path");

    // Verify containers are loaded
    EXPECT_EQ(containerManager.mContainerMap.size(), 2);
    auto it1 = containerManager.mContainerMap.find("test1");
    auto it2 = containerManager.mContainerMap.find("test2");
    EXPECT_TRUE(it1 != containerManager.mContainerMap.end());
    EXPECT_TRUE(it2 != containerManager.mContainerMap.end());

    // Verify container info
    EXPECT_EQ(it1->second->mID, "test1");
    EXPECT_EQ(it1->second->mLogPath, "/var/log/containers/test1.log");
    EXPECT_EQ(it1->second->mUpperDir, "/var/lib/docker/overlay2/test1");
    EXPECT_EQ(it1->second->mK8sInfo.mNamespace, "default");
    EXPECT_EQ(it1->second->mK8sInfo.mPod, "test-pod");
    EXPECT_EQ(it1->second->mK8sInfo.mContainerName, "test-container");

    // Verify metadata by checking the vectors
    bool foundNamespace = false, foundPodName = false, foundContainerName = false, foundImageName = false;
    for (const auto& metadata : it1->second->mMetadatas) {
        if (GetDefaultTagKeyString(metadata.first) == "_namespace_" && metadata.second == "default")
            foundNamespace = true;
        else if (GetDefaultTagKeyString(metadata.first) == "_pod_name_" && metadata.second == "test-pod")
            foundPodName = true;
        else if (GetDefaultTagKeyString(metadata.first) == "_container_name_" && metadata.second == "test-container")
            foundContainerName = true;
        else if (GetDefaultTagKeyString(metadata.first) == "_image_name_")
            foundImageName = true;
    }

    EXPECT_TRUE(foundNamespace);
    EXPECT_TRUE(foundPodName);
    EXPECT_TRUE(foundContainerName);
    EXPECT_FALSE(foundImageName);

    // Verify config diffs are created
    EXPECT_TRUE(containerManager.mConfigContainerDiffMap.find("##1.0##config1")
                != containerManager.mConfigContainerDiffMap.end());
    EXPECT_TRUE(containerManager.mConfigContainerDiffMap.find("##1.0##config2")
                != containerManager.mConfigContainerDiffMap.end());
}

void ContainerManagerUnittest::TestLoadContainerInfoFromDetailFormatWithTags() const {
    ContainerManager containerManager;

    // Test data with Tags format (legacy format)
    Json::Value root;
    root["version"] = "0.1.0";

    Json::Value detailArray(Json::arrayValue);
    Json::Value item1(Json::objectValue);
    item1["config_name"] = "##1.0##k8s-log-c84f2ea7e4b1641c882a67ea014247720$file-test";
    item1["container_id"] = "2e832614c7e4ca22a79d5735fc7bf2b77d76d1dabc18cd5d6fc47764394a97c0";
    item1["params"] = R"({
        "ID": "2e832614c7e4ca22a79d5735fc7bf2b77d76d1dabc18cd5d6fc47764394a97c0",
        "Path": "/logtail_host/run/containerd/io.containerd.runtime.v2.task/k8s.io/2e832614c7e4ca22a79d5735fc7bf2b77d76d1dabc18cd5d6fc47764394a97c0/rootfs/app/logs",
        "Tags": [
            "_container_name_",
            "external-snapshot-controller",
            "_pod_name_",
            "csi-provisioner-5f5d9d84fc-hc6mt",
            "_namespace_",
            "kube-system",
            "_pod_uid_",
            "21d7468a-dc19-44fa-81ad-c799f21df5ff",
            "_container_ip_",
            "192.168.0.48",
            "_image_name_",
            "registry-cn-hongkong-vpc.ack.aliyuncs.com/acs/snapshot-controller:v4.0.0-a230d5b3-aliyun"
        ]
    })";

    detailArray.append(item1);
    root["detail"] = detailArray;

    // Test loading from detail format with Tags
    containerManager.loadContainerInfoFromDetailFormat(root, "test_path");

    // Verify container is loaded
    EXPECT_EQ(containerManager.mContainerMap.size(), 1);
    auto it = containerManager.mContainerMap.find("2e832614c7e4ca22a79d5735fc7bf2b77d76d1dabc18cd5d6fc47764394a97c0");
    EXPECT_TRUE(it != containerManager.mContainerMap.end());

    // Verify container info
    EXPECT_EQ(it->second->mID, "2e832614c7e4ca22a79d5735fc7bf2b77d76d1dabc18cd5d6fc47764394a97c0");
    EXPECT_EQ(it->second->mUpperDir, "");
    EXPECT_EQ(it->second->mK8sInfo.mNamespace, "kube-system");
    EXPECT_EQ(it->second->mK8sInfo.mPod, "csi-provisioner-5f5d9d84fc-hc6mt");
    EXPECT_EQ(it->second->mK8sInfo.mContainerName, "external-snapshot-controller");
    EXPECT_EQ(it->second->mContainerLabels["_image_name_"],
              "registry-cn-hongkong-vpc.ack.aliyuncs.com/acs/snapshot-controller:v4.0.0-a230d5b3-aliyun");
    EXPECT_EQ(it->second->mContainerLabels["_container_ip_"], "192.168.0.48");

    // Verify metadata is stored
    bool foundNamespace = false, foundPodName = false, foundContainerName = false, foundImageName = false,
         foundContainerIp = false, foundPodUid = false;
    for (const auto& metadata : it->second->mMetadatas) {
        std::string key = GetDefaultTagKeyString(metadata.first);
        if (key == "_namespace_" && metadata.second == "kube-system")
            foundNamespace = true;
        else if (key == "_pod_name_" && metadata.second == "csi-provisioner-5f5d9d84fc-hc6mt")
            foundPodName = true;
        else if (key == "_container_name_" && metadata.second == "external-snapshot-controller")
            foundContainerName = true;
        else if (key == "_image_name_")
            foundImageName = true;
        else if (key == "_container_ip_" && metadata.second == "192.168.0.48")
            foundContainerIp = true;
        else if (key == "_pod_uid_" && metadata.second == "21d7468a-dc19-44fa-81ad-c799f21df5ff")
            foundPodUid = true;
    }

    EXPECT_TRUE(foundNamespace);
    EXPECT_TRUE(foundPodName);
    EXPECT_TRUE(foundContainerName);
    EXPECT_TRUE(foundImageName);
    EXPECT_TRUE(foundContainerIp);
    EXPECT_TRUE(foundPodUid);

    // Verify config diff is created
    EXPECT_TRUE(
        containerManager.mConfigContainerDiffMap.find("##1.0##k8s-log-c84f2ea7e4b1641c882a67ea014247720$file-test")
        != containerManager.mConfigContainerDiffMap.end());
}

void ContainerManagerUnittest::TestLoadContainerInfoFromDetailFormatWithMounts() const {
    ContainerManager containerManager;

    // Test data with MetaDatas and Mounts (new format)
    Json::Value root;
    root["version"] = "0.1.0";

    Json::Value detailArray(Json::arrayValue);
    Json::Value item1(Json::objectValue);
    item1["config_name"] = "##1.0##k8s-log-c84f2ea7e4b1641c882a67ea014247720$file-test";
    item1["container_id"] = "8859e31bc57dae7cf750cc2353204c6b4d1fa9a40e4eb26fb4c5257be81dcf44";
    item1["params"] = R"({
        "ID": "8859e31bc57dae7cf750cc2353204c6b4d1fa9a40e4eb26fb4c5257be81dcf44",
        "LogPath": "/var/log/pods/dynatrace_dynatrace-webhook-5676d99f4-5dl6n_bddb70c4-e44d-4685-b429-4150aaa51eea/webhook/1.log",
        "MetaDatas": [
            "_pod_name_",
            "dynatrace-webhook-5676d99f4-5dl6n",
            "_namespace_",
            "dynatrace",
            "_pod_uid_",
            "bddb70c4-e44d-4685-b429-4150aaa51eea",
            "_container_ip_",
            "192.168.0.50",
            "_image_name_",
            "public.ecr.aws/dynatrace/dynatrace-operator:v1.2.1",
            "_container_name_",
            "webhook"
        ],
        "Mounts": [
            {
                "Destination": "/dev/shm",
                "Source": "/run/containerd/io.containerd.grpc.v1.cri/sandboxes/81028d27e413191ac921800bc5393dedfce19e7f56313c9aad9a442d9d80e9e0/shm"
            },
            {
                "Destination": "/etc/hostname",
                "Source": "/var/lib/containerd/io.containerd.grpc.v1.cri/sandboxes/81028d27e413191ac921800bc5393dedfce19e7f56313c9aad9a442d9d80e9e0/hostname"
            },
            {
                "Destination": "/var/run/secrets/kubernetes.io/serviceaccount",
                "Source": "/var/lib/kubelet/pods/bddb70c4-e44d-4685-b429-4150aaa51eea/volumes/kubernetes.io~projected/kube-api-access-hp4zj"
            }
        ],
        "Path": "/logtail_host/run/containerd/io.containerd.runtime.v2.task/k8s.io/8859e31bc57dae7cf750cc2353204c6b4d1fa9a40e4eb26fb4c5257be81dcf44/rootfs/app/logs",
        "Tags": [],
        "UpperDir": "/run/containerd/io.containerd.runtime.v2.task/k8s.io/8859e31bc57dae7cf750cc2353204c6b4d1fa9a40e4eb26fb4c5257be81dcf44/rootfs"
    })";

    detailArray.append(item1);
    root["detail"] = detailArray;

    // Test loading from detail format with MetaDatas and Mounts
    containerManager.loadContainerInfoFromDetailFormat(root, "test_path");

    // Verify container is loaded
    EXPECT_EQ(containerManager.mContainerMap.size(), 1);
    auto it = containerManager.mContainerMap.find("8859e31bc57dae7cf750cc2353204c6b4d1fa9a40e4eb26fb4c5257be81dcf44");
    EXPECT_TRUE(it != containerManager.mContainerMap.end());

    // Verify container info
    EXPECT_EQ(it->second->mID, "8859e31bc57dae7cf750cc2353204c6b4d1fa9a40e4eb26fb4c5257be81dcf44");
    EXPECT_EQ(
        it->second->mLogPath,
        "/var/log/pods/dynatrace_dynatrace-webhook-5676d99f4-5dl6n_bddb70c4-e44d-4685-b429-4150aaa51eea/webhook/1.log");
    EXPECT_EQ(it->second->mUpperDir,
              "/run/containerd/io.containerd.runtime.v2.task/k8s.io/"
              "8859e31bc57dae7cf750cc2353204c6b4d1fa9a40e4eb26fb4c5257be81dcf44/rootfs");
    EXPECT_EQ(it->second->mK8sInfo.mNamespace, "dynatrace");
    EXPECT_EQ(it->second->mK8sInfo.mPod, "dynatrace-webhook-5676d99f4-5dl6n");
    EXPECT_EQ(it->second->mK8sInfo.mContainerName, "webhook");
    EXPECT_EQ(it->second->mContainerLabels["_image_name_"], "public.ecr.aws/dynatrace/dynatrace-operator:v1.2.1");
    EXPECT_EQ(it->second->mContainerLabels["_container_ip_"], "192.168.0.50");

    // Verify mounts are loaded
    EXPECT_EQ(it->second->mMounts.size(), 3);
    EXPECT_EQ(it->second->mMounts[0].mDestination, "/dev/shm");
    EXPECT_EQ(it->second->mMounts[0].mSource,
              "/run/containerd/io.containerd.grpc.v1.cri/sandboxes/"
              "81028d27e413191ac921800bc5393dedfce19e7f56313c9aad9a442d9d80e9e0/shm");
    EXPECT_EQ(it->second->mMounts[1].mDestination, "/etc/hostname");
    EXPECT_EQ(it->second->mMounts[2].mDestination, "/var/run/secrets/kubernetes.io/serviceaccount");

    // Verify metadata is stored
    bool foundNamespace = false, foundPodName = false, foundContainerName = false;
    for (const auto& metadata : it->second->mMetadatas) {
        std::string key = GetDefaultTagKeyString(metadata.first);
        if (key == "_namespace_" && metadata.second == "dynatrace")
            foundNamespace = true;
        else if (key == "_pod_name_" && metadata.second == "dynatrace-webhook-5676d99f4-5dl6n")
            foundPodName = true;
        else if (key == "_container_name_" && metadata.second == "webhook")
            foundContainerName = true;
    }

    EXPECT_TRUE(foundNamespace);
    EXPECT_TRUE(foundPodName);
    EXPECT_TRUE(foundContainerName);
}

void ContainerManagerUnittest::TestLoadContainerInfoFromDetailFormatMixed() const {
    ContainerManager containerManager;

    // Test data with both Tags format and MetaDatas format
    Json::Value root;
    root["version"] = "0.1.0";

    Json::Value detailArray(Json::arrayValue);

    // Item with Tags format
    Json::Value item1(Json::objectValue);
    item1["config_name"] = "##1.0##config1";
    item1["container_id"] = "container1";
    item1["params"] = R"({
        "ID": "container1",
        "Path": "/path/to/container1",
        "Tags": [
            "_container_name_",
            "test-container-1",
            "_pod_name_",
            "test-pod-1",
            "_namespace_",
            "default"
        ]
    })";

    // Item with MetaDatas format
    Json::Value item2(Json::objectValue);
    item2["config_name"] = "##1.0##config2";
    item2["container_id"] = "container2";
    item2["params"] = R"({
        "ID": "container2",
        "LogPath": "/var/log/container2.log",
        "UpperDir": "/var/lib/docker/overlay2/container2",
        "MetaDatas": [
            "_container_name_",
            "test-container-2",
            "_pod_name_",
            "test-pod-2",
            "_namespace_",
            "kube-system"
        ],
        "Mounts": [
            {
                "Destination": "/app/logs",
                "Source": "/var/log/apps"
            }
        ]
    })";

    detailArray.append(item1);
    detailArray.append(item2);
    root["detail"] = detailArray;

    // Test loading both formats
    containerManager.loadContainerInfoFromDetailFormat(root, "test_path");

    // Verify both containers are loaded
    EXPECT_EQ(containerManager.mContainerMap.size(), 2);

    // Verify container1 (Tags format)
    auto it1 = containerManager.mContainerMap.find("container1");
    EXPECT_TRUE(it1 != containerManager.mContainerMap.end());
    EXPECT_EQ(it1->second->mID, "container1");
    EXPECT_EQ(it1->second->mK8sInfo.mNamespace, "default");
    EXPECT_EQ(it1->second->mK8sInfo.mPod, "test-pod-1");
    EXPECT_EQ(it1->second->mK8sInfo.mContainerName, "test-container-1");

    // Verify container2 (MetaDatas format)
    auto it2 = containerManager.mContainerMap.find("container2");
    EXPECT_TRUE(it2 != containerManager.mContainerMap.end());
    EXPECT_EQ(it2->second->mID, "container2");
    EXPECT_EQ(it2->second->mLogPath, "/var/log/container2.log");
    EXPECT_EQ(it2->second->mUpperDir, "/var/lib/docker/overlay2/container2");
    EXPECT_EQ(it2->second->mK8sInfo.mNamespace, "kube-system");
    EXPECT_EQ(it2->second->mK8sInfo.mPod, "test-pod-2");
    EXPECT_EQ(it2->second->mK8sInfo.mContainerName, "test-container-2");
    EXPECT_EQ(it2->second->mMounts.size(), 1);
    EXPECT_EQ(it2->second->mMounts[0].mDestination, "/app/logs");
}

void ContainerManagerUnittest::TestLoadContainerInfoFromDetailFormatPathHandling() const {
    ContainerManager containerManager;

    // Test Path field handling with various scenarios
    Json::Value root;
    root["version"] = "0.1.0";

    Json::Value detailArray(Json::arrayValue);

    // Scenario 1: Path with /logtail_host prefix, no UpperDir -> should strip prefix
    Json::Value item1(Json::objectValue);
    item1["config_name"] = "##1.0##config1";
    item1["container_id"] = "container1";
    item1["params"] = R"({
        "ID": "container1",
        "Path": "/logtail_host/var/lib/docker/overlay2/abc123/merged",
        "Tags": []
    })";

    // Scenario 2: Path without /logtail_host prefix, no UpperDir -> should use as-is
    Json::Value item2(Json::objectValue);
    item2["config_name"] = "##1.0##config2";
    item2["container_id"] = "container2";
    item2["params"] = R"({
        "ID": "container2",
        "Path": "/var/lib/docker/overlay2/def456/merged",
        "Tags": []
    })";

    // Scenario 3: Both Path and UpperDir present -> should keep UpperDir, ignore Path
    Json::Value item3(Json::objectValue);
    item3["config_name"] = "##1.0##config3";
    item3["container_id"] = "container3";
    item3["params"] = R"({
        "ID": "container3",
        "Path": "/logtail_host/some/other/path",
        "UpperDir": "/var/lib/docker/overlay2/ghi789/merged",
        "Tags": []
    })";

    detailArray.append(item1);
    detailArray.append(item2);
    detailArray.append(item3);
    root["detail"] = detailArray;

    // Test loading
    containerManager.loadContainerInfoFromDetailFormat(root, "test_path");

    // Verify all containers are loaded
    EXPECT_EQ(containerManager.mContainerMap.size(), 3);

    // Verify Scenario 1: Path with /logtail_host prefix stripped
    auto it1 = containerManager.mContainerMap.find("container1");
    EXPECT_TRUE(it1 != containerManager.mContainerMap.end());
    EXPECT_EQ(it1->second->mUpperDir, "");

    // Verify Scenario 2: Path without prefix used as-is
    auto it2 = containerManager.mContainerMap.find("container2");
    EXPECT_TRUE(it2 != containerManager.mContainerMap.end());
    EXPECT_EQ(it2->second->mUpperDir, "");

    // Verify Scenario 3: UpperDir takes precedence, Path is ignored
    auto it3 = containerManager.mContainerMap.find("container3");
    EXPECT_TRUE(it3 != containerManager.mContainerMap.end());
    EXPECT_EQ(it3->second->mUpperDir, "/var/lib/docker/overlay2/ghi789/merged");
}

void ContainerManagerUnittest::TestLoadContainerInfoFromContainersFormat() const {
    ContainerManager containerManager;

    // Prepare test data in v1.0.0+ format (Containers array)
    Json::Value root;
    root["version"] = "1.0.0";

    Json::Value containersArray(Json::arrayValue);
    Json::Value container1(Json::objectValue);
    container1["ID"] = "test1";
    container1["LogPath"] = "/var/log/containers/test1.log";
    container1["UpperDir"] = "/var/lib/docker/overlay2/test1";

    Json::Value container2(Json::objectValue);
    container2["ID"] = "test2";
    container2["LogPath"] = "/var/log/containers/test2.log";
    container2["UpperDir"] = "/var/lib/docker/overlay2/test2";

    containersArray.append(container1);
    containersArray.append(container2);
    root["Containers"] = containersArray;

    // Test loading from containers format
    containerManager.loadContainerInfoFromContainersFormat(root, "test_path");

    // Verify containers are loaded
    EXPECT_EQ(containerManager.mContainerMap.size(), 2);
    auto it1 = containerManager.mContainerMap.find("test1");
    auto it2 = containerManager.mContainerMap.find("test2");
    EXPECT_TRUE(it1 != containerManager.mContainerMap.end());
    EXPECT_TRUE(it2 != containerManager.mContainerMap.end());

    // Verify container info
    EXPECT_EQ(it1->second->mID, "test1");
    EXPECT_EQ(it1->second->mLogPath, "/var/log/containers/test1.log");
    EXPECT_EQ(it1->second->mUpperDir, "/var/lib/docker/overlay2/test1");
}

void ContainerManagerUnittest::TestLoadContainerInfoVersionHandling() const {
    ContainerManager containerManager;

    // Test v0.1.0 format detection and loading
    {
        Json::Value root;
        root["version"] = "0.1.0";
        root["detail"] = Json::Value(Json::arrayValue);

        containerManager.loadContainerInfoFromDetailFormat(root, "test_path");
        // Should not crash even with empty detail array
        EXPECT_TRUE(true);
    }

    // Test v1.0.0+ format detection and loading
    {
        Json::Value root;
        root["version"] = "1.0.0";
        root["Containers"] = Json::Value(Json::arrayValue);

        containerManager.loadContainerInfoFromContainersFormat(root, "test_path");
        // Should not crash even with empty containers array
        EXPECT_TRUE(true);
    }

    // Test missing version field (defaults to 1.0.0)
    {
        Json::Value root;
        root["Containers"] = Json::Value(Json::arrayValue);

        containerManager.loadContainerInfoFromContainersFormat(root, "test_path");
        // Should not crash
        EXPECT_TRUE(true);
    }
}

void ContainerManagerUnittest::TestSaveContainerInfoWithVersion() const {
    ContainerManager containerManager;

    // Prepare a container with metadata
    RawContainerInfo containerInfo;
    containerInfo.mID = "save_version_test";
    containerInfo.mUpperDir = "/upper/save_version_test";
    containerInfo.mLogPath = "/log/save_version_test";
    containerInfo.AddMetadata("test_key", "test_value");
    containerInfo.AddMetadata("another_key", "another_value");
    containerManager.mContainerMap["save_version_test"] = std::make_shared<RawContainerInfo>(containerInfo);

    // Save to file
    containerManager.SaveContainerInfo();

    // Clear and reload to test serialization/deserialization
    containerManager.mContainerMap.clear();
    containerManager.LoadContainerInfo();

    // Verify the container was loaded with metadata
    auto it = containerManager.mContainerMap.find("save_version_test");
    EXPECT_TRUE(it != containerManager.mContainerMap.end());
    // Check custom metadata (since test_key and another_key are not in containerNameTag)
    bool foundTestKey = false, foundAnotherKey = false;
    for (const auto& customMetadata : it->second->mCustomMetadatas) {
        if (customMetadata.first == "test_key" && customMetadata.second == "test_value")
            foundTestKey = true;
        else if (customMetadata.first == "another_key" && customMetadata.second == "another_value")
            foundAnotherKey = true;
    }
    EXPECT_TRUE(foundTestKey);
    EXPECT_TRUE(foundAnotherKey);
}

std::string ContainerManagerUnittest::findTestDataDirectory() const {
    std::string testDataDir = GetProcessExecutionDir();
    // Remove trailing path separator if present
    if (!testDataDir.empty() && testDataDir.back() == PATH_SEPARATOR[0]) {
        testDataDir.pop_back();
    }
    // Add path to test data directory
    testDataDir += PATH_SEPARATOR + "testDataSet" + PATH_SEPARATOR + "ContainerManagerUnittest";
    return testDataDir;
}

void ContainerManagerUnittest::TestContainerMatchingConsistency() const {
    // This test loads the shared test data and verifies that the C++ implementation
    // produces the same results as the Go implementation for container matching logic.

    // Get test data directory path using smart path detection
    std::string testDataDir = findTestDataDirectory();
    if (testDataDir.empty()) {
        std::cout << "Test data directory not found, skipping consistency test" << std::endl;
        return;
    }

    fsutil::Dir dir(testDataDir);
    if (!dir.Open()) {
        std::cout << "Failed to open test data directory: " << testDataDir << std::endl;
        std::cout << "Test data directory not found, skipping consistency test" << std::endl;
        return;
    }

    // Collect all JSON files in the directory
    std::vector<std::string> testFiles;
    fsutil::Entry entry;
    while ((entry = dir.ReadNext()).operator bool()) {
        std::string fileName = entry.Name();
        if (fileName.size() > 5 && fileName.substr(fileName.size() - 5) == ".json") {
            testFiles.push_back(testDataDir + PATH_SEPARATOR + fileName);
        }
    }
    dir.Close();

    if (testFiles.empty()) {
        std::cout << "No JSON test files found in directory, skipping consistency test" << std::endl;
        return;
    }

    // Sort files for consistent test order
    std::sort(testFiles.begin(), testFiles.end());

    // Run tests for each file
    for (const auto& testFilePath : testFiles) {
        std::cout << "Running tests from file: " << testFilePath << std::endl;
        runTestFile(testFilePath);
    }
}

void ContainerManagerUnittest::runTestFile(const std::string& testFilePath) const {
    // Load test data from JSON file
    std::ifstream file(testFilePath);
    if (!file.is_open()) {
        FAIL() << "Failed to open test file: " << testFilePath;
        return;
    }

    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;
    if (!Json::parseFromStream(reader, file, &root, &errs)) {
        FAIL() << "Failed to parse test data JSON from file " << testFilePath << ": " << errs;
        return;
    }

    // Parse containers
    ContainerManager containerManager;
    const Json::Value& containers = root["containers"];
    for (const auto& containerJson : containers) {
        RawContainerInfo containerInfo;
        containerInfo.mID = containerJson["id"].asString();

        // Parse container labels
        const Json::Value& labels = containerJson["labels"];
        for (const auto& labelKey : labels.getMemberNames()) {
            containerInfo.mContainerLabels[labelKey] = labels[labelKey].asString();
        }

        // Parse environment variables
        const Json::Value& envArray = containerJson["env"];
        for (const auto& envStr : envArray) {
            std::string env = envStr.asString();
            size_t equalPos = env.find('=');
            if (equalPos != std::string::npos) {
                std::string key = env.substr(0, equalPos);
                std::string value = env.substr(equalPos + 1);
                containerInfo.mEnv[key] = value;
            }
        }

        // Parse K8S info
        const Json::Value& k8s = containerJson["k8s"];
        containerInfo.mK8sInfo.mNamespace = k8s["namespace"].asString();
        containerInfo.mK8sInfo.mPod = k8s["pod"].asString();
        containerInfo.mK8sInfo.mContainerName = k8s["container_name"].asString();
        containerInfo.mK8sInfo.mPausedContainer = k8s["paused_container"].asBool();

        const Json::Value& k8sLabels = k8s["labels"];
        for (const auto& labelKey : k8sLabels.getMemberNames()) {
            containerInfo.mK8sInfo.mLabels[labelKey] = k8sLabels[labelKey].asString();
        }

        containerManager.mContainerMap[containerInfo.mID] = std::make_shared<RawContainerInfo>(containerInfo);
    }

    // Run test cases
    const Json::Value& testCases = root["test_cases"];
    for (const auto& testCase : testCases) {
        std::string testName = testCase["name"].asString();
        std::string description = testCase["description"].asString();

        LOG_DEBUG(sLogger, ("testCase start", description));

        // Parse filters using ContainerFilters::Init approach
        ContainerFilters filters;
        const Json::Value& filterJson = testCase["filters"];

        // Use the new method to parse ContainerFilterConfig from test JSON
        ContainerFilterConfig config;
        if (!parseContainerFilterConfigFromTestJson(filterJson, config)) {
            FAIL() << "Failed to parse container filter config for test case: " << testName;
        }

        // Initialize ContainerFilters using the standard Init method
        std::string exception;
        if (!filters.Init(config, exception)) {
            FAIL() << "Failed to initialize ContainerFilters for test case: " << testName;
        }

        // Parse expected results
        std::set<std::string> expectedMatchedIDs;
        const Json::Value& expectedArray = testCase["expected_matched_ids"];
        if (expectedArray.isArray()) {
            for (const auto& expectedId : expectedArray) {
                expectedMatchedIDs.insert(expectedId.asString());
            }
        } else if (expectedArray.isNull()) {
            // Handle null case - no expected matches
        } else {
            FAIL() << "Invalid expected_matched_ids format in test case: " << testName;
        }

        // Run the matching logic
        std::set<std::string> fullList;
        std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> matchList;
        ContainerDiff diff;

        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, true, false, diff);

        // Collect actual matched IDs
        std::set<std::string> actualMatchedIDs;
        for (const auto& container : diff.mAdded) {
            actualMatchedIDs.insert(container->mID);
        }

        // Verify results
        std::string errorMsg = "Test case '" + testName + "' (" + description + ") failed. ";
        errorMsg += "Expected: [";
        for (const auto& id : expectedMatchedIDs) {
            errorMsg += id + ",";
        }
        errorMsg += "] Got: [";
        for (const auto& id : actualMatchedIDs) {
            errorMsg += id + ",";
        }
        errorMsg += "]";

        EXPECT_EQ(actualMatchedIDs, expectedMatchedIDs) << errorMsg;

        LOG_DEBUG(sLogger, ("testCase end", testCase.toStyledString()));
    }
}

bool ContainerManagerUnittest::parseContainerFilterConfigFromTestJson(const Json::Value& filterJson,
                                                                      ContainerFilterConfig& config) const {
    // Clear existing config
    config = ContainerFilterConfig();

    // Parse K8s regex filters
    if (!filterJson["K8sNamespaceRegex"].asString().empty()) {
        config.mK8sNamespaceRegex = filterJson["K8sNamespaceRegex"].asString();
    }
    if (!filterJson["K8sPodRegex"].asString().empty()) {
        config.mK8sPodRegex = filterJson["K8sPodRegex"].asString();
    }
    if (!filterJson["K8sContainerRegex"].asString().empty()) {
        config.mK8sContainerRegex = filterJson["K8sContainerRegex"].asString();
    }

    // Parse K8s label filters
    if (filterJson.isMember("IncludeK8sLabel")) {
        const Json::Value& includeK8sLabel = filterJson["IncludeK8sLabel"];
        for (const auto& key : includeK8sLabel.getMemberNames()) {
            config.mIncludeK8sLabel[key] = includeK8sLabel[key].asString();
        }
    }
    if (filterJson.isMember("ExcludeK8sLabel")) {
        const Json::Value& excludeK8sLabel = filterJson["ExcludeK8sLabel"];
        for (const auto& key : excludeK8sLabel.getMemberNames()) {
            config.mExcludeK8sLabel[key] = excludeK8sLabel[key].asString();
        }
    }

    // Parse environment filters
    if (filterJson.isMember("IncludeEnv")) {
        const Json::Value& includeEnv = filterJson["IncludeEnv"];
        for (const auto& key : includeEnv.getMemberNames()) {
            config.mIncludeEnv[key] = includeEnv[key].asString();
        }
    }
    if (filterJson.isMember("ExcludeEnv")) {
        const Json::Value& excludeEnv = filterJson["ExcludeEnv"];
        for (const auto& key : excludeEnv.getMemberNames()) {
            config.mExcludeEnv[key] = excludeEnv[key].asString();
        }
    }

    // Parse container label filters
    if (filterJson.isMember("IncludeContainerLabel")) {
        const Json::Value& includeContainerLabel = filterJson["IncludeContainerLabel"];
        for (const auto& key : includeContainerLabel.getMemberNames()) {
            config.mIncludeContainerLabel[key] = includeContainerLabel[key].asString();
        }
    }
    if (filterJson.isMember("ExcludeContainerLabel")) {
        const Json::Value& excludeContainerLabel = filterJson["ExcludeContainerLabel"];
        for (const auto& key : excludeContainerLabel.getMemberNames()) {
            config.mExcludeContainerLabel[key] = excludeContainerLabel[key].asString();
        }
    }

    return true;
}

void ContainerManagerUnittest::TestcomputeMatchedContainersDiffWithSnapshot() const {
    // This test verifies that computeMatchedContainersDiff uses a snapshot of mContainerMap
    // to avoid race conditions when the map is modified concurrently
    ContainerManager containerManager;

    // Set up initial containers in mContainerMap
    RawContainerInfo info1;
    info1.mID = "snapshot1";
    info1.mLogPath = "/var/lib/docker/containers/snapshot1/logs";
    info1.mStatus = "running";
    containerManager.mContainerMap["snapshot1"] = std::make_shared<RawContainerInfo>(info1);

    RawContainerInfo info2;
    info2.mID = "snapshot2";
    info2.mLogPath = "/var/lib/docker/containers/snapshot2/logs";
    info2.mStatus = "running";
    containerManager.mContainerMap["snapshot2"] = std::make_shared<RawContainerInfo>(info2);

    RawContainerInfo info3;
    info3.mID = "snapshot3";
    info3.mLogPath = "/var/lib/docker/containers/snapshot3/logs";
    info3.mStatus = "exited";
    containerManager.mContainerMap["snapshot3"] = std::make_shared<RawContainerInfo>(info3);

    // Test 1: Verify that only running containers are added (isStdio = false)
    {
        std::set<std::string> fullList;
        std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> matchList;
        ContainerFilters filters;
        ContainerDiff diff;

        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, false, diff);

        // Should only add running containers to both diff.mAdded and fullList
        EXPECT_EQ(diff.mAdded.size(), 2);
        EXPECT_EQ(fullList.size(), 2); // Only running containers are added to fullList when isStdio=false

        std::set<std::string> addedIds;
        for (const auto& container : diff.mAdded) {
            addedIds.insert(container->mID);
        }
        EXPECT_TRUE(addedIds.find("snapshot1") != addedIds.end());
        EXPECT_TRUE(addedIds.find("snapshot2") != addedIds.end());
        EXPECT_TRUE(addedIds.find("snapshot3") == addedIds.end()); // exited container should not be added

        // Verify fullList only contains running containers
        EXPECT_TRUE(fullList.find("snapshot1") != fullList.end());
        EXPECT_TRUE(fullList.find("snapshot2") != fullList.end());
        EXPECT_TRUE(fullList.find("snapshot3") == fullList.end());
    }

    // Test 2: Verify that isStdio=true includes non-running containers
    {
        std::set<std::string> fullList;
        std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> matchList;
        ContainerFilters filters;
        ContainerDiff diff;

        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, true, false, diff);

        // Should add all containers when isStdio=true (including exited)
        EXPECT_EQ(diff.mAdded.size(), 3);
        EXPECT_EQ(fullList.size(), 3); // All containers are added to fullList when isStdio=true

        std::set<std::string> addedIds;
        for (const auto& container : diff.mAdded) {
            addedIds.insert(container->mID);
        }
        EXPECT_TRUE(addedIds.find("snapshot1") != addedIds.end());
        EXPECT_TRUE(addedIds.find("snapshot2") != addedIds.end());
        EXPECT_TRUE(addedIds.find("snapshot3") != addedIds.end());

        // Verify fullList contains all containers
        EXPECT_TRUE(fullList.find("snapshot1") != fullList.end());
        EXPECT_TRUE(fullList.find("snapshot2") != fullList.end());
        EXPECT_TRUE(fullList.find("snapshot3") != fullList.end());
    }

    // Test 3: Verify snapshot behavior - containers removed from fullList are detected
    {
        std::set<std::string> fullList;
        fullList.insert("snapshot1");
        fullList.insert("removed1"); // This container exists in fullList but not in mContainerMap

        std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> matchList;
        RawContainerInfo removedInfo;
        removedInfo.mID = "removed1";
        removedInfo.mLogPath = "/var/lib/docker/containers/removed1/logs";
        matchList["removed1"] = std::make_shared<RawContainerInfo>(removedInfo);

        ContainerFilters filters;
        ContainerDiff diff;

        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, false, diff);

        // The removed container should be in mRemoved list
        EXPECT_TRUE(std::find(diff.mRemoved.begin(), diff.mRemoved.end(), "removed1") != diff.mRemoved.end());
        // The removed container should no longer be in fullList
        EXPECT_EQ(fullList.count("removed1"), 0);

        // snapshot1 should still be in fullList
        EXPECT_TRUE(fullList.find("snapshot1") != fullList.end());

        // snapshot2 should be added to fullList (running container not previously in fullList)
        EXPECT_TRUE(fullList.find("snapshot2") != fullList.end());

        // snapshot3 should NOT be in fullList (exited container with isStdio=false)
        EXPECT_TRUE(fullList.find("snapshot3") == fullList.end());

        // Final fullList size should be 2 (snapshot1 and snapshot2)
        EXPECT_EQ(fullList.size(), 2);

        // diff.mAdded should contain snapshot2
        EXPECT_EQ(diff.mAdded.size(), 1);
        if (diff.mAdded.size() > 0) {
            EXPECT_EQ(diff.mAdded[0]->mID, "snapshot2");
        }
    }

    // Test 4: Verify snapshot behavior - modified containers are detected
    {
        std::set<std::string> fullList;
        fullList.insert("snapshot1");

        std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> matchList;
        RawContainerInfo oldInfo;
        oldInfo.mID = "snapshot1";
        oldInfo.mLogPath = "/old/path"; // Different from current mContainerMap
        matchList["snapshot1"] = std::make_shared<RawContainerInfo>(oldInfo);

        ContainerFilters filters;
        ContainerDiff diff;

        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, false, diff);

        // The container should be in mModified list
        EXPECT_EQ(diff.mModified.size(), 1);
        if (diff.mModified.size() > 0) {
            EXPECT_EQ(diff.mModified[0]->mID, "snapshot1");
            EXPECT_EQ(diff.mModified[0]->mLogPath, "/var/lib/docker/containers/snapshot1/logs");
        }
    }
}

void ContainerManagerUnittest::TestNullRawContainerInfoHandling() const {
    // This test verifies that null mRawContainerInfo pointers are handled gracefully
    // as added in the race condition fix

    // Note: This test is limited because ContainerInfo is typically managed internally
    // and we cannot easily inject null mRawContainerInfo in the current implementation.
    // However, we can test the defensive checks are in place by verifying the code
    // doesn't crash with various edge cases.

    ContainerManager containerManager;

    // Test 1: Empty container map should not cause issues
    {
        std::set<std::string> fullList;
        std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> matchList;
        ContainerFilters filters;
        ContainerDiff diff;

        // Should not crash with empty map
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, false, diff);
        EXPECT_EQ(diff.mAdded.size(), 0);
        EXPECT_EQ(diff.mRemoved.size(), 0);
        EXPECT_EQ(diff.mModified.size(), 0);
    }

    // Test 2: Container with minimal info
    {
        RawContainerInfo minimalInfo;
        minimalInfo.mID = "minimal1";
        minimalInfo.mStatus = "running";
        containerManager.mContainerMap["minimal1"] = std::make_shared<RawContainerInfo>(minimalInfo);

        std::set<std::string> fullList;
        std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> matchList;
        ContainerFilters filters;
        ContainerDiff diff;

        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, false, diff);
        EXPECT_EQ(diff.mAdded.size(), 1);
        if (diff.mAdded.size() > 0) {
            EXPECT_EQ(diff.mAdded[0]->mID, "minimal1");
        }
    }

    // Test 3: Multiple containers with various states
    {
        containerManager.mContainerMap.clear();

        RawContainerInfo running1;
        running1.mID = "running1";
        running1.mStatus = "running";
        running1.mLogPath = "/var/log/running1";
        containerManager.mContainerMap["running1"] = std::make_shared<RawContainerInfo>(running1);

        RawContainerInfo exited1;
        exited1.mID = "exited1";
        exited1.mStatus = "exited";
        exited1.mLogPath = "/var/log/exited1";
        containerManager.mContainerMap["exited1"] = std::make_shared<RawContainerInfo>(exited1);

        RawContainerInfo created1;
        created1.mID = "created1";
        created1.mStatus = "created";
        created1.mLogPath = "/var/log/created1";
        containerManager.mContainerMap["created1"] = std::make_shared<RawContainerInfo>(created1);

        std::set<std::string> fullList;
        std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> matchList;
        ContainerFilters filters;
        ContainerDiff diff;

        // With isStdio=false, only running containers should be added
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, false, diff);
        EXPECT_EQ(diff.mAdded.size(), 1);
        EXPECT_EQ(fullList.size(), 1); // Only running container in fullList
        if (diff.mAdded.size() > 0) {
            EXPECT_EQ(diff.mAdded[0]->mID, "running1");
        }

        // With isStdio=true, all containers should be added
        fullList.clear();
        matchList.clear();
        ContainerDiff diff2;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, true, false, diff2);
        EXPECT_EQ(diff2.mAdded.size(), 3);
        EXPECT_EQ(fullList.size(), 3); // All containers in fullList when isStdio=true
    }

    // Test 4: Verify thread-safe snapshot usage
    {
        containerManager.mContainerMap.clear();

        // Add some containers
        for (int i = 0; i < 10; i++) {
            RawContainerInfo info;
            info.mID = "container" + std::to_string(i);
            info.mStatus = (i % 2 == 0) ? "running" : "exited";
            info.mLogPath = "/var/log/container" + std::to_string(i);
            containerManager.mContainerMap[info.mID] = std::make_shared<RawContainerInfo>(info);
        }

        std::set<std::string> fullList;
        std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> matchList;
        ContainerFilters filters;
        ContainerDiff diff;

        // This should use a snapshot and not be affected by concurrent modifications
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, false, diff);

        // Only running containers (even indices: 0, 2, 4, 6, 8) should be added
        EXPECT_EQ(diff.mAdded.size(), 5);
        EXPECT_EQ(fullList.size(), 5); // Only running containers are added to fullList when isStdio=false

        // Verify that only running containers are in fullList
        for (int i = 0; i < 10; i++) {
            std::string containerId = "container" + std::to_string(i);
            if (i % 2 == 0) {
                EXPECT_TRUE(fullList.find(containerId) != fullList.end());
            } else {
                EXPECT_TRUE(fullList.find(containerId) == fullList.end());
            }
        }
    }
}

void ContainerManagerUnittest::runConcurrentContainerMapAccessTest(bool enableT2,
                                                                   bool enableT3,
                                                                   const std::string& testName) {
    // This test simulates concurrent access to mContainerMap and mRawContainerInfo to reproduce
    // race conditions related to container lifecycle management.
    //
    // Test scenarios:
    // 1. Thread 1: Continuously calls refreshAllContainersSnapshot() and incrementallyUpdateContainersSnapshot()
    //              (simulates real Polling thread behavior, updates mContainerMap with new RawContainerInfo)
    // 2. Thread 2 (optional): Calls CheckContainerDiffForAllConfig() and ApplyContainerDiffs()
    //              (simulates FileServer::Resume() behavior, can trigger GetCustomExternalTags crash)
    // 3. Thread 3 (optional): Calls GetContainerStoppedEvents()
    //              (tests mRawContainerInfo access with proper locking)
    //
    // This test can expose race conditions including:
    // - Iterator invalidation when mContainerMap is modified during iteration
    // - Use-after-free when accessing RawContainerInfo fields (mEnv, mK8sInfo.mLabels) after container replacement
    // - Concurrent access to unordered_map fields without proper synchronization (in GetCustomExternalTags)
    // - Concurrent access to mConfigContainerDiffMap without proper locking
    // - Null pointer dereference if mRawContainerInfo is not checked
    ContainerManager containerManager;

    // Set running state so CheckContainerDiffForAllConfig can execute
    containerManager.mIsRunning = true;

    // Initialize with some containers
    for (int i = 0; i < 20; i++) {
        RawContainerInfo info;
        info.mID = "container_" + std::to_string(i);
        info.mStatus = "running";
        info.mLogPath = "/var/log/container_" + std::to_string(i);
        containerManager.mContainerMap[info.mID] = std::make_shared<RawContainerInfo>(info);
    }

    // Setup FileServer with a test config to enable CheckContainerDiffForAllConfig and ApplyContainerDiffs
    ctx.SetConfigName(testName);

    // Create FileDiscoveryOptions with container discovery enabled
    Json::Value inputConfigJson;
    inputConfigJson["FilePaths"].append("/tmp/test/*.log");
    inputConfigJson["ContainerFilters"]["IncludeEnv"]["ENV_KEY_1"] = "";
    inputConfigJson["ContainerFilters"]["IncludeK8sLabel"]["app"] = "";
    inputConfigJson["ExternalEnvTag"]["ENV_KEY_1"] = "custom_env_tag";
    inputConfigJson["ExternalK8sLabelTag"]["app"] = "custom_k8s_tag";

    mDiscoveryOpts = FileDiscoveryOptions();
    mDiscoveryOpts.Init(inputConfigJson, ctx, "test");
    mDiscoveryOpts.SetEnableContainerDiscoveryFlag(true);

    // Initialize ContainerDiscoveryOptions separately
    ContainerDiscoveryOptions containerDiscoveryOpts;
    containerDiscoveryOpts.Init(inputConfigJson, ctx, "test");
    mDiscoveryOpts.SetContainerDiscoveryOptions(std::move(containerDiscoveryOpts));

    mDiscoveryOpts.SetContainerInfo(std::make_shared<std::vector<ContainerInfo>>());
    mDiscoveryOpts.SetFullContainerList(std::make_shared<std::set<std::string>>());
    mDiscoveryOpts.SetDeduceAndSetContainerBaseDirFunc(
        [](ContainerInfo& containerInfo, const CollectionPipelineContext* ctx, const FileDiscoveryOptions* opts) {
            containerInfo.mRealBaseDirs.push_back("/tmp/test");
            return true;
        });

    // Add config to FileServer so CheckContainerDiffForAllConfig can find it
    FileServer::GetInstance()->AddFileDiscoveryConfig(testName, &mDiscoveryOpts, &ctx);

    std::atomic<bool> testRunning{true};
    std::atomic<int> iterationCount{0};
    std::atomic<int> errorCount{0};

    // Thread 1: Continuously call refreshAllContainersSnapshot and incrementallyUpdateContainersSnapshot
    // This simulates the real polling thread behavior
    auto modifyThread = [&]() {
        int counter = 100;
        while (testRunning) {
            try {
                // Alternate between full refresh and incremental updates (with some randomness)
                if (counter % 2 == 0 || rand() % 3 == 0) {
                    // Simulate refreshAllContainersSnapshot with subset of existing containers
                    // This triggers RawContainerInfo object replacement for selected containers
                    // Only update 5 containers per refresh to avoid huge JSON string building
                    std::ostringstream metaBuilder;
                    metaBuilder << R"({"All": [)";

                    // Include 5 rotating containers with updated metadata
                    for (int i = 0; i < 5; i++) {
                        int containerId = (counter + i) % 20;
                        if (i > 0)
                            metaBuilder << ",";
                        metaBuilder << R"({"ID": "container_)" << containerId << R"(", "Name": "test_container_)"
                                    << containerId << R"(", "Status": "running")"
                                    << R"(, "LogPath": "/var/log/container_)" << containerId << R"(")"
                                    << R"(, "UpperDir": "/var/lib/docker/overlay2/container_)" << containerId
                                    << R"(/diff")"
                                    << R"(, "Env": {"ENV_KEY_1": "value_)" << counter << R"(", "COUNTER": ")" << counter
                                    << R"("})"
                                    << R"(, "K8sInfo": {"Namespace": "default", "Pod": "pod-)" << containerId
                                    << R"(", "Labels": {"app": "app_)" << (counter % 5) << R"(", "version": "v)"
                                    << (counter % 3) << R"("}}})";
                    }

                    metaBuilder << R"(]})";

                    LogtailPluginMock::GetInstance()->SetUpContainersMeta(metaBuilder.str());
                    containerManager.refreshAllContainersSnapshot();
                } else {
                    // Simulate incrementallyUpdateContainersSnapshot with updates to existing containers
                    // Update 2-3 existing containers to trigger RawContainerInfo replacement
                    std::ostringstream diffBuilder;
                    diffBuilder << R"({"Update": [)";

                    int numUpdates = 2 + (counter % 2);
                    for (int i = 0; i < numUpdates; i++) {
                        int containerId = (counter + i) % 20;
                        if (i > 0)
                            diffBuilder << ",";
                        diffBuilder << R"({"ID": "container_)" << containerId << R"(", "Name": "test_container_)"
                                    << containerId << R"(", "Status": "running")"
                                    << R"(, "LogPath": "/var/log/container_)" << containerId << R"(")"
                                    << R"(, "UpperDir": "/var/lib/docker/overlay2/container_)" << containerId
                                    << R"(/diff")"
                                    << R"(, "Env": {"ENV_KEY_1": "incremental_)" << counter
                                    << R"(", "UPDATED_ENV": "new_)" << counter << R"("})"
                                    << R"(, "K8sInfo": {"Namespace": "default", "Pod": "pod-)" << containerId
                                    << R"(", "Labels": {"app": "incremental_)" << counter
                                    << R"(", "updated": "true"}}})";
                    }

                    diffBuilder << R"(], "Delete": ["old_container_)" << (counter - 50) << R"("], "Stop": ["container_)"
                                << (counter % 20) << R"("]})";

                    LogtailPluginMock::GetInstance()->SetUpDiffContainersMeta(diffBuilder.str());
                    containerManager.incrementallyUpdateContainersSnapshot();
                }

                counter++;

                // Random sleep interval (80-200us) to create more timing variations
                std::this_thread::sleep_for(std::chrono::microseconds(80 + rand() % 120));
            } catch (const std::exception& e) {
                errorCount++;
            }
        }
    };

    // Thread 2: Continuously call CheckContainerDiffForAllConfig and ApplyContainerDiffs
    // This simulates the real FileServer::Resume() behavior
    auto readThread = [&]() {
        // Random initial delay (0-50ms) to vary thread interleaving
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 50));

        while (testRunning) {
            try {
                // Check container diffs for all configs (fills mConfigContainerDiffMap)
                containerManager.CheckContainerDiffForAllConfig();

                // Apply the diffs (reads and clears mConfigContainerDiffMap)
                // This is where GetCustomExternalTags gets called and may crash
                // When thread 1 replaces RawContainerInfo in mContainerMap, the old info's
                // mEnv and mK8sInfo.mLabels may be accessed here causing use-after-free
                containerManager.ApplyContainerDiffs();

                iterationCount++;

                // Random sleep interval (50-150us) to create more varied race conditions
                std::this_thread::sleep_for(std::chrono::microseconds(50 + rand() % 100));
            } catch (const std::exception& e) {
                errorCount++;
            }
        }
    };

    // Thread 3: Call GetContainerStoppedEvents to test concurrent access to mRawContainerInfo
    // This tests the fix for accessing mRawContainerInfo->mID without proper locking


    auto stoppedEventsThread = [&]() {
        // Random initial delay (0-80ms) to vary thread interleaving
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 80));

        int counter = 0;
        while (testRunning) {
            try {
                // Periodically add some container IDs to mStoppedContainerIDs
                if (counter % 5 == 0) {
                    std::lock_guard<std::mutex> lock(containerManager.mStoppedContainerIDsMutex);
                    containerManager.mStoppedContainerIDs.push_back("container_" + std::to_string(counter % 20));
                    containerManager.mStoppedContainerIDs.push_back("dynamic_" + std::to_string(counter + 100));
                }

                // Call GetContainerStoppedEvents which accesses mRawContainerInfo
                // This method was also fixed in commit e0b33e88 to add proper locking
                std::vector<Event*> events;
                containerManager.GetContainerStoppedEvents(events);

                // Clean up events
                for (auto* event : events) {
                    delete event;
                }

                counter++;
                iterationCount++;

                // Random sleep interval (80-180us) to create more varied race conditions
                std::this_thread::sleep_for(std::chrono::microseconds(80 + rand() % 100));
            } catch (const std::exception& e) {
                errorCount++;
            }
        }
    };


    // Start threads based on configuration
    std::thread t1(modifyThread);
    std::unique_ptr<std::thread> t2;
    std::unique_ptr<std::thread> t3;

    if (enableT2) {
        t2 = std::make_unique<std::thread>(readThread);
    }
    if (enableT3) {
        t3 = std::make_unique<std::thread>(stoppedEventsThread);
    }

    // Let them run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // Stop threads
    testRunning = false;

    t1.join();
    if (t2) {
        t2->join();
    }
    if (t3) {
        t3->join();
    }

    // Cleanup FileServer config
    FileServer::GetInstance()->RemoveFileDiscoveryConfig(testName);
    // With the fix (snapshot), there should be no errors
    EXPECT_EQ(errorCount.load(), 0) << "Concurrent access caused " << errorCount.load() << " errors";

    // Verify we actually ran many iterations
    EXPECT_GT(iterationCount.load(), 100) << "Test should run many iterations to stress test concurrency";

    std::string threadConfig = "T1";
    if (enableT2)
        threadConfig += "+T2";
    if (enableT3)
        threadConfig += "+T3";
    std::cout << "Concurrent test (" << threadConfig << ") completed: " << iterationCount.load() << " iterations with "
              << errorCount.load() << " errors" << std::endl;
}

void ContainerManagerUnittest::TestConcurrentContainerMapAccess_T1T2() {
    runConcurrentContainerMapAccessTest(true, false, "test_concurrent_config_t1t2");
}

void ContainerManagerUnittest::TestConcurrentContainerMapAccess_T1T3() {
    runConcurrentContainerMapAccessTest(false, true, "test_concurrent_config_t1t3");
}

void ContainerManagerUnittest::TestConcurrentContainerMapAccess_T1T2T3() {
    runConcurrentContainerMapAccessTest(true, true, "test_concurrent_config_t1t2t3");
}

void ContainerManagerUnittest::TestSequentialContainerDiffAndApply() {
    // This test verifies that after multiple refreshAllContainersSnapshot and incrementallyUpdateContainersSnapshot
    // operations, CheckContainerDiffForAllConfig and ApplyContainerDiffs produce correct results.
    //
    // Test flow:
    // 1. Setup FileServer with test config
    // 2. Execute refreshAllContainersSnapshot multiple times to simulate container updates
    // 3. Execute incrementallyUpdateContainersSnapshot to simulate incremental updates
    // 4. Call CheckContainerDiffForAllConfig and ApplyContainerDiffs multiple times
    // 5. Verify the FileDiscoveryOptions container info is correctly updated after each round

    const std::string testName = "test_sequential_diff_apply";
    ContainerManager containerManager;
    containerManager.mIsRunning = true;

    // Setup FileServer with a test config
    ctx.SetConfigName(testName);

    // Create FileDiscoveryOptions with container discovery enabled
    Json::Value inputConfigJson;
    inputConfigJson["FilePaths"].append("/tmp/test/*.log");
    inputConfigJson["ContainerFilters"]["IncludeEnv"]["ENV_KEY_1"] = "";
    inputConfigJson["ContainerFilters"]["IncludeK8sLabel"]["app"] = "";
    inputConfigJson["ExternalEnvTag"]["ENV_KEY_1"] = "custom_env_tag";
    inputConfigJson["ExternalK8sLabelTag"]["app"] = "custom_k8s_tag";

    mDiscoveryOpts = FileDiscoveryOptions();
    mDiscoveryOpts.Init(inputConfigJson, ctx, "test");
    mDiscoveryOpts.SetEnableContainerDiscoveryFlag(true);

    // Initialize ContainerDiscoveryOptions separately
    ContainerDiscoveryOptions containerDiscoveryOpts;
    containerDiscoveryOpts.Init(inputConfigJson, ctx, "test");
    mDiscoveryOpts.SetContainerDiscoveryOptions(std::move(containerDiscoveryOpts));

    mDiscoveryOpts.SetContainerInfo(std::make_shared<std::vector<ContainerInfo>>());
    mDiscoveryOpts.SetFullContainerList(std::make_shared<std::set<std::string>>());
    mDiscoveryOpts.SetLastContainerUpdateTime(0); // Set to 0 to trigger first check
    mDiscoveryOpts.SetDeduceAndSetContainerBaseDirFunc(
        [](ContainerInfo& containerInfo, const CollectionPipelineContext* ctx, const FileDiscoveryOptions* opts) {
            containerInfo.mRealBaseDirs.push_back("/tmp/test");
            return true;
        });

    // Add config to FileServer
    FileServer::GetInstance()->AddFileDiscoveryConfig(testName, &mDiscoveryOpts, &ctx);

    // ===== Round 1: Initial full refresh with 5 containers =====
    std::ostringstream metaBuilder1;
    metaBuilder1 << R"({"All": [)";
    for (int i = 0; i < 5; i++) {
        if (i > 0)
            metaBuilder1 << ",";
        metaBuilder1 << R"({"ID": "container_)" << i << R"(", "Name": "test_container_)" << i
                     << R"(", "Status": "running")"
                     << R"(, "LogPath": "/var/log/container_)" << i << R"(")"
                     << R"(, "UpperDir": "/var/lib/docker/overlay2/container_)" << i << R"(/diff")"
                     << R"(, "Env": {"ENV_KEY_1": "value_)" << i << R"("})"
                     << R"(, "K8sInfo": {"Namespace": "default", "Pod": "pod-)" << i << R"(", "Labels": {"app": "app_)"
                     << i << R"("}}})";
    }
    metaBuilder1 << R"(]})";

    LogtailPluginMock::GetInstance()->SetUpContainersMeta(metaBuilder1.str());
    containerManager.refreshAllContainersSnapshot();
    containerManager.mLastFullUpdateTime = 100; // Manually set timestamp for predictable testing

    // Verify mContainerMap has 5 containers
    {
        ReadLock lock(containerManager.mContainerMapRWLock);
        EXPECT_EQ(containerManager.mContainerMap.size(), 5);
    }

    // Check diffs and apply (Round 1)
    mDiscoveryOpts.SetLastContainerUpdateTime(50); // Older than mLastFullUpdateTime
    bool hasUpdate = containerManager.CheckContainerDiffForAllConfig();
    EXPECT_TRUE(hasUpdate) << "First CheckContainerDiffForAllConfig should detect new containers";

    auto diff = containerManager.mConfigContainerDiffMap[testName];
    EXPECT_TRUE(diff != nullptr);
    EXPECT_EQ(diff->mAdded.size(), 5) << "Should have 5 added containers in first round";

    containerManager.ApplyContainerDiffs();

    // Verify FileDiscoveryOptions container info was populated
    auto containerInfo = mDiscoveryOpts.GetContainerInfo();
    EXPECT_TRUE(containerInfo != nullptr);
    EXPECT_EQ(containerInfo->size(), 5) << "FileDiscoveryOptions should have 5 containers after first apply";

    // ===== Round 2: Incremental update (add container_5, update container_0) =====
    std::ostringstream diffBuilder1;
    diffBuilder1 << R"({"Update": [)";
    diffBuilder1 << R"({"ID": "container_0", "Name": "test_container_0_v2", "Status": "running")"
                 << R"(, "LogPath": "/var/log/container_0_updated")"
                 << R"(, "UpperDir": "/var/lib/docker/overlay2/container_0/diff")"
                 << R"(, "Env": {"ENV_KEY_1": "updated_0"})"
                 << R"(, "K8sInfo": {"Namespace": "default", "Pod": "pod-0")"
                 << R"(, "Labels": {"app": "app_0"}}},)"
                 << R"({"ID": "container_5", "Name": "test_container_5", "Status": "running")"
                 << R"(, "LogPath": "/var/log/container_5")"
                 << R"(, "UpperDir": "/var/lib/docker/overlay2/container_5/diff")"
                 << R"(, "Env": {"ENV_KEY_1": "value_5"})"
                 << R"(, "K8sInfo": {"Namespace": "default", "Pod": "pod-5")"
                 << R"(, "Labels": {"app": "app_5"}}})"
                 << R"(], "Delete": [], "Stop": []})";

    LogtailPluginMock::GetInstance()->SetUpDiffContainersMeta(diffBuilder1.str());
    containerManager.incrementallyUpdateContainersSnapshot();
    containerManager.mLastIncrementalUpdateTime = 200; // Manually set timestamp

    // Verify incremental update in mContainerMap
    {
        ReadLock lock(containerManager.mContainerMapRWLock);
        EXPECT_EQ(containerManager.mContainerMap.size(), 6); // 5 + 1 = 6
        auto container0 = containerManager.mContainerMap["container_0"];
        EXPECT_EQ(container0->mLogPath, "/var/log/container_0_updated");
        EXPECT_EQ(container0->mName, "test_container_0_v2");
    }

    // Check diffs and apply (Round 2)
    mDiscoveryOpts.SetLastContainerUpdateTime(150); // Between mLastFullUpdateTime and mLastIncrementalUpdateTime
    hasUpdate = containerManager.CheckContainerDiffForAllConfig();
    EXPECT_TRUE(hasUpdate) << "Second CheckContainerDiffForAllConfig should detect updates";

    diff = containerManager.mConfigContainerDiffMap[testName];
    EXPECT_TRUE(diff != nullptr);
    EXPECT_EQ(diff->mAdded.size(), 1) << "Should have 1 added container (container_5)";
    EXPECT_EQ(diff->mModified.size(), 1) << "Should have 1 modified container (container_0)";

    if (diff->mAdded.size() > 0) {
        EXPECT_EQ(diff->mAdded[0]->mID, "container_5");
    }
    if (diff->mModified.size() > 0) {
        EXPECT_EQ(diff->mModified[0]->mID, "container_0");
        EXPECT_EQ(diff->mModified[0]->mLogPath, "/var/log/container_0_updated");
    }

    containerManager.ApplyContainerDiffs();

    // Verify state after second round
    containerInfo = mDiscoveryOpts.GetContainerInfo();
    EXPECT_EQ(containerInfo->size(), 6) << "FileDiscoveryOptions should have 6 containers after second apply";

    std::unordered_map<std::string, ContainerInfo> containerInfoMap;
    for (const auto& info : *containerInfo) {
        containerInfoMap[info.mRawContainerInfo->mID] = info;
    }

    // Verify container_0 was updated
    EXPECT_TRUE(containerInfoMap.find("container_0") != containerInfoMap.end());
    EXPECT_EQ(containerInfoMap["container_0"].mRawContainerInfo->mLogPath, "/var/log/container_0_updated");
    EXPECT_EQ(containerInfoMap["container_0"].mRawContainerInfo->mName, "test_container_0_v2");

    // Verify container_5 was added
    EXPECT_TRUE(containerInfoMap.find("container_5") != containerInfoMap.end());

    // ===== Round 3: Incremental update (delete container_1, container_2) =====
    std::ostringstream diffBuilder2;
    diffBuilder2 << R"({"Update": [], "Delete": ["container_1", "container_2"], "Stop": []})";

    LogtailPluginMock::GetInstance()->SetUpDiffContainersMeta(diffBuilder2.str());
    containerManager.incrementallyUpdateContainersSnapshot();
    containerManager.mLastIncrementalUpdateTime = 300; // Manually set timestamp

    // Verify deletions in mContainerMap
    {
        ReadLock lock(containerManager.mContainerMapRWLock);
        EXPECT_EQ(containerManager.mContainerMap.size(), 4); // 6 - 2 = 4
        EXPECT_TRUE(containerManager.mContainerMap.find("container_1") == containerManager.mContainerMap.end());
        EXPECT_TRUE(containerManager.mContainerMap.find("container_2") == containerManager.mContainerMap.end());
    }

    // Check diffs and apply (Round 3)
    mDiscoveryOpts.SetLastContainerUpdateTime(250); // Between 200 and 300
    hasUpdate = containerManager.CheckContainerDiffForAllConfig();
    EXPECT_TRUE(hasUpdate) << "Third CheckContainerDiffForAllConfig should detect deletions";

    diff = containerManager.mConfigContainerDiffMap[testName];
    EXPECT_TRUE(diff != nullptr);
    EXPECT_EQ(diff->mRemoved.size(), 2) << "Should have 2 removed containers (container_1, container_2)";

    containerManager.ApplyContainerDiffs();

    // Verify final state
    containerInfo = mDiscoveryOpts.GetContainerInfo();
    EXPECT_EQ(containerInfo->size(), 4) << "FileDiscoveryOptions should have 4 containers after third apply";

    containerInfoMap.clear();
    for (const auto& info : *containerInfo) {
        containerInfoMap[info.mRawContainerInfo->mID] = info;
    }

    // Verify container_1 and container_2 were removed
    EXPECT_TRUE(containerInfoMap.find("container_1") == containerInfoMap.end());
    EXPECT_TRUE(containerInfoMap.find("container_2") == containerInfoMap.end());

    // Verify remaining containers
    EXPECT_TRUE(containerInfoMap.find("container_0") != containerInfoMap.end());
    EXPECT_TRUE(containerInfoMap.find("container_3") != containerInfoMap.end());
    EXPECT_TRUE(containerInfoMap.find("container_4") != containerInfoMap.end());
    EXPECT_TRUE(containerInfoMap.find("container_5") != containerInfoMap.end());

    // Save old pointers before full refresh to verify they are replaced with new pointers
    // This is critical for memory safety - after full refresh, old RawContainerInfo objects should be replaced
    std::unordered_map<std::string, RawContainerInfo*> oldRawPointers;
    for (const auto& info : *containerInfo) {
        oldRawPointers[info.mRawContainerInfo->mID] = info.mRawContainerInfo.get();
    }
    EXPECT_EQ(oldRawPointers.size(), 4) << "Should have saved 4 old pointers";

    // ===== Round 4: Full refresh (refreshAllContainersSnapshot) =====
    // Simulate a complete refresh from container runtime with 9 containers
    // This tests that full refresh correctly handles existing + new containers
    std::ostringstream metaBuilder3;
    metaBuilder3 << R"({"All": [)";
    // Include existing containers (0, 3, 4, 5) and new ones (6, 7, 8, 9, 10)
    std::vector<int> containerIds = {0, 3, 4, 5, 6, 7, 8, 9, 10};
    for (size_t i = 0; i < containerIds.size(); i++) {
        int id = containerIds[i];
        if (i > 0)
            metaBuilder3 << ",";
        metaBuilder3 << R"({"ID": "container_)" << id << R"(", "Name": "test_container_)" << id
                     << R"(", "Status": "running")"
                     << R"(, "LogPath": "/var/log/container_)" << id << R"(")"
                     << R"(, "UpperDir": "/var/lib/docker/overlay2/container_)" << id << R"(/diff")"
                     << R"(, "Env": {"ENV_KEY_1": "fullrefresh2_)" << id << R"("})"
                     << R"(, "K8sInfo": {"Namespace": "default", "Pod": "pod-)" << id << R"(", "Labels": {"app": "app_)"
                     << id << R"("}}})";
    }
    metaBuilder3 << R"(]})";

    LogtailPluginMock::GetInstance()->SetUpContainersMeta(metaBuilder3.str());
    containerManager.refreshAllContainersSnapshot();
    containerManager.mLastFullUpdateTime = 400; // Manually set timestamp

    // Verify mContainerMap has 9 containers
    {
        ReadLock lock(containerManager.mContainerMapRWLock);
        EXPECT_EQ(containerManager.mContainerMap.size(), 9);
        // Verify new containers exist
        EXPECT_TRUE(containerManager.mContainerMap.find("container_6") != containerManager.mContainerMap.end());
        EXPECT_TRUE(containerManager.mContainerMap.find("container_7") != containerManager.mContainerMap.end());
        EXPECT_TRUE(containerManager.mContainerMap.find("container_8") != containerManager.mContainerMap.end());
        EXPECT_TRUE(containerManager.mContainerMap.find("container_9") != containerManager.mContainerMap.end());
        EXPECT_TRUE(containerManager.mContainerMap.find("container_10") != containerManager.mContainerMap.end());
    }

    // Check diffs and apply (Round 4)
    mDiscoveryOpts.SetLastContainerUpdateTime(350); // Older than mLastFullUpdateTime
    hasUpdate = containerManager.CheckContainerDiffForAllConfig();
    EXPECT_TRUE(hasUpdate) << "Fourth CheckContainerDiffForAllConfig should detect full refresh changes";

    diff = containerManager.mConfigContainerDiffMap[testName];
    EXPECT_TRUE(diff != nullptr);
    // Full refresh resets baseline, so all 9 containers are treated as "added"
    EXPECT_EQ(diff->mAdded.size(), 9) << "Should have 9 added containers (full refresh resets baseline)";

    containerManager.ApplyContainerDiffs();

    // Verify FileDiscoveryOptions has 9 containers
    containerInfo = mDiscoveryOpts.GetContainerInfo();
    EXPECT_EQ(containerInfo->size(), 9) << "FileDiscoveryOptions should have 9 containers after fourth apply";

    containerInfoMap.clear();
    for (const auto& info : *containerInfo) {
        containerInfoMap[info.mRawContainerInfo->mID] = info;
    }

    // Verify all 9 expected containers are present and validate their details
    EXPECT_EQ(containerInfoMap.size(), 9);
    for (int id : containerIds) {
        std::string containerId = "container_" + std::to_string(id);
        EXPECT_TRUE(containerInfoMap.find(containerId) != containerInfoMap.end())
            << "Container " << containerId << " should be in FileDiscoveryOptions";

        if (containerInfoMap.find(containerId) != containerInfoMap.end()) {
            const auto& containerInfoEntry = containerInfoMap[containerId];

            // Verify container details
            EXPECT_EQ(containerInfoEntry.mRawContainerInfo->mID, containerId);
            EXPECT_EQ(containerInfoEntry.mRawContainerInfo->mStatus, "running");
            EXPECT_EQ(containerInfoEntry.mRawContainerInfo->mLogPath, "/var/log/container_" + std::to_string(id));

            // CRITICAL: Verify that pointers were replaced with new ones after full refresh
            // This ensures that refreshAllContainersSnapshot creates new RawContainerInfo objects
            // rather than reusing old ones, which is essential for memory safety
            auto oldPtrIt = oldRawPointers.find(containerId);
            if (oldPtrIt != oldRawPointers.end()) {
                RawContainerInfo* oldPtr = oldPtrIt->second;
                RawContainerInfo* newPtr = containerInfoEntry.mRawContainerInfo.get();
                EXPECT_NE(oldPtr, newPtr)
                    << "Container " << containerId << " MUST have NEW pointer after full refresh. "
                    << "Old pointer: " << oldPtr << ", New pointer: " << newPtr << ". "
                    << "Same pointer means use-after-free risk!";

                std::cout << "Container " << containerId << " pointer verification: Old=" << oldPtr
                          << ", New=" << newPtr << " (different=OK)" << std::endl;
            }
        }
    }

    // Verify deleted containers (1, 2) are still not present
    EXPECT_TRUE(containerInfoMap.find("container_1") == containerInfoMap.end())
        << "Previously deleted container_1 should not reappear";
    EXPECT_TRUE(containerInfoMap.find("container_2") == containerInfoMap.end())
        << "Previously deleted container_2 should not reappear";

    // ===== Round 5: Verify no changes when config is up-to-date =====
    mDiscoveryOpts.SetLastContainerUpdateTime(450); // Newer than mLastFullUpdateTime
    hasUpdate = containerManager.CheckContainerDiffForAllConfig();
    EXPECT_FALSE(hasUpdate) << "CheckContainerDiffForAllConfig should not detect updates when config is up-to-date";

    // Cleanup
    FileServer::GetInstance()->RemoveFileDiscoveryConfig(testName);

    std::cout << "Sequential container diff and apply test completed successfully" << std::endl;
}

void ContainerManagerUnittest::parseLabelFilters(const Json::Value& filtersJson, MatchCriteriaFilter& filter) const {
    // Clear existing filters to ensure clean state
    filter.mIncludeFields.mFieldsMap.clear();
    filter.mIncludeFields.mFieldsRegMap.clear();
    filter.mExcludeFields.mFieldsMap.clear();
    filter.mExcludeFields.mFieldsRegMap.clear();

    // Include filters
    const Json::Value& includeJson = filtersJson["include"];

    // Static include
    const Json::Value& staticInclude = includeJson["static"];
    for (const auto& key : staticInclude.getMemberNames()) {
        filter.mIncludeFields.mFieldsMap[key] = staticInclude[key].asString();
    }

    // Regex include
    const Json::Value& regexInclude = includeJson["regex"];
    for (const auto& key : regexInclude.getMemberNames()) {
        try {
            filter.mIncludeFields.mFieldsRegMap[key] = std::make_shared<boost::regex>(regexInclude[key].asString());
        } catch (const std::exception& e) {
            std::cout << "Error parsing regex: " << e.what() << std::endl;
            FAIL() << "Error parsing regex: " << e.what();
        }
    }

    // Exclude filters
    const Json::Value& excludeJson = filtersJson["exclude"];

    // Static exclude
    const Json::Value& staticExclude = excludeJson["static"];
    for (const auto& key : staticExclude.getMemberNames()) {
        filter.mExcludeFields.mFieldsMap[key] = staticExclude[key].asString();
    }

    // Regex exclude
    const Json::Value& regexExclude = excludeJson["regex"];
    for (const auto& key : regexExclude.getMemberNames()) {
        try {
            filter.mExcludeFields.mFieldsRegMap[key] = std::make_shared<boost::regex>(regexExclude[key].asString());
        } catch (const std::exception& e) {
            std::cout << "Error parsing regex: " << e.what() << std::endl;
            FAIL() << "Error parsing regex: " << e.what();
        }
    }
}

UNIT_TEST_CASE(ContainerManagerUnittest, TestcomputeMatchedContainersDiff)
UNIT_TEST_CASE(ContainerManagerUnittest, TestrefreshAllContainersSnapshot)
UNIT_TEST_CASE(ContainerManagerUnittest, TestincrementallyUpdateContainersSnapshot)
UNIT_TEST_CASE(ContainerManagerUnittest, TestSaveLoadContainerInfo)
UNIT_TEST_CASE(ContainerManagerUnittest, TestLoadContainerInfoFromDetailFormat)
UNIT_TEST_CASE(ContainerManagerUnittest, TestLoadContainerInfoFromDetailFormatWithTags)
UNIT_TEST_CASE(ContainerManagerUnittest, TestLoadContainerInfoFromDetailFormatWithMounts)
UNIT_TEST_CASE(ContainerManagerUnittest, TestLoadContainerInfoFromDetailFormatMixed)
UNIT_TEST_CASE(ContainerManagerUnittest, TestLoadContainerInfoFromDetailFormatPathHandling)
UNIT_TEST_CASE(ContainerManagerUnittest, TestLoadContainerInfoFromContainersFormat)
UNIT_TEST_CASE(ContainerManagerUnittest, TestLoadContainerInfoVersionHandling)
UNIT_TEST_CASE(ContainerManagerUnittest, TestSaveContainerInfoWithVersion)
UNIT_TEST_CASE(ContainerManagerUnittest, TestContainerMatchingConsistency)
UNIT_TEST_CASE(ContainerManagerUnittest, TestcomputeMatchedContainersDiffWithSnapshot)
UNIT_TEST_CASE(ContainerManagerUnittest, TestNullRawContainerInfoHandling)
UNIT_TEST_CASE(ContainerManagerUnittest, TestConcurrentContainerMapAccess_T1T2)
UNIT_TEST_CASE(ContainerManagerUnittest, TestConcurrentContainerMapAccess_T1T3)
UNIT_TEST_CASE(ContainerManagerUnittest, TestConcurrentContainerMapAccess_T1T2T3)
UNIT_TEST_CASE(ContainerManagerUnittest, TestSequentialContainerDiffAndApply)

} // namespace logtail

UNIT_TEST_MAIN
