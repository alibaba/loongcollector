// Copyright 2024 iLogtail Authors
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

// #include "app_config/AppConfig.h"
#include "plugin/flusher/sls/SLSClientManager.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_STRING(default_access_key_id);
DECLARE_FLAG_STRING(default_access_key);
// DECLARE_FLAG_INT32(sls_hosts_probe_interval_sec);
// DECLARE_FLAG_INT32(sls_all_hosts_probe_interval_sec);
// DECLARE_FLAG_BOOL(send_prefer_real_ip);
// DECLARE_FLAG_INT32(send_switch_real_ip_interval);

using namespace std;

namespace logtail {

class HostInfoUnittest : public ::testing::Test {
public:
    void TestHostname();
    void TestLatency();

private:
    const std::string mHostname = "project.endpoint";
};

void HostInfoUnittest::TestHostname() {
    HostInfo hostInfo(mHostname);
    APSARA_TEST_EQUAL(mHostname, hostInfo.GetHostname());
}

void HostInfoUnittest::TestLatency() {
    HostInfo hostInfo(mHostname);
    APSARA_TEST_TRUE(hostInfo.IsForbidden());

    auto latency = chrono::milliseconds(100);
    hostInfo.SetLatency(latency);
    APSARA_TEST_EQUAL(latency, hostInfo.GetLatency());
    APSARA_TEST_FALSE(hostInfo.IsForbidden());

    hostInfo.SetForbidden();
    APSARA_TEST_EQUAL(chrono::milliseconds::max(), hostInfo.GetLatency());
}

UNIT_TEST_CASE(HostInfoUnittest, TestHostname)
UNIT_TEST_CASE(HostInfoUnittest, TestLatency)

class CandidateHostsInfoUnittest : public ::testing::Test {
public:
    void TestBasicInfo();
    void TestHostsInfo();
    void TestUpdateHosts();
    void TestFirstHost();

private:
    const string mProject = "project";
    const string mRegion = "region";
    const EndpointMode mMode = EndpointMode::DEFAULT;
};

void CandidateHostsInfoUnittest::TestBasicInfo() {
    CandidateHostsInfo info(mProject, mRegion, mMode);
    APSARA_TEST_EQUAL(mProject, info.GetProject());
    APSARA_TEST_EQUAL(mRegion, info.GetRegion());
    APSARA_TEST_EQUAL(mMode, info.GetMode());
}

void CandidateHostsInfoUnittest::TestHostsInfo() {
    const string host1 = mProject + ".endpoint_1";
    const string host21 = mProject + ".endpoint_2_1";
    const string host22 = mProject + ".endpoint_2_2";
    const string host3 = mProject + ".endpoint_3";

    CandidateHostsInfo info(mProject, mRegion, mMode);
    info.mCandidateHosts.push_back({host1});
    info.mCandidateHosts.push_back({host21, host22});
    info.mCandidateHosts.push_back({host3});

    // initialized
    APSARA_TEST_TRUE(info.GetCurrentHost().empty());
    {
        vector<string> res;
        info.GetAllHosts(res);
        APSARA_TEST_EQUAL(4U, res.size());
        APSARA_TEST_EQUAL(host1, res[0]);
        APSARA_TEST_EQUAL(host21, res[1]);
        APSARA_TEST_EQUAL(host22, res[2]);
        APSARA_TEST_EQUAL(host3, res[3]);
    }
    {
        vector<string> res;
        info.GetProbeHosts(res);
        APSARA_TEST_EQUAL(4U, res.size());
        APSARA_TEST_EQUAL(host1, res[0]);
        APSARA_TEST_EQUAL(host21, res[1]);
        APSARA_TEST_EQUAL(host22, res[2]);
        APSARA_TEST_EQUAL(host3, res[3]);
    }

    // no valid host
    info.SelectBestHost();
    APSARA_TEST_TRUE(info.GetCurrentHost().empty());

    // some hosts become valid
    info.UpdateHostInfo(host21, chrono::milliseconds(100));
    info.SelectBestHost();

    APSARA_TEST_EQUAL(host21, info.GetCurrentHost());
    {
        vector<string> res;
        info.GetProbeHosts(res);
        APSARA_TEST_EQUAL(1U, res.size());
        APSARA_TEST_EQUAL(host21, res[0]);
    }
    {
        vector<string> res;
        info.GetAllHosts(res);
        APSARA_TEST_EQUAL(4U, res.size());
        APSARA_TEST_EQUAL(host1, res[0]);
        APSARA_TEST_EQUAL(host21, res[1]);
        APSARA_TEST_EQUAL(host22, res[2]);
        APSARA_TEST_EQUAL(host3, res[3]);
    }

    // host with the same priority as the current one has lower latency
    info.UpdateHostInfo(host22, chrono::milliseconds(50));
    info.SelectBestHost();

    APSARA_TEST_EQUAL(host22, info.GetCurrentHost());
    {
        vector<string> res;
        info.GetProbeHosts(res);
        APSARA_TEST_EQUAL(2U, res.size());
        APSARA_TEST_EQUAL(host21, res[0]);
        APSARA_TEST_EQUAL(host22, res[1]);
    }
    {
        vector<string> res;
        info.GetAllHosts(res);
        APSARA_TEST_EQUAL(4U, res.size());
        APSARA_TEST_EQUAL(host1, res[0]);
        APSARA_TEST_EQUAL(host21, res[1]);
        APSARA_TEST_EQUAL(host22, res[2]);
        APSARA_TEST_EQUAL(host3, res[3]);
    }

    // host with higher priority becomes valid
    info.UpdateHostInfo(host1, chrono::milliseconds(200));
    info.SelectBestHost();

    APSARA_TEST_EQUAL(host1, info.GetCurrentHost());
    {
        vector<string> res;
        info.GetProbeHosts(res);
        APSARA_TEST_EQUAL(3U, res.size());
        APSARA_TEST_EQUAL(host1, res[0]);
        APSARA_TEST_EQUAL(host21, res[1]);
        APSARA_TEST_EQUAL(host22, res[2]);
    }
    {
        vector<string> res;
        info.GetAllHosts(res);
        APSARA_TEST_EQUAL(4U, res.size());
        APSARA_TEST_EQUAL(host1, res[0]);
        APSARA_TEST_EQUAL(host21, res[1]);
        APSARA_TEST_EQUAL(host22, res[2]);
        APSARA_TEST_EQUAL(host3, res[3]);
    }

    // no change
    info.SelectBestHost();

    APSARA_TEST_EQUAL(host1, info.GetCurrentHost());
    {
        vector<string> res;
        info.GetProbeHosts(res);
        APSARA_TEST_EQUAL(3U, res.size());
        APSARA_TEST_EQUAL(host1, res[0]);
        APSARA_TEST_EQUAL(host21, res[1]);
        APSARA_TEST_EQUAL(host22, res[2]);
    }
    {
        vector<string> res;
        info.GetAllHosts(res);
        APSARA_TEST_EQUAL(4U, res.size());
        APSARA_TEST_EQUAL(host1, res[0]);
        APSARA_TEST_EQUAL(host21, res[1]);
        APSARA_TEST_EQUAL(host22, res[2]);
        APSARA_TEST_EQUAL(host3, res[3]);
    }

    // all hosts becomes invalid
    info.UpdateHostInfo(host1, chrono::milliseconds::max());
    info.UpdateHostInfo(host21, chrono::milliseconds::max());
    info.UpdateHostInfo(host22, chrono::milliseconds::max());
    info.UpdateHostInfo(host3, chrono::milliseconds::max());
    info.SelectBestHost();

    APSARA_TEST_TRUE(info.GetCurrentHost().empty());
    {
        vector<string> res;
        info.GetProbeHosts(res);
        APSARA_TEST_EQUAL(4U, res.size());
        APSARA_TEST_EQUAL(host1, res[0]);
        APSARA_TEST_EQUAL(host21, res[1]);
        APSARA_TEST_EQUAL(host22, res[2]);
        APSARA_TEST_EQUAL(host3, res[3]);
    }
    {
        vector<string> res;
        info.GetAllHosts(res);
        APSARA_TEST_EQUAL(4U, res.size());
        APSARA_TEST_EQUAL(host1, res[0]);
        APSARA_TEST_EQUAL(host21, res[1]);
        APSARA_TEST_EQUAL(host22, res[2]);
        APSARA_TEST_EQUAL(host3, res[3]);
    }
}

void CandidateHostsInfoUnittest::TestUpdateHosts() {
    const string region = "region";
    const string publicEndpoint = region + ".log.aliyuncs.com";
    const string privateEndpoint = region + "-intranet.log.aliyuncs.com";
    const string internalEndpoint = region + "-internal.log.aliyuncs.com";
    const string globalEndpoint = "log-global.aliyuncs.com";
    const string customEndpoint = "custom.endpoint";
    const string publicHost = mProject + "." + publicEndpoint;
    const string privateHost = mProject + "." + privateEndpoint;
    const string internalHost = mProject + "." + internalEndpoint;
    const string globalHost = mProject + "." + globalEndpoint;
    const string customHost = mProject + "." + customEndpoint;
    {
        // default mode
        CandidateHostsInfo info("project", region, EndpointMode::DEFAULT);

        // from no remote endpoints
        CandidateEndpoints regionEndpoints{EndpointMode::DEFAULT, {publicEndpoint}, {}};
        info.UpdateHosts(regionEndpoints);
        APSARA_TEST_EQUAL(1U, info.mCandidateHosts.size());
        APSARA_TEST_EQUAL(1U, info.mCandidateHosts[0].size());
        APSARA_TEST_EQUAL(publicHost, info.mCandidateHosts[0][0].GetHostname());
        APSARA_TEST_EQUAL(chrono::milliseconds::max(), info.mCandidateHosts[0][0].GetLatency());

        info.UpdateHostInfo(publicHost, chrono::milliseconds(100));

        // to with remote endpoints
        regionEndpoints.mRemoteEndpoints = {privateEndpoint, publicEndpoint};
        info.UpdateHosts(regionEndpoints);
        APSARA_TEST_EQUAL(2U, info.mCandidateHosts.size());
        APSARA_TEST_EQUAL(1U, info.mCandidateHosts[0].size());
        APSARA_TEST_EQUAL(publicHost, info.mCandidateHosts[0][0].GetHostname());
        APSARA_TEST_EQUAL(chrono::milliseconds(100), info.mCandidateHosts[0][0].GetLatency());
        APSARA_TEST_EQUAL(1U, info.mCandidateHosts[1].size());
        APSARA_TEST_EQUAL(privateHost, info.mCandidateHosts[1][0].GetHostname());
        APSARA_TEST_EQUAL(chrono::milliseconds::max(), info.mCandidateHosts[1][0].GetLatency());

        info.UpdateHostInfo(publicHost, chrono::milliseconds(50));
        info.UpdateHostInfo(privateHost, chrono::milliseconds(150));

        // to updated remote endpoints
        regionEndpoints.mRemoteEndpoints = {internalEndpoint, privateEndpoint, publicEndpoint};
        info.UpdateHosts(regionEndpoints);
        APSARA_TEST_EQUAL(3U, info.mCandidateHosts.size());
        APSARA_TEST_EQUAL(1U, info.mCandidateHosts[0].size());
        APSARA_TEST_EQUAL(publicHost, info.mCandidateHosts[0][0].GetHostname());
        APSARA_TEST_EQUAL(chrono::milliseconds(50), info.mCandidateHosts[0][0].GetLatency());
        APSARA_TEST_EQUAL(1U, info.mCandidateHosts[1].size());
        APSARA_TEST_EQUAL(internalHost, info.mCandidateHosts[1][0].GetHostname());
        APSARA_TEST_EQUAL(chrono::milliseconds::max(), info.mCandidateHosts[1][0].GetLatency());
        APSARA_TEST_EQUAL(1U, info.mCandidateHosts[2].size());
        APSARA_TEST_EQUAL(privateHost, info.mCandidateHosts[2][0].GetHostname());
        APSARA_TEST_EQUAL(chrono::milliseconds(150), info.mCandidateHosts[2][0].GetLatency());
    }
    {
        // accelerate mode
        {
            CandidateHostsInfo info("project", region, EndpointMode::ACCELERATE);
            // from no remote endpoints
            CandidateEndpoints regionEndpoints{EndpointMode::DEFAULT, {publicEndpoint}, {}};
            info.UpdateHosts(regionEndpoints);
            APSARA_TEST_EQUAL(1U, info.mCandidateHosts.size());
            APSARA_TEST_EQUAL(1U, info.mCandidateHosts[0].size());
            APSARA_TEST_EQUAL(globalHost, info.mCandidateHosts[0][0].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds::max(), info.mCandidateHosts[0][0].GetLatency());

            info.UpdateHostInfo(globalHost, chrono::milliseconds(100));

            // to with remote endpoints
            regionEndpoints.mRemoteEndpoints = {privateEndpoint, publicEndpoint};
            info.UpdateHosts(regionEndpoints);
            APSARA_TEST_EQUAL(1U, info.mCandidateHosts.size());
            APSARA_TEST_EQUAL(2U, info.mCandidateHosts[0].size());
            APSARA_TEST_EQUAL(globalHost, info.mCandidateHosts[0][0].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds(100), info.mCandidateHosts[0][0].GetLatency());
            APSARA_TEST_EQUAL(publicHost, info.mCandidateHosts[0][1].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds::max(), info.mCandidateHosts[0][1].GetLatency());

            info.UpdateHostInfo(globalHost, chrono::milliseconds(50));
            info.UpdateHostInfo(publicHost, chrono::milliseconds(150));

            // to updated remote endpoints
            regionEndpoints.mRemoteEndpoints = {internalEndpoint, privateEndpoint, publicEndpoint};
            info.UpdateHosts(regionEndpoints);
            APSARA_TEST_EQUAL(1U, info.mCandidateHosts.size());
            APSARA_TEST_EQUAL(2U, info.mCandidateHosts[0].size());
            APSARA_TEST_EQUAL(globalHost, info.mCandidateHosts[0][0].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds(50), info.mCandidateHosts[0][0].GetLatency());
            APSARA_TEST_EQUAL(publicHost, info.mCandidateHosts[0][1].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds(150), info.mCandidateHosts[0][1].GetLatency());
        }
        {
            CandidateHostsInfo info("project", region, EndpointMode::ACCELERATE);
            // from no remote endpoints
            CandidateEndpoints regionEndpoints{EndpointMode::ACCELERATE, {globalEndpoint}, {}};
            info.UpdateHosts(regionEndpoints);
            APSARA_TEST_EQUAL(1U, info.mCandidateHosts.size());
            APSARA_TEST_EQUAL(1U, info.mCandidateHosts[0].size());
            APSARA_TEST_EQUAL(globalHost, info.mCandidateHosts[0][0].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds::max(), info.mCandidateHosts[0][0].GetLatency());

            info.UpdateHostInfo(globalHost, chrono::milliseconds(100));

            // to with remote endpoints
            regionEndpoints.mRemoteEndpoints = {privateEndpoint, publicEndpoint};
            info.UpdateHosts(regionEndpoints);
            APSARA_TEST_EQUAL(1U, info.mCandidateHosts.size());
            APSARA_TEST_EQUAL(2U, info.mCandidateHosts[0].size());
            APSARA_TEST_EQUAL(globalHost, info.mCandidateHosts[0][0].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds(100), info.mCandidateHosts[0][0].GetLatency());
            APSARA_TEST_EQUAL(publicHost, info.mCandidateHosts[0][1].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds::max(), info.mCandidateHosts[0][1].GetLatency());

            info.UpdateHostInfo(globalHost, chrono::milliseconds(50));
            info.UpdateHostInfo(publicHost, chrono::milliseconds(150));

            // to updated remote endpoints
            regionEndpoints.mRemoteEndpoints = {internalEndpoint, privateEndpoint, publicEndpoint};
            info.UpdateHosts(regionEndpoints);
            APSARA_TEST_EQUAL(1U, info.mCandidateHosts.size());
            APSARA_TEST_EQUAL(2U, info.mCandidateHosts[0].size());
            APSARA_TEST_EQUAL(globalHost, info.mCandidateHosts[0][0].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds(50), info.mCandidateHosts[0][0].GetLatency());
            APSARA_TEST_EQUAL(publicHost, info.mCandidateHosts[0][1].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds(150), info.mCandidateHosts[0][1].GetLatency());
        }
        {
            CandidateHostsInfo info("project", region, EndpointMode::ACCELERATE);
            // from no remote endpoints
            CandidateEndpoints regionEndpoints{EndpointMode::CUSTOM, {customEndpoint}, {}};
            info.UpdateHosts(regionEndpoints);
            APSARA_TEST_EQUAL(1U, info.mCandidateHosts.size());
            APSARA_TEST_EQUAL(1U, info.mCandidateHosts[0].size());
            APSARA_TEST_EQUAL(globalHost, info.mCandidateHosts[0][0].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds::max(), info.mCandidateHosts[0][0].GetLatency());

            info.UpdateHostInfo(globalHost, chrono::milliseconds(100));

            // to with remote endpoints
            regionEndpoints.mRemoteEndpoints = {privateEndpoint, publicEndpoint};
            info.UpdateHosts(regionEndpoints);
            APSARA_TEST_EQUAL(1U, info.mCandidateHosts.size());
            APSARA_TEST_EQUAL(2U, info.mCandidateHosts[0].size());
            APSARA_TEST_EQUAL(globalHost, info.mCandidateHosts[0][0].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds(100), info.mCandidateHosts[0][0].GetLatency());
            APSARA_TEST_EQUAL(publicHost, info.mCandidateHosts[0][1].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds::max(), info.mCandidateHosts[0][1].GetLatency());

            info.UpdateHostInfo(globalHost, chrono::milliseconds(50));
            info.UpdateHostInfo(publicHost, chrono::milliseconds(150));

            // to updated remote endpoints
            regionEndpoints.mRemoteEndpoints = {internalEndpoint, privateEndpoint, publicEndpoint};
            info.UpdateHosts(regionEndpoints);
            APSARA_TEST_EQUAL(1U, info.mCandidateHosts.size());
            APSARA_TEST_EQUAL(2U, info.mCandidateHosts[0].size());
            APSARA_TEST_EQUAL(globalHost, info.mCandidateHosts[0][0].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds(50), info.mCandidateHosts[0][0].GetLatency());
            APSARA_TEST_EQUAL(publicHost, info.mCandidateHosts[0][1].GetHostname());
            APSARA_TEST_EQUAL(chrono::milliseconds(150), info.mCandidateHosts[0][1].GetLatency());
        }
    }
    {
        // custom mode
        CandidateHostsInfo info("project", region, EndpointMode::CUSTOM);

        // from no remote endpoints
        CandidateEndpoints regionEndpoints{EndpointMode::CUSTOM, {customEndpoint}, {}};
        info.UpdateHosts(regionEndpoints);
        APSARA_TEST_EQUAL(1U, info.mCandidateHosts.size());
        APSARA_TEST_EQUAL(1U, info.mCandidateHosts[0].size());
        APSARA_TEST_EQUAL(customHost, info.mCandidateHosts[0][0].GetHostname());
        APSARA_TEST_EQUAL(chrono::milliseconds::max(), info.mCandidateHosts[0][0].GetLatency());

        info.UpdateHostInfo(customHost, chrono::milliseconds(100));

        // to with remote endpoints
        regionEndpoints.mRemoteEndpoints = {privateEndpoint, publicEndpoint};
        info.UpdateHosts(regionEndpoints);
        APSARA_TEST_EQUAL(1U, info.mCandidateHosts.size());
        APSARA_TEST_EQUAL(1U, info.mCandidateHosts[0].size());
        APSARA_TEST_EQUAL(customHost, info.mCandidateHosts[0][0].GetHostname());
        APSARA_TEST_EQUAL(chrono::milliseconds(100), info.mCandidateHosts[0][0].GetLatency());
    }
}

void CandidateHostsInfoUnittest::TestFirstHost() {
    const string host1 = mProject + ".endpoint_1";
    const string host2 = mProject + ".endpoint_2";

    CandidateHostsInfo info(mProject, mRegion, mMode);
    APSARA_TEST_EQUAL("", info.GetFirstHost());

    info.mCandidateHosts.push_back({host1, host2});
    APSARA_TEST_EQUAL(host1, info.GetFirstHost());

    info.mCandidateHosts[0].clear();
    APSARA_TEST_EQUAL("", info.GetFirstHost());
}

UNIT_TEST_CASE(CandidateHostsInfoUnittest, TestBasicInfo)
UNIT_TEST_CASE(CandidateHostsInfoUnittest, TestHostsInfo)
UNIT_TEST_CASE(CandidateHostsInfoUnittest, TestUpdateHosts)
UNIT_TEST_CASE(CandidateHostsInfoUnittest, TestFirstHost)

// class ProbeNetworkMock {
// public:
//     static HttpResponse DoProbeNetwork(const std::unique_ptr<SLSClientManager::ProbeNetworkHttpRequest>& req) {
//         HttpResponse response;
//         chrono::milliseconds latency = chrono::milliseconds::max();
//         if (!enableUnavailableHosts) {
//             if (req->mProject == project1 && req->mHost == project1 + "." + publicEndpoint1) {
//                 static uint32_t sec = 1;
//                 latency = chrono::milliseconds(sec++);
//             } else if (req->mProject == project2) {
//                 if (req->mHost == project2 + "." + publicEndpoint2) {
//                     static uint32_t sec = 10;
//                     latency = chrono::milliseconds(sec++);
//                 } else if (req->mHost == project2 + "." + globalEndpoint) {
//                     static uint32_t sec = 5;
//                     latency = chrono::milliseconds(sec++);
//                 }
//             } else if (req->mProject == project3 && req->mHost == project3 + "." + customEndpoint) {
//                 static uint32_t sec = 20;
//                 latency = chrono::milliseconds(sec++);
//             }
//         } else {
//             static uint32_t sec = 200;
//             latency = chrono::milliseconds(sec++);
//         }

//         response.SetResponseTime(latency);
//         return response;
//     }

//     static void Prepare() {
//         publicEndpoint1 = region1 + ".log.aliyuncs.com";
//         privateEndpoint1 = region1 + "-intranet.log.aliyuncs.com";
//         internalEndpoint1 = region1 + "-internal.log.aliyuncs.com";
//         publicEndpoint2 = region2 + ".log.aliyuncs.com";
//         privateEndpoint2 = region2 + "-intranet.log.aliyuncs.com";
//         internalEndpoint2 = region2 + "-internal.log.aliyuncs.com";

//         // region_1: only public endpoint is available
//         // region_2: both accelerate and public endpoints are available
//         // region_3: only custom endpoint is available
//         // project_x belongs to region_x
//         SLSClientManager::GetInstance()->UpdateLocalRegionEndpointsAndHttpsInfo(region1, {publicEndpoint1});
//         SLSClientManager::GetInstance()->UpdateLocalRegionEndpointsAndHttpsInfo(region2, {globalEndpoint});
//         SLSClientManager::GetInstance()->UpdateLocalRegionEndpointsAndHttpsInfo(region3, {customEndpoint});
//         SLSClientManager::GetInstance()->UpdateRemoteRegionEndpoints(
//             region1, {internalEndpoint1, privateEndpoint1, publicEndpoint1});
//         SLSClientManager::GetInstance()->UpdateRemoteRegionEndpoints(
//             region2, {internalEndpoint2, privateEndpoint2, publicEndpoint2});
//     }

//     static bool enableUnavailableHosts;

//     static string project1;
//     static string project2;
//     static string project3;
//     static string region1;
//     static string region2;
//     static string region3;
//     static string publicEndpoint1;
//     static string privateEndpoint1;
//     static string internalEndpoint1;
//     static string publicEndpoint2;
//     static string privateEndpoint2;
//     static string internalEndpoint2;
//     static string globalEndpoint;
//     static string customEndpoint;
// };

// bool ProbeNetworkMock::enableUnavailableHosts = false;
// string ProbeNetworkMock::region1 = "region_1";
// string ProbeNetworkMock::region2 = "region_2";
// string ProbeNetworkMock::region3 = "region_3";
// string ProbeNetworkMock::project1 = "project_1";
// string ProbeNetworkMock::project2 = "project_2";
// string ProbeNetworkMock::project3 = "project_3";
// string ProbeNetworkMock::publicEndpoint1;
// string ProbeNetworkMock::privateEndpoint1;
// string ProbeNetworkMock::internalEndpoint1;
// string ProbeNetworkMock::publicEndpoint2;
// string ProbeNetworkMock::privateEndpoint2;
// string ProbeNetworkMock::internalEndpoint2;
// string ProbeNetworkMock::globalEndpoint = "log-global.aliyuncs.com";
// string ProbeNetworkMock::customEndpoint = "custom.endpoint";

// class GetRealIpMock {
// public:
//     static bool GetEndpointRealIpMock(const string& endpoint, string& ip) {
//         if (endpoint.find(normalRegion) != string::npos) {
//             static size_t i = 0;
//             ip = normalRegionIps[i];
//             i = (i + 1) % normalRegionIps.size();
//             return true;
//         } else if (endpoint.find(unavailableRegion) != string::npos) {
//             static size_t i = 0;
//             ip = unavailableRegionIps[i];
//             i = (i + 1) % unavailableRegionIps.size();
//             return true;
//         } else {
//             return false;
//         }
//     }

//     static HttpResponse DoProbeNetwork(const std::unique_ptr<SLSClientManager::ProbeNetworkHttpRequest>& req) {
//         HttpResponse response;
//         response.SetResponseTime(chrono::milliseconds(100));
//         return response;
//     }

//     static string normalRegion;
//     static string unavailableRegion;
//     static vector<string> normalRegionIps;
//     static vector<string> unavailableRegionIps;
// };

// string GetRealIpMock::normalRegion = "region_1";
// string GetRealIpMock::unavailableRegion = "region_2";
// vector<string> GetRealIpMock::normalRegionIps = {"192.168.0.1", "192.168.0.2"};
// vector<string> GetRealIpMock::unavailableRegionIps = {"10.0.0.1", "10.0.0.2"};

class SLSClientManagerUnittest : public ::testing::Test {
public:
    // void TestLocalRegionEndpoints();
    // void TestRemoteRegionEndpoints();
    // void TestGetCandidateHostsInfo();
    // void TestUpdateHostInfo();
    // void TestUsingHttps();
    // void TestProbeNetwork();
    // void TestRealIp();
    void TestAccessKeyManagement();
    void TestGetCandidateHostsInfo();

protected:
    // static void SetUpTestCase() {
    //     SLSClientManager::GetInstance()->mDoProbeNetwork = ProbeNetworkMock::DoProbeNetwork;
    //     SLSClientManager::GetInstance()->mGetEndpointRealIp = GetRealIpMock::GetEndpointRealIpMock;
    // }

