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


#include <algorithm>
#include <boost/regex.hpp>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "gtest/gtest.h"
#include "json/json.h"

#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "common/RuntimeUtil.h"
#include "container_manager/ContainerDiscoveryOptions.h"
#include "container_manager/ContainerManager.h"
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
    void TestLoadContainerInfoFromContainersFormat() const;
    void TestLoadContainerInfoVersionHandling() const;
    void TestSaveContainerInfoWithVersion() const;
    void TestContainerMatchingConsistency() const;
    void runTestFile(const std::string& testFilePath) const;

private:
    void parseLabelFilters(const Json::Value& filtersJson, MatchCriteriaFilter& filter) const;
    bool parseContainerFilterConfigFromTestJson(const Json::Value& filterJson, ContainerFilterConfig& config) const;
    std::string findTestDataDirectory() const;
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, diff);
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
        containerManager.computeMatchedContainersDiff(fullList2, matchList2, filters, false, diff);
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
        containerManager.computeMatchedContainersDiff(fullList3, matchList3, filters, false, diff);
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, diff);
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, diff);
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, diff);
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, diff);
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, false, diff);
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, true, diff1);
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters2, true, diff2);
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters3, true, diff3);
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, true, diff1);
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters2, true, diff2);
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters3, true, diff3);
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters4, true, diff4);
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, true, diff);
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

        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, true, diff);

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
UNIT_TEST_CASE(ContainerManagerUnittest, TestLoadContainerInfoFromContainersFormat)
UNIT_TEST_CASE(ContainerManagerUnittest, TestLoadContainerInfoVersionHandling)
UNIT_TEST_CASE(ContainerManagerUnittest, TestSaveContainerInfoWithVersion)
UNIT_TEST_CASE(ContainerManagerUnittest, TestContainerMatchingConsistency)

} // namespace logtail

UNIT_TEST_MAIN
