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

#include <memory>
#include <string>
#include <algorithm>
#include <vector>
#include <set>
#include <unordered_map>
#include <boost/regex.hpp>
#include "gtest/gtest.h"
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, diff);
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
        containerManager.computeMatchedContainersDiff(fullList2, matchList2, filters, diff);
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
        containerManager.computeMatchedContainersDiff(fullList3, matchList3, filters, diff);
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
        containerManager.mContainerMap["123"] = std::make_shared<RawContainerInfo>(containerInfo1);

        RawContainerInfo containerInfo2;
        containerInfo2.mID = "1234";
        containerInfo2.mLogPath = "/var/lib/docker/containers/1234/logs";
        containerInfo2.mEnv["test"] = "test2";
        containerManager.mContainerMap["1234"] = std::make_shared<RawContainerInfo>(containerInfo2);

        matchList.clear();
        fullList.clear();

        filters.mEnvFilter.mIncludeFields.mFieldsMap["test"] = "test";
        ContainerDiff diff;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, diff);
        EXPECT_EQ(fullList.size(), 2);
        EXPECT_EQ(diff.mAdded.size(), 1);
        EXPECT_EQ(diff.mAdded[0]->mID, "123");
    }   

    {
        // test env exclude filter (exclude key=value)
        ContainerFilters filters;
        matchList.clear();
        fullList.clear();

        // exclude key "test" with value "test" so only 1234 is added
        filters.mEnvFilter.mExcludeFields.mFieldsMap["test"] = "test";
        ContainerDiff diff;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, diff);
        EXPECT_EQ(fullList.size(), 2);
        EXPECT_EQ(diff.mAdded.size(), 1);
        EXPECT_EQ(diff.mAdded[0]->mID, "1234");
    }

    {
        // test env include regex
        ContainerFilters filters;
        matchList.clear();
        fullList.clear();

        filters.mEnvFilter.mIncludeFields.mFieldsRegMap["test"] = std::make_shared<boost::regex>("^test2$");
        ContainerDiff diff;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, diff);
        EXPECT_EQ(fullList.size(), 2);
        EXPECT_EQ(diff.mAdded.size(), 1);
        EXPECT_EQ(diff.mAdded[0]->mID, "1234");
    }

    {
        // test env exclude regex
        ContainerFilters filters;
        matchList.clear();
        fullList.clear();

        filters.mEnvFilter.mExcludeFields.mFieldsRegMap["test"] = std::make_shared<boost::regex>("^test2$");
        ContainerDiff diff;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, diff);
        EXPECT_EQ(fullList.size(), 2);
        EXPECT_EQ(diff.mAdded.size(), 1);
        EXPECT_EQ(diff.mAdded[0]->mID, "123");
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
        containerManager.mContainerMap["123"] = std::make_shared<RawContainerInfo>(containerInfo1);

        RawContainerInfo containerInfo2;
        containerInfo2.mID = "1234";
        containerInfo2.mLogPath = "/var/lib/docker/containers/1234/logs";
        containerInfo2.mK8sInfo.mPod = "pod2";
        containerInfo2.mK8sInfo.mNamespace = "namespace2";
        containerInfo2.mK8sInfo.mContainerName = "container2";
        containerManager.mContainerMap["1234"] = std::make_shared<RawContainerInfo>(containerInfo2);

        matchList.clear();
        fullList.clear();

        filters.mK8SFilter.mPodReg = std::make_shared<boost::regex>("pod1");
        filters.mK8SFilter.mNamespaceReg = std::make_shared<boost::regex>("namespace1");
        filters.mK8SFilter.mContainerReg = std::make_shared<boost::regex>("container1");
        ContainerDiff diff;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, diff);
        EXPECT_EQ(fullList.size(), 2);
        EXPECT_EQ(diff.mAdded.size(), 1);
        EXPECT_EQ(diff.mAdded[0]->mID, "123");
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, diff1);
        EXPECT_EQ(fullList.count("k8s1") + fullList.count("k8s2"), 2);
        EXPECT_EQ(diff1.mAdded.size(), 1);
        EXPECT_EQ(diff1.mAdded[0]->mID, "k8s1");

        // exclude label
        matchList.clear();
        fullList.clear();
        ContainerFilters filters2;
        filters2.mK8SFilter.mK8sLabelFilter.mExcludeFields.mFieldsMap["tier"] = "backend";
        ContainerDiff diff2;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters2, diff2);
        EXPECT_EQ(fullList.count("k8s1") + fullList.count("k8s2"), 2);
        EXPECT_EQ(diff2.mAdded.size(), 1);
        EXPECT_EQ(diff2.mAdded[0]->mID, "k8s1");

        // include regex
        matchList.clear();
        fullList.clear();
        ContainerFilters filters3;
        filters3.mK8SFilter.mK8sLabelFilter.mIncludeFields.mFieldsRegMap["tier"]
            = std::make_shared<boost::regex>("^front.*$");
        ContainerDiff diff3;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters3, diff3);
        EXPECT_EQ(diff3.mAdded.size(), 1);
        EXPECT_EQ(diff3.mAdded[0]->mID, "k8s1");
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, diff1);
        EXPECT_EQ(diff1.mAdded.size(), 1);
        EXPECT_EQ(diff1.mAdded[0]->mID, "cl1");

        // exclude map
        matchList.clear();
        fullList.clear();
        ContainerFilters filters2;
        filters2.mContainerLabelFilter.mExcludeFields.mFieldsMap["app"] = "nginx";
        ContainerDiff diff2;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters2, diff2);
        EXPECT_EQ(diff2.mAdded.size(), 1);
        EXPECT_EQ(diff2.mAdded[0]->mID, "cl2");

        // include regex
        matchList.clear();
        fullList.clear();
        ContainerFilters filters3;
        filters3.mContainerLabelFilter.mIncludeFields.mFieldsRegMap["app"]
            = std::make_shared<boost::regex>("^ng.*");
        ContainerDiff diff3;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters3, diff3);
        EXPECT_EQ(diff3.mAdded.size(), 1);
        EXPECT_EQ(diff3.mAdded[0]->mID, "cl1");

        // exclude regex
        matchList.clear();
        fullList.clear();
        ContainerFilters filters4;
        filters4.mContainerLabelFilter.mExcludeFields.mFieldsRegMap["app"]
            = std::make_shared<boost::regex>("^re.*");
        ContainerDiff diff4;
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters4, diff4);
        EXPECT_EQ(diff4.mAdded.size(), 1);
        EXPECT_EQ(diff4.mAdded[0]->mID, "cl1");
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
        containerManager.computeMatchedContainersDiff(fullList, matchList, filters, diff);
        EXPECT_EQ(diff.mAdded.size(), 1);
        EXPECT_EQ(diff.mAdded[0]->mID, "combo1");
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
	"AllCmd": [{
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
            "DiffCmd": {
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
        })");
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

UNIT_TEST_CASE(ContainerManagerUnittest, TestcomputeMatchedContainersDiff)
UNIT_TEST_CASE(ContainerManagerUnittest, TestrefreshAllContainersSnapshot)
UNIT_TEST_CASE(ContainerManagerUnittest, TestincrementallyUpdateContainersSnapshot)
UNIT_TEST_CASE(ContainerManagerUnittest, TestSaveLoadContainerInfo)

} // namespace logtail

UNIT_TEST_MAIN