    // void TearDown() override { mManager.Clear(); }

private:
    SLSClientManager mManager;
};

// void SLSClientManagerUnittest::TestLocalRegionEndpoints() {
//     const string region = "region";
//     {
//         // default
//         const string endpoint = region + ".log.aliyuncs.com";
//         mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {endpoint});

//         auto& item = mManager.mRegionCandidateEndpointsMap[region];
//         APSARA_TEST_EQUAL(EndpointMode::DEFAULT, item.mMode);
//         APSARA_TEST_EQUAL(1U, item.mLocalEndpoints.size());
//         APSARA_TEST_EQUAL(endpoint, item.mLocalEndpoints[0]);
//         APSARA_TEST_FALSE(mManager.UsingHttps(region));
//     }
//     {
//         // accelerate (+ default)
//         const string endpoint1 = region + "-intranet.log.aliyuncs.com";
//         const string endpoint2 = kAccelerationDataEndpoint;
//         mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {endpoint1, endpoint2});

//         auto& item = mManager.mRegionCandidateEndpointsMap[region];
//         APSARA_TEST_EQUAL(EndpointMode::ACCELERATE, item.mMode);
//         APSARA_TEST_EQUAL(0U, item.mLocalEndpoints.size());
//         APSARA_TEST_FALSE(mManager.UsingHttps(region));
//     }
//     {
//         // custom (+ default)
//         const string endpoint1 = region + "-intranet.log.aliyuncs.com";
//         const string endpoint2 = "custom.endpoint";
//         mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {endpoint1, endpoint2});

//         auto& item = mManager.mRegionCandidateEndpointsMap[region];
//         APSARA_TEST_EQUAL(EndpointMode::CUSTOM, item.mMode);
//         APSARA_TEST_EQUAL(1U, item.mLocalEndpoints.size());
//         APSARA_TEST_EQUAL(endpoint2, item.mLocalEndpoints[0]);
//         APSARA_TEST_FALSE(mManager.UsingHttps(region));
//     }
//     {
//         // accelerate + custom (+ default)
//         const string endpoint1 = "custom.endpoint";
//         const string endpoint2 = region + "-intranet.log.aliyuncs.com";
//         const string endpoint3 = kAccelerationDataEndpoint;
//         mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {endpoint1, endpoint2, endpoint3});

//         auto& item = mManager.mRegionCandidateEndpointsMap[region];
//         APSARA_TEST_EQUAL(EndpointMode::ACCELERATE, item.mMode);
//         APSARA_TEST_EQUAL(0U, item.mLocalEndpoints.size());
//         APSARA_TEST_FALSE(mManager.UsingHttps(region));
//     }
//     {
//         // http -> https -> https
//         const string endpoint1 = region + "-intranet.log.aliyuncs.com";
//         const string endpoint2 = "https://" + kAccelerationDataEndpoint;
//         mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {endpoint1, endpoint2});
//         APSARA_TEST_TRUE(mManager.UsingHttps(region));

//         mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {endpoint1, endpoint2});
//         APSARA_TEST_TRUE(mManager.UsingHttps(region));
//     }
//     {
//         // https -> http -> http
//         const string endpoint1 = region + "-intranet.log.aliyuncs.com";
//         const string endpoint2 = kAccelerationDataEndpoint;
//         mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {endpoint1, endpoint2});
//         APSARA_TEST_FALSE(mManager.UsingHttps(region));

//         mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {endpoint1, endpoint2});
//         APSARA_TEST_FALSE(mManager.UsingHttps(region));
//     }
// }

// void SLSClientManagerUnittest::TestRemoteRegionEndpoints() {
//     {
//         auto info1 = mManager.GetCandidateHostsInfo("region_1", "project_1", EndpointMode::DEFAULT);
//         auto info2 = mManager.GetCandidateHostsInfo("region_1", "project_2", EndpointMode::DEFAULT);
//         auto info3 = mManager.GetCandidateHostsInfo("region_2", "project_3", EndpointMode::DEFAULT);
//         APSARA_TEST_EQUAL(0U, info3->mCandidateHosts.size());

//         // create remote info
//         mManager.UpdateRemoteRegionEndpoints("region_1", {"endpoint_1", "endpoint_2"});
//         APSARA_TEST_EQUAL(2U, info1->mCandidateHosts.size());
//         APSARA_TEST_EQUAL(2U, info2->mCandidateHosts.size());
//         APSARA_TEST_EQUAL(0U, info3->mCandidateHosts.size());
//         APSARA_TEST_EQUAL(1U, info1->mCandidateHosts[0].size());
//         APSARA_TEST_EQUAL(1U, info1->mCandidateHosts[1].size());
//         APSARA_TEST_EQUAL(1U, info2->mCandidateHosts[0].size());
//         APSARA_TEST_EQUAL(1U, info2->mCandidateHosts[1].size());
//         APSARA_TEST_EQUAL("project_1.endpoint_1", info1->mCandidateHosts[0][0].GetHostname());
//         APSARA_TEST_EQUAL("project_1.endpoint_2", info1->mCandidateHosts[1][0].GetHostname());
//         APSARA_TEST_EQUAL("project_2.endpoint_1", info2->mCandidateHosts[0][0].GetHostname());
//         APSARA_TEST_EQUAL("project_2.endpoint_2", info2->mCandidateHosts[1][0].GetHostname());

//         // update remote info with overwrite
//         mManager.UpdateRemoteRegionEndpoints("region_1", {"endpoint_2", "endpoint_3"});
//         APSARA_TEST_EQUAL(2U, info1->mCandidateHosts.size());
//         APSARA_TEST_EQUAL(2U, info2->mCandidateHosts.size());
//         APSARA_TEST_EQUAL(0U, info3->mCandidateHosts.size());
//         APSARA_TEST_EQUAL(1U, info1->mCandidateHosts[0].size());
//         APSARA_TEST_EQUAL(1U, info1->mCandidateHosts[1].size());
//         APSARA_TEST_EQUAL(1U, info2->mCandidateHosts[0].size());
//         APSARA_TEST_EQUAL(1U, info2->mCandidateHosts[1].size());
//         APSARA_TEST_EQUAL("project_1.endpoint_2", info1->mCandidateHosts[0][0].GetHostname());
//         APSARA_TEST_EQUAL("project_1.endpoint_3", info1->mCandidateHosts[1][0].GetHostname());
//         APSARA_TEST_EQUAL("project_2.endpoint_2", info2->mCandidateHosts[0][0].GetHostname());
//         APSARA_TEST_EQUAL("project_2.endpoint_3", info2->mCandidateHosts[1][0].GetHostname());

//         // update remote info with create
//         mManager.UpdateRemoteRegionEndpoints(
//             "region_1", {"endpoint_4"}, SLSClientManager::RemoteEndpointUpdateAction::CREATE);
//         APSARA_TEST_EQUAL(2U, info1->mCandidateHosts.size());
//         APSARA_TEST_EQUAL(2U, info2->mCandidateHosts.size());
//         APSARA_TEST_EQUAL(0U, info3->mCandidateHosts.size());
//         APSARA_TEST_EQUAL(1U, info1->mCandidateHosts[0].size());
//         APSARA_TEST_EQUAL(1U, info1->mCandidateHosts[1].size());
//         APSARA_TEST_EQUAL(1U, info2->mCandidateHosts[0].size());
//         APSARA_TEST_EQUAL(1U, info2->mCandidateHosts[1].size());
//         APSARA_TEST_EQUAL("project_1.endpoint_2", info1->mCandidateHosts[0][0].GetHostname());
//         APSARA_TEST_EQUAL("project_1.endpoint_3", info1->mCandidateHosts[1][0].GetHostname());
//         APSARA_TEST_EQUAL("project_2.endpoint_2", info2->mCandidateHosts[0][0].GetHostname());
//         APSARA_TEST_EQUAL("project_2.endpoint_3", info2->mCandidateHosts[1][0].GetHostname());

//         // update remote info with append
//         mManager.UpdateRemoteRegionEndpoints(
//             "region_1", {"endpoint_3", "endpoint_1"}, SLSClientManager::RemoteEndpointUpdateAction::APPEND);
//         APSARA_TEST_EQUAL(3U, info1->mCandidateHosts.size());
//         APSARA_TEST_EQUAL(3U, info2->mCandidateHosts.size());
//         APSARA_TEST_EQUAL(0U, info3->mCandidateHosts.size());
//         APSARA_TEST_EQUAL(1U, info1->mCandidateHosts[0].size());
//         APSARA_TEST_EQUAL(1U, info1->mCandidateHosts[1].size());
//         APSARA_TEST_EQUAL(1U, info1->mCandidateHosts[2].size());
//         APSARA_TEST_EQUAL(1U, info2->mCandidateHosts[0].size());
//         APSARA_TEST_EQUAL(1U, info2->mCandidateHosts[1].size());
//         APSARA_TEST_EQUAL(1U, info2->mCandidateHosts[2].size());
//         APSARA_TEST_EQUAL("project_1.endpoint_2", info1->mCandidateHosts[0][0].GetHostname());
//         APSARA_TEST_EQUAL("project_1.endpoint_3", info1->mCandidateHosts[1][0].GetHostname());
//         APSARA_TEST_EQUAL("project_1.endpoint_1", info1->mCandidateHosts[2][0].GetHostname());
//         APSARA_TEST_EQUAL("project_2.endpoint_2", info2->mCandidateHosts[0][0].GetHostname());
//         APSARA_TEST_EQUAL("project_2.endpoint_3", info2->mCandidateHosts[1][0].GetHostname());
//         APSARA_TEST_EQUAL("project_2.endpoint_1", info2->mCandidateHosts[2][0].GetHostname());
//     }
//     {
//         // get candidate host info after remote info is updated
//         auto info = mManager.GetCandidateHostsInfo("region_1", "project_1", EndpointMode::DEFAULT);
//         APSARA_TEST_EQUAL(3U, info->mCandidateHosts.size());
//         APSARA_TEST_EQUAL(1U, info->mCandidateHosts[0].size());
//         APSARA_TEST_EQUAL(1U, info->mCandidateHosts[1].size());
//         APSARA_TEST_EQUAL(1U, info->mCandidateHosts[2].size());
//         APSARA_TEST_EQUAL("project_1.endpoint_2", info->mCandidateHosts[0][0].GetHostname());
//         APSARA_TEST_EQUAL("project_1.endpoint_3", info->mCandidateHosts[1][0].GetHostname());
//         APSARA_TEST_EQUAL("project_1.endpoint_1", info->mCandidateHosts[2][0].GetHostname());
//     }
// }

// void SLSClientManagerUnittest::TestGetCandidateHostsInfo() {
//     const string region = "region";
//     const string project = "project";
//     const string publicEndpoint = region + ".log.aliyuncs.com";
//     const string privateEndpoint = region + "-intranet.log.aliyuncs.com";
//     const string internalEndpoint = region + "-internal.log.aliyuncs.com";
//     const string globalEndpoint = kAccelerationDataEndpoint;
//     const string customEndpoint = "custom.endpoint";
//     const string publicHost = project + "." + publicEndpoint;
//     const string privateHost = project + "." + privateEndpoint;
//     const string internalHost = project + "." + internalEndpoint;
//     const string globalHost = project + "." + globalEndpoint;
//     const string customHost = project + "." + customEndpoint;
//     {
//         // no candidate host info && no region endpoints
//         {
//             // default mode
//             auto info = mManager.GetCandidateHostsInfo(region, project, EndpointMode::DEFAULT);
//             APSARA_TEST_EQUAL(project, info->GetProject());
//             APSARA_TEST_EQUAL(region, info->GetRegion());
//             APSARA_TEST_EQUAL(EndpointMode::DEFAULT, info->GetMode());
//             APSARA_TEST_EQUAL(0U, info->mCandidateHosts.size());
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mProjectCandidateHostsInfosMap[project];
//                 APSARA_TEST_EQUAL(1U, infos.size());
//                 auto& weakInfo = *infos.begin();
//                 APSARA_TEST_FALSE(weakInfo.expired());
//                 APSARA_TEST_EQUAL(info.get(), weakInfo.lock().get());
//             }
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mRegionCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mRegionCandidateHostsInfosMap[region];
//                 APSARA_TEST_EQUAL(1U, infos.size());
//                 auto& weakInfo = *infos.begin();
//                 APSARA_TEST_FALSE(weakInfo.expired());
//                 APSARA_TEST_EQUAL(info.get(), weakInfo.lock().get());
//             }
//             APSARA_TEST_EQUAL(1U, mManager.mUnInitializedCandidateHostsInfos.size());
//             APSARA_TEST_EQUAL(info.get(), mManager.mUnInitializedCandidateHostsInfos[0].lock().get());
//         }
//         {
//             // accelerate mode
//             auto info = mManager.GetCandidateHostsInfo(region, project, EndpointMode::ACCELERATE);
//             APSARA_TEST_EQUAL(project, info->GetProject());
//             APSARA_TEST_EQUAL(region, info->GetRegion());
//             APSARA_TEST_EQUAL(EndpointMode::ACCELERATE, info->GetMode());
//             APSARA_TEST_EQUAL(1U, info->mCandidateHosts.size());
//             APSARA_TEST_EQUAL(1U, info->mCandidateHosts[0].size());
//             APSARA_TEST_EQUAL(globalHost, info->mCandidateHosts[0][0].GetHostname());
//             APSARA_TEST_EQUAL(chrono::milliseconds::max(), info->mCandidateHosts[0][0].GetLatency());
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mProjectCandidateHostsInfosMap[project];
//                 APSARA_TEST_EQUAL(2U, infos.size());
//                 auto it = infos.begin();
//                 APSARA_TEST_TRUE(it->expired());
//                 ++it;
//                 APSARA_TEST_FALSE(it->expired());
//                 APSARA_TEST_EQUAL(info.get(), it->lock().get());
//             }
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mRegionCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mRegionCandidateHostsInfosMap[region];
//                 APSARA_TEST_EQUAL(2U, infos.size());
//                 auto it = infos.begin();
//                 APSARA_TEST_TRUE(it->expired());
//                 ++it;
//                 APSARA_TEST_FALSE(it->expired());
//                 APSARA_TEST_EQUAL(info.get(), it->lock().get());
//             }
//             APSARA_TEST_EQUAL(2U, mManager.mUnInitializedCandidateHostsInfos.size());
//             APSARA_TEST_EQUAL(info.get(), mManager.mUnInitializedCandidateHostsInfos[1].lock().get());
//         }
//         mManager.Clear();
//     }
//     {
//         // no candidate host info && with region endpoints && region endpoint mode is default
//         mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {privateEndpoint});
//         {
//             // default mode
//             auto info = mManager.GetCandidateHostsInfo(region, project, EndpointMode::DEFAULT);
//             APSARA_TEST_EQUAL(project, info->GetProject());
//             APSARA_TEST_EQUAL(region, info->GetRegion());
//             APSARA_TEST_EQUAL(EndpointMode::DEFAULT, info->GetMode());
//             APSARA_TEST_EQUAL(1U, info->mCandidateHosts.size());
//             APSARA_TEST_EQUAL(1U, info->mCandidateHosts[0].size());
//             APSARA_TEST_EQUAL(privateHost, info->mCandidateHosts[0][0].GetHostname());
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mProjectCandidateHostsInfosMap[project];
//                 APSARA_TEST_EQUAL(1U, infos.size());
//                 auto& weakInfo = *infos.begin();
//                 APSARA_TEST_FALSE(weakInfo.expired());
//                 APSARA_TEST_EQUAL(info.get(), weakInfo.lock().get());
//             }
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mRegionCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mRegionCandidateHostsInfosMap[region];
//                 APSARA_TEST_EQUAL(1U, infos.size());
//                 auto& weakInfo = *infos.begin();
//                 APSARA_TEST_FALSE(weakInfo.expired());
//                 APSARA_TEST_EQUAL(info.get(), weakInfo.lock().get());
//             }
//             APSARA_TEST_EQUAL(1U, mManager.mUnInitializedCandidateHostsInfos.size());
//             APSARA_TEST_EQUAL(info.get(), mManager.mUnInitializedCandidateHostsInfos[0].lock().get());
//         }
//         {
//             // accelerate mode
//             auto info = mManager.GetCandidateHostsInfo(region, project, EndpointMode::ACCELERATE);
//             APSARA_TEST_EQUAL(project, info->GetProject());
//             APSARA_TEST_EQUAL(region, info->GetRegion());
//             APSARA_TEST_EQUAL(EndpointMode::ACCELERATE, info->GetMode());
//             APSARA_TEST_EQUAL(1U, info->mCandidateHosts.size());
//             APSARA_TEST_EQUAL(1U, info->mCandidateHosts[0].size());
//             APSARA_TEST_EQUAL(globalHost, info->mCandidateHosts[0][0].GetHostname());
//             APSARA_TEST_EQUAL(chrono::milliseconds::max(), info->mCandidateHosts[0][0].GetLatency());
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mProjectCandidateHostsInfosMap[project];
//                 APSARA_TEST_EQUAL(2U, infos.size());
//                 auto it = infos.begin();
//                 APSARA_TEST_TRUE(it->expired());
//                 ++it;
//                 APSARA_TEST_FALSE(it->expired());
//                 APSARA_TEST_EQUAL(info.get(), it->lock().get());
//             }
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mRegionCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mRegionCandidateHostsInfosMap[region];
//                 APSARA_TEST_EQUAL(2U, infos.size());
//                 auto it = infos.begin();
//                 APSARA_TEST_TRUE(it->expired());
//                 ++it;
//                 APSARA_TEST_FALSE(it->expired());
//                 APSARA_TEST_EQUAL(info.get(), it->lock().get());
//             }
//             APSARA_TEST_EQUAL(2U, mManager.mUnInitializedCandidateHostsInfos.size());
//             APSARA_TEST_EQUAL(info.get(), mManager.mUnInitializedCandidateHostsInfos[1].lock().get());
//         }
//         mManager.Clear();
//     }
//     {
//         // no candidate host info && with region endpoints && region endpoint mode is accelerate
//         mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {globalEndpoint});
//         {
//             // default mode
//             auto info = mManager.GetCandidateHostsInfo(region, project, EndpointMode::DEFAULT);
//             APSARA_TEST_EQUAL(project, info->GetProject());
//             APSARA_TEST_EQUAL(region, info->GetRegion());
//             APSARA_TEST_EQUAL(EndpointMode::ACCELERATE, info->GetMode());
//             APSARA_TEST_EQUAL(1U, info->mCandidateHosts.size());
//             APSARA_TEST_EQUAL(1U, info->mCandidateHosts[0].size());
//             APSARA_TEST_EQUAL(globalHost, info->mCandidateHosts[0][0].GetHostname());
//             APSARA_TEST_EQUAL(chrono::milliseconds::max(), info->mCandidateHosts[0][0].GetLatency());
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mProjectCandidateHostsInfosMap[project];
//                 APSARA_TEST_EQUAL(1U, infos.size());
//                 auto& weakInfo = *infos.begin();
//                 APSARA_TEST_FALSE(weakInfo.expired());
//                 APSARA_TEST_EQUAL(info.get(), weakInfo.lock().get());
//             }
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mRegionCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mRegionCandidateHostsInfosMap[region];
//                 APSARA_TEST_EQUAL(1U, infos.size());
//                 auto& weakInfo = *infos.begin();
//                 APSARA_TEST_FALSE(weakInfo.expired());
//                 APSARA_TEST_EQUAL(info.get(), weakInfo.lock().get());
//             }
//             APSARA_TEST_EQUAL(1U, mManager.mUnInitializedCandidateHostsInfos.size());
//             APSARA_TEST_EQUAL(info.get(), mManager.mUnInitializedCandidateHostsInfos[0].lock().get());
//         }
//         {
//             // accelerate mode
//             auto info = mManager.GetCandidateHostsInfo(region, project, EndpointMode::ACCELERATE);
//             APSARA_TEST_EQUAL(project, info->GetProject());
//             APSARA_TEST_EQUAL(region, info->GetRegion());
//             APSARA_TEST_EQUAL(EndpointMode::ACCELERATE, info->GetMode());
//             APSARA_TEST_EQUAL(1U, info->mCandidateHosts.size());
//             APSARA_TEST_EQUAL(1U, info->mCandidateHosts[0].size());
//             APSARA_TEST_EQUAL(globalHost, info->mCandidateHosts[0][0].GetHostname());
//             APSARA_TEST_EQUAL(chrono::milliseconds::max(), info->mCandidateHosts[0][0].GetLatency());
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mProjectCandidateHostsInfosMap[project];
//                 APSARA_TEST_EQUAL(2U, infos.size());
//                 auto it = infos.begin();
//                 APSARA_TEST_TRUE(it->expired());
//                 ++it;
//                 APSARA_TEST_FALSE(it->expired());
//                 APSARA_TEST_EQUAL(info.get(), it->lock().get());
//             }
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mRegionCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mRegionCandidateHostsInfosMap[region];
//                 APSARA_TEST_EQUAL(2U, infos.size());
//                 auto it = infos.begin();
//                 APSARA_TEST_TRUE(it->expired());
//                 ++it;
//                 APSARA_TEST_FALSE(it->expired());
//                 APSARA_TEST_EQUAL(info.get(), it->lock().get());
//             }
//             APSARA_TEST_EQUAL(2U, mManager.mUnInitializedCandidateHostsInfos.size());
//             APSARA_TEST_EQUAL(info.get(), mManager.mUnInitializedCandidateHostsInfos[1].lock().get());
//         }
//         mManager.Clear();
//     }
//     {
//         // no candidate host info && with region endpoints && region endpoint mode is custom
//         mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {customEndpoint});
//         {
//             // default mode
//             auto info = mManager.GetCandidateHostsInfo(region, project, EndpointMode::DEFAULT);
//             APSARA_TEST_EQUAL(project, info->GetProject());
//             APSARA_TEST_EQUAL(region, info->GetRegion());
//             APSARA_TEST_EQUAL(EndpointMode::CUSTOM, info->GetMode());
//             APSARA_TEST_EQUAL(1U, info->mCandidateHosts.size());
//             APSARA_TEST_EQUAL(1U, info->mCandidateHosts[0].size());
//             APSARA_TEST_EQUAL(customHost, info->mCandidateHosts[0][0].GetHostname());
//             APSARA_TEST_EQUAL(chrono::milliseconds::max(), info->mCandidateHosts[0][0].GetLatency());
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mProjectCandidateHostsInfosMap[project];
//                 APSARA_TEST_EQUAL(1U, infos.size());
//                 auto& weakInfo = *infos.begin();
//                 APSARA_TEST_FALSE(weakInfo.expired());
//                 APSARA_TEST_EQUAL(info.get(), weakInfo.lock().get());
//             }
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mRegionCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mRegionCandidateHostsInfosMap[region];
//                 APSARA_TEST_EQUAL(1U, infos.size());
//                 auto& weakInfo = *infos.begin();
//                 APSARA_TEST_FALSE(weakInfo.expired());
//                 APSARA_TEST_EQUAL(info.get(), weakInfo.lock().get());
//             }
//             APSARA_TEST_EQUAL(1U, mManager.mUnInitializedCandidateHostsInfos.size());
//             APSARA_TEST_EQUAL(info.get(), mManager.mUnInitializedCandidateHostsInfos[0].lock().get());
//         }
//         {
//             // accelerate mode
//             auto info = mManager.GetCandidateHostsInfo(region, project, EndpointMode::ACCELERATE);
//             APSARA_TEST_EQUAL(project, info->GetProject());
//             APSARA_TEST_EQUAL(region, info->GetRegion());
//             APSARA_TEST_EQUAL(EndpointMode::ACCELERATE, info->GetMode());
//             APSARA_TEST_EQUAL(1U, info->mCandidateHosts.size());
//             APSARA_TEST_EQUAL(1U, info->mCandidateHosts[0].size());
//             APSARA_TEST_EQUAL(globalHost, info->mCandidateHosts[0][0].GetHostname());
//             APSARA_TEST_EQUAL(chrono::milliseconds::max(), info->mCandidateHosts[0][0].GetLatency());
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mProjectCandidateHostsInfosMap[project];
//                 APSARA_TEST_EQUAL(2U, infos.size());
//                 auto& weakInfo = *(++infos.begin());
//                 APSARA_TEST_FALSE(weakInfo.expired());
//                 APSARA_TEST_EQUAL(info.get(), weakInfo.lock().get());
//             }
//             {
//                 APSARA_TEST_EQUAL(1U, mManager.mRegionCandidateHostsInfosMap.size());
//                 auto& infos = mManager.mRegionCandidateHostsInfosMap[region];
//                 APSARA_TEST_EQUAL(2U, infos.size());
//                 auto& weakInfo = *(++infos.begin());
//                 APSARA_TEST_FALSE(weakInfo.expired());
//                 APSARA_TEST_EQUAL(info.get(), weakInfo.lock().get());
//             }
//             APSARA_TEST_EQUAL(2U, mManager.mUnInitializedCandidateHostsInfos.size());
//             APSARA_TEST_EQUAL(info.get(), mManager.mUnInitializedCandidateHostsInfos[1].lock().get());
//         }
//         mManager.Clear();
//     }
//     {
//         // candidate host info exists && no region endpoints
//         auto info1 = mManager.GetCandidateHostsInfo(region, project, EndpointMode::DEFAULT);
//         auto info2 = mManager.GetCandidateHostsInfo(region, project, EndpointMode::DEFAULT);
//         APSARA_TEST_EQUAL(info1.get(), info2.get());
//         APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//         APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap[project].size());
//         APSARA_TEST_EQUAL(1U, mManager.mUnInitializedCandidateHostsInfos.size());
//         APSARA_TEST_EQUAL(info1.get(), mManager.mUnInitializedCandidateHostsInfos[0].lock().get());

//         mManager.Clear();
//     }
//     {
//         // candidate host info exists && with region endpoints && region endpoint mode is default
//         mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {privateEndpoint});
//         auto infoDefault = mManager.GetCandidateHostsInfo(region, project, EndpointMode::DEFAULT);
//         auto infoAcc = mManager.GetCandidateHostsInfo(region, project, EndpointMode::ACCELERATE);
//         APSARA_TEST_EQUAL(2U, mManager.mUnInitializedCandidateHostsInfos.size());
//         APSARA_TEST_EQUAL(infoDefault.get(), mManager.mUnInitializedCandidateHostsInfos[0].lock().get());
//         APSARA_TEST_EQUAL(infoAcc.get(), mManager.mUnInitializedCandidateHostsInfos[1].lock().get());
//         {
//             // default mode
//             auto info = mManager.GetCandidateHostsInfo(region, project, EndpointMode::DEFAULT);
//             APSARA_TEST_EQUAL(infoDefault.get(), info.get());
//             APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//             APSARA_TEST_EQUAL(2U, mManager.mProjectCandidateHostsInfosMap[project].size());
//         }
//         {
//             // accelerate mode
//             auto info = mManager.GetCandidateHostsInfo(region, project, EndpointMode::ACCELERATE);
//             APSARA_TEST_EQUAL(infoAcc.get(), info.get());
//             APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//             APSARA_TEST_EQUAL(2U, mManager.mProjectCandidateHostsInfosMap[project].size());
//         }
//         mManager.Clear();
//     }
//     {
//         // candidate host info exists && with region endpoints && region endpoint mode is accelerate
//         mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {globalEndpoint});
//         auto infoAcc = mManager.GetCandidateHostsInfo(region, project, EndpointMode::DEFAULT);
//         APSARA_TEST_EQUAL(1U, mManager.mUnInitializedCandidateHostsInfos.size());
//         APSARA_TEST_EQUAL(infoAcc.get(), mManager.mUnInitializedCandidateHostsInfos[0].lock().get());
//         {
//             // default mode
//             auto info = mManager.GetCandidateHostsInfo(region, project, EndpointMode::DEFAULT);
//             APSARA_TEST_EQUAL(infoAcc.get(), info.get());
//             APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//             APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap[project].size());
//         }
//         {
//             // accelerate mode
//             auto info = mManager.GetCandidateHostsInfo(region, project, EndpointMode::ACCELERATE);
//             APSARA_TEST_EQUAL(infoAcc.get(), info.get());
//             APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//             APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap[project].size());
//         }
//         mManager.Clear();
//     }
//     {
//         // candidate host info exists && with region endpoints && region endpoint mode is custom
//         mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {customEndpoint});
//         auto infoCustom = mManager.GetCandidateHostsInfo(region, project, EndpointMode::DEFAULT);
//         auto infoAcc = mManager.GetCandidateHostsInfo(region, project, EndpointMode::ACCELERATE);
//         APSARA_TEST_EQUAL(2U, mManager.mUnInitializedCandidateHostsInfos.size());
//         APSARA_TEST_EQUAL(infoCustom.get(), mManager.mUnInitializedCandidateHostsInfos[0].lock().get());
//         APSARA_TEST_EQUAL(infoAcc.get(), mManager.mUnInitializedCandidateHostsInfos[1].lock().get());
//         {
//             // default mode
//             auto info = mManager.GetCandidateHostsInfo(region, project, EndpointMode::DEFAULT);
//             APSARA_TEST_EQUAL(infoCustom.get(), info.get());
//             APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//             APSARA_TEST_EQUAL(2U, mManager.mProjectCandidateHostsInfosMap[project].size());
//         }
//         {
//             // accelerate mode
//             auto info = mManager.GetCandidateHostsInfo(region, project, EndpointMode::ACCELERATE);
//             APSARA_TEST_EQUAL(infoAcc.get(), info.get());
//             APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
//             APSARA_TEST_EQUAL(2U, mManager.mProjectCandidateHostsInfosMap[project].size());
//         }
//         mManager.Clear();
//     }
// }

// void SLSClientManagerUnittest::TestUpdateHostInfo() {
//     const string region = "region";
//     const string project = "project";
//     const EndpointMode mode = EndpointMode::DEFAULT;
//     const string endpoint1 = region + ".log.aliyuncs.com";
//     const string endpoint2 = region + "-intranet.log.aliyuncs.com";
//     const string host1 = project + "." + endpoint1;
//     const string host2 = project + "." + endpoint2;
//     mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {endpoint1, endpoint2});
//     {
//         auto info = mManager.GetCandidateHostsInfo(region, project, mode);

//         APSARA_TEST_TRUE(mManager.UpdateHostInfo(project, mode, host1, chrono::milliseconds(100)));
//         APSARA_TEST_EQUAL(chrono::milliseconds(100), info->mCandidateHosts[0][0].GetLatency());
//     }

//     // expired info
//     APSARA_TEST_FALSE(mManager.UpdateHostInfo(project, mode, host1, chrono::milliseconds(50)));

//     // unknown project
//     APSARA_TEST_FALSE(mManager.UpdateHostInfo("unknown_project", mode, host1, chrono::milliseconds(50)));
// }

// void SLSClientManagerUnittest::TestUsingHttps() {
//     const string region = "region";
//     const string host = region + ".log.aliyuncs.com";
//     mManager.UpdateLocalRegionEndpointsAndHttpsInfo(region, {host});
//     APSARA_TEST_FALSE(mManager.UsingHttps(region));

//     AppConfig::GetInstance()->mSendDataPort = 443;
//     APSARA_TEST_TRUE(mManager.UsingHttps(region));
// }

// void SLSClientManagerUnittest::TestProbeNetwork() {
//     ProbeNetworkMock::Prepare();
//     const string publicHost1 = ProbeNetworkMock::project1 + "." + ProbeNetworkMock::publicEndpoint1;
//     const string privateHost1 = ProbeNetworkMock::project1 + "." + ProbeNetworkMock::privateEndpoint1;
//     const string internalHost1 = ProbeNetworkMock::project1 + "." + ProbeNetworkMock::internalEndpoint1;
//     const string globalHost1 = ProbeNetworkMock::project1 + "." + ProbeNetworkMock::globalEndpoint;
//     const string publicHost2 = ProbeNetworkMock::project2 + "." + ProbeNetworkMock::publicEndpoint2;
//     const string globalHost2 = ProbeNetworkMock::project2 + "." + ProbeNetworkMock::globalEndpoint;
//     const string globalHost3 = ProbeNetworkMock::project3 + "." + ProbeNetworkMock::globalEndpoint;
//     const string customHost3 = ProbeNetworkMock::project3 + "." + ProbeNetworkMock::customEndpoint;

//     // ! means not available
//     vector<shared_ptr<CandidateHostsInfo>> infos;
//     infos.push_back(mManager.GetCandidateHostsInfo(
//         ProbeNetworkMock::region1, ProbeNetworkMock::project1, EndpointMode::DEFAULT)); // public !internal !private
//     infos.push_back(mManager.GetCandidateHostsInfo(
//         ProbeNetworkMock::region1, ProbeNetworkMock::project1, EndpointMode::ACCELERATE)); // !accelerate public
//     infos.push_back(mManager.GetCandidateHostsInfo(
//         ProbeNetworkMock::region2, ProbeNetworkMock::project2, EndpointMode::DEFAULT)); // accelerate public
//     infos.push_back(mManager.GetCandidateHostsInfo(
//         ProbeNetworkMock::region3, ProbeNetworkMock::project3, EndpointMode::DEFAULT)); // custom
//     infos.push_back(mManager.GetCandidateHostsInfo(
//         ProbeNetworkMock::region3, ProbeNetworkMock::project3, EndpointMode::ACCELERATE)); // !accelerate

//     // probe uninitialized host
//     mManager.DoProbeUnInitializedHost();
//     APSARA_TEST_TRUE(mManager.mUnInitializedCandidateHostsInfos.empty());
//     APSARA_TEST_EQUAL(infos.size(), mManager.mPartiallyInitializedCandidateHostsInfos.size());
//     for (size_t i = 0; i < infos.size(); ++i) {
//         APSARA_TEST_EQUAL(infos[i].get(), mManager.mPartiallyInitializedCandidateHostsInfos[i].lock().get());
//     }

//     APSARA_TEST_EQUAL(publicHost1, infos[0]->GetCurrentHost());
//     APSARA_TEST_EQUAL("", infos[1]->GetCurrentHost());
//     APSARA_TEST_EQUAL(globalHost2, infos[2]->GetCurrentHost());
//     APSARA_TEST_EQUAL(customHost3, infos[3]->GetCurrentHost());
//     APSARA_TEST_EQUAL("", infos[4]->GetCurrentHost());
//     vector<size_t> len = {1, 2, 1, 1, 1};
//     for (size_t i = 0; i < infos.size(); ++i) {
//         vector<string> res;
//         infos[i]->GetProbeHosts(res);
//         APSARA_TEST_EQUAL(len[i], res.size());
//     }

//     // probe partially initialized host
//     mManager.DoProbeHost();
//     APSARA_TEST_TRUE(mManager.mPartiallyInitializedCandidateHostsInfos.empty());

//     APSARA_TEST_EQUAL(publicHost1, infos[0]->GetCurrentHost());
//     APSARA_TEST_EQUAL(publicHost1, infos[1]->GetCurrentHost());
//     APSARA_TEST_EQUAL(globalHost2, infos[2]->GetCurrentHost());
//     APSARA_TEST_EQUAL(customHost3, infos[3]->GetCurrentHost());
//     APSARA_TEST_EQUAL("", infos[4]->GetCurrentHost());
//     len = {1, 1, 2, 1, 1};
//     for (size_t i = 0; i < infos.size(); ++i) {
//         vector<string> res;
//         infos[i]->GetProbeHosts(res);
//         APSARA_TEST_EQUAL(len[i], res.size());
//     }

//     vector<vector<chrono::milliseconds>> oldLatencies;
//     for (const auto& info : infos) {
//         auto& latency = oldLatencies.emplace_back();
//         for (const auto& item : info->mCandidateHosts) {
//             for (const auto& entry : item) {
//                 latency.push_back(entry.GetLatency());
//             }
//         }
//     }

//     // probe all avaialble hosts
//     INT32_FLAG(sls_hosts_probe_interval_sec) = 0;
//     mManager.DoProbeHost();

//     APSARA_TEST_EQUAL(publicHost1, infos[0]->GetCurrentHost());
//     APSARA_TEST_EQUAL(publicHost1, infos[1]->GetCurrentHost());
//     APSARA_TEST_EQUAL(globalHost2, infos[2]->GetCurrentHost());
//     APSARA_TEST_EQUAL(customHost3, infos[3]->GetCurrentHost());
//     APSARA_TEST_EQUAL("", infos[4]->GetCurrentHost());
//     len = {1, 1, 2, 1, 1};
//     for (size_t i = 0; i < infos.size(); ++i) {
//         vector<string> res;
//         infos[i]->GetProbeHosts(res);
//         APSARA_TEST_EQUAL(len[i], res.size());
//     }

//     vector<vector<chrono::milliseconds>> newLatencies;
//     for (const auto& info : infos) {
//         auto& latency = newLatencies.emplace_back();
//         for (const auto& item : info->mCandidateHosts) {
//             for (const auto& entry : item) {
//                 latency.push_back(entry.GetLatency());
//             }
//         }
//     }

//     for (size_t i = 0; i < oldLatencies.size(); ++i) {
//         for (size_t j = 0; j < oldLatencies[i].size(); ++j) {
//             if (oldLatencies[i][j] != chrono::milliseconds::max()) {
//                 APSARA_TEST_NOT_EQUAL(newLatencies[i][j], oldLatencies[i][j]);
//             } else {
//                 APSARA_TEST_EQUAL(newLatencies[i][j], oldLatencies[i][j]);
//             }
//         }
//     }
//     INT32_FLAG(sls_hosts_probe_interval_sec) = 60;

//     // probe all hosts
//     INT32_FLAG(sls_all_hosts_probe_interval_sec) = 0;
//     ProbeNetworkMock::enableUnavailableHosts = true;
//     mManager.DoProbeHost();

//     APSARA_TEST_EQUAL(publicHost1, infos[0]->GetCurrentHost());
//     APSARA_TEST_EQUAL(globalHost1, infos[1]->GetCurrentHost());
//     APSARA_TEST_EQUAL(globalHost2, infos[2]->GetCurrentHost());
//     APSARA_TEST_EQUAL(customHost3, infos[3]->GetCurrentHost());
//     APSARA_TEST_EQUAL(globalHost3, infos[4]->GetCurrentHost());

//     len = {3, 2, 2, 1, 1};
//     for (size_t i = 0; i < infos.size(); ++i) {
//         vector<string> res;
//         infos[i]->GetProbeHosts(res);
//         APSARA_TEST_EQUAL(len[i], res.size());
//     }
//     ProbeNetworkMock::enableUnavailableHosts = false;
//     INT32_FLAG(sls_all_hosts_probe_interval_sec) = 300;

//     // expired info
//     infos[2].reset();
//     infos[4].reset();
//     mManager.DoProbeHost();
//     APSARA_TEST_EQUAL(2U, mManager.mProjectCandidateHostsInfosMap.size());
//     APSARA_TEST_EQUAL(2U, mManager.mProjectCandidateHostsInfosMap[ProbeNetworkMock::project1].size());
//     APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap[ProbeNetworkMock::project3].size());
//     APSARA_TEST_EQUAL(2U, mManager.mRegionCandidateHostsInfosMap.size());
//     APSARA_TEST_EQUAL(2U, mManager.mRegionCandidateHostsInfosMap[ProbeNetworkMock::region1].size());
//     APSARA_TEST_EQUAL(1U, mManager.mRegionCandidateHostsInfosMap[ProbeNetworkMock::region3].size());
// }

// void SLSClientManagerUnittest::TestRealIp() {
//     BOOL_FLAG(send_prefer_real_ip) = true;
//     mManager.mDoProbeNetwork = GetRealIpMock::DoProbeNetwork;

//     const string unknownRegion = "unknown_region";
//     const string normalRegionEndpoint1 = GetRealIpMock::normalRegion + "-intranet.log.aliyuncs.com";
//     const string normalRegionEndpoint2 = GetRealIpMock::normalRegion + ".log.aliyuncs.com";
//     const string unavailableRegionEndpoint = GetRealIpMock::unavailableRegion + ".log.aliyuncs.com";
//     const string unknownRegionEndpoint = unknownRegion + ".log.aliyuncs.com";
//     mManager.UpdateRemoteRegionEndpoints(GetRealIpMock::normalRegion, {normalRegionEndpoint1,
//     normalRegionEndpoint2}); mManager.UpdateRemoteRegionEndpoints(GetRealIpMock::unavailableRegion,
//     {unavailableRegionEndpoint}); mManager.UpdateRemoteRegionEndpoints(unknownRegion, {unknownRegionEndpoint});
//     APSARA_TEST_EQUAL(3U, mManager.mRegionRealIpCandidateHostsInfosMap.size());
//     {
//         vector<string> hosts;
//         mManager.mRegionRealIpCandidateHostsInfosMap.at(GetRealIpMock::normalRegion).GetAllHosts(hosts);
//         APSARA_TEST_EQUAL(2U, hosts.size());
//         APSARA_TEST_EQUAL(normalRegionEndpoint1, hosts[0]);
//         APSARA_TEST_EQUAL(normalRegionEndpoint2, hosts[1]);
//     }
//     {
//         vector<string> hosts;
//         mManager.mRegionRealIpCandidateHostsInfosMap.at(GetRealIpMock::unavailableRegion).GetAllHosts(hosts);
//         APSARA_TEST_EQUAL(1U, hosts.size());
//         APSARA_TEST_EQUAL(unavailableRegionEndpoint, hosts[0]);
//     }
//     {
//         vector<string> hosts;
//         mManager.mRegionRealIpCandidateHostsInfosMap.at(unknownRegion).GetAllHosts(hosts);
//         APSARA_TEST_EQUAL(1U, hosts.size());
//         APSARA_TEST_EQUAL(unknownRegionEndpoint, hosts[0]);
//     }

//     // no endpoint available
//     mManager.DoUpdateRealIp();
//     APSARA_TEST_EQUAL(0U, mManager.mRegionRealIpMap.size());

//     // probe host
//     INT32_FLAG(sls_all_hosts_probe_interval_sec) = 0;
//     mManager.DoProbeHost();
//     INT32_FLAG(sls_all_hosts_probe_interval_sec) = 300;
//     for (const auto& item : mManager.mRegionRealIpCandidateHostsInfosMap) {
//         APSARA_TEST_NOT_EQUAL("", item.second.GetCurrentHost());
//     }

//     // update all regions
//     INT32_FLAG(send_switch_real_ip_interval) = 0;
//     mManager.UpdateOutdatedRealIpRegions(GetRealIpMock::unavailableRegion);
//     mManager.DoUpdateRealIp();
//     APSARA_TEST_TRUE(mManager.mOutdatedRealIpRegions.empty());
//     APSARA_TEST_EQUAL(2U, mManager.mRegionRealIpMap.size());
//     APSARA_TEST_EQUAL(GetRealIpMock::normalRegionIps[0], mManager.GetRealIp(GetRealIpMock::normalRegion));
//     APSARA_TEST_EQUAL(GetRealIpMock::unavailableRegionIps[0], mManager.GetRealIp(GetRealIpMock::unavailableRegion));
//     INT32_FLAG(send_switch_real_ip_interval) = 60;

//     // update only outdated regions
//     mManager.UpdateOutdatedRealIpRegions(GetRealIpMock::normalRegion);
//     mManager.DoUpdateRealIp();
//     APSARA_TEST_TRUE(mManager.mOutdatedRealIpRegions.empty());
//     APSARA_TEST_EQUAL(2U, mManager.mRegionRealIpMap.size());
//     APSARA_TEST_EQUAL(GetRealIpMock::normalRegionIps[1], mManager.GetRealIp(GetRealIpMock::normalRegion));
//     APSARA_TEST_EQUAL(GetRealIpMock::unavailableRegionIps[0], mManager.GetRealIp(GetRealIpMock::unavailableRegion));

//     mManager.mDoProbeNetwork = ProbeNetworkMock::DoProbeNetwork;
//     BOOL_FLAG(send_prefer_real_ip) = false;
// }

void SLSClientManagerUnittest::TestAccessKeyManagement() {
    string accessKeyId, accessKeySecret;
    SLSClientManager::AuthType type;
    mManager.GetAccessKey("", type, accessKeyId, accessKeySecret);
    APSARA_TEST_EQUAL(SLSClientManager::AuthType::AK, type);
    APSARA_TEST_EQUAL(STRING_FLAG(default_access_key_id), accessKeyId);
    APSARA_TEST_EQUAL(STRING_FLAG(default_access_key), accessKeySecret);
}

void SLSClientManagerUnittest::TestGetCandidateHostsInfo() {
    const string project = "project";
    const string endpoint = "endpoint";
    CandidateHostsInfo* infoPtr = nullptr;
    {
        auto info1 = mManager.GetCandidateHostsInfo(project, endpoint);
        auto info2 = mManager.GetCandidateHostsInfo(project, endpoint);
        infoPtr = info1.get();
        APSARA_TEST_EQUAL(info1.get(), info2.get());
        APSARA_TEST_EQUAL(project, info1->GetProject());
        APSARA_TEST_EQUAL("", info1->GetRegion());
        APSARA_TEST_EQUAL(EndpointMode::CUSTOM, info1->GetMode());
        APSARA_TEST_TRUE(info1->IsInitialized());
        APSARA_TEST_EQUAL(project + "." + endpoint, info1->GetCurrentHost());
        APSARA_TEST_EQUAL(1U, info1->mCandidateHosts.size());
        APSARA_TEST_EQUAL(1U, info1->mCandidateHosts[0].size());
        APSARA_TEST_EQUAL(project + "." + endpoint, info1->mCandidateHosts[0][0].GetHostname());
        APSARA_TEST_EQUAL(chrono::milliseconds(10), info1->mCandidateHosts[0][0].GetLatency());
        APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
        auto& infos = mManager.mProjectCandidateHostsInfosMap[project];
        APSARA_TEST_EQUAL(1U, infos.size());
        auto& weakInfo = *infos.begin();
        APSARA_TEST_FALSE(weakInfo.expired());
        APSARA_TEST_EQUAL(info1.get(), weakInfo.lock().get());
    }
    {
        auto info = mManager.GetCandidateHostsInfo(project, endpoint);
        APSARA_TEST_NOT_EQUAL(infoPtr, info.get());
        APSARA_TEST_EQUAL(1U, mManager.mProjectCandidateHostsInfosMap.size());
        APSARA_TEST_EQUAL(2U, mManager.mProjectCandidateHostsInfosMap[project].size());
    }
}

// UNIT_TEST_CASE(SLSClientManagerUnittest, TestLocalRegionEndpoints)
// UNIT_TEST_CASE(SLSClientManagerUnittest, TestRemoteRegionEndpoints)
// UNIT_TEST_CASE(SLSClientManagerUnittest, TestGetCandidateHostsInfo)
// UNIT_TEST_CASE(SLSClientManagerUnittest, TestUpdateHostInfo)
// UNIT_TEST_CASE(SLSClientManagerUnittest, TestUsingHttps)
// UNIT_TEST_CASE(SLSClientManagerUnittest, TestProbeNetwork)
// UNIT_TEST_CASE(SLSClientManagerUnittest, TestRealIp)
UNIT_TEST_CASE(SLSClientManagerUnittest, TestAccessKeyManagement)
UNIT_TEST_CASE(SLSClientManagerUnittest, TestGetCandidateHostsInfo)

} // namespace logtail

UNIT_TEST_MAIN
