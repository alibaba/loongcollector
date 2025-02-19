#include <coolbpf/net.h>
#include <coolbpf/security.skel.h>
#include <sys/resource.h>

#include "ebpf/driver/eBPFWrapper.h"
#include "unittest/Unittest.h"

namespace logtail {
namespace ebpf {

class eBPFWrapperUnittest : public ::testing::Test {
public:
    eBPFWrapperUnittest() {
        struct rlimit rlim_new = {
            .rlim_cur = RLIM_INFINITY,
            .rlim_max = RLIM_INFINITY,
        };

        if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
            exit(1);
        }
    }
    void TestInitialization();
    void TestMapOperations();
    void TestPerfBufferOperations();
    void TestAttachOperations();
    void TestTailCall();

protected:
    void SetUp() override { mWrapper = std::make_shared<BPFWrapper<security_bpf>>(); }

    void TearDown() override {
        if (mWrapper) {
            mWrapper->Destroy();
        }
    }

private:
    std::shared_ptr<BPFWrapper<security_bpf>> mWrapper;
};

void eBPFWrapperUnittest::TestInitialization() {
    APSARA_TEST_EQUAL(mWrapper->Init(), 0);
}

void eBPFWrapperUnittest::TestMapOperations() {
    // APSARA_TEST_EQUAL(mWrapper->Init(), 0);

    // uint32_t key = 1;
    // uint32_t value = 100;

    // APSARA_TEST_EQUAL(
    //     mWrapper->UpdateBPFHashMap("test_map", &key, &value, 0),
    //     0
    // );

    // // 查找操作
    // uint32_t lookup_value = 0;
    // APSARA_TEST_EQUAL(
    //     mWrapper->LookupBPFHashMap("test_map", &key, &lookup_value),
    //     0
    // );
    // APSARA_TEST_EQUAL(lookup_value, value);

    // // 删除操作
    // APSARA_TEST_EQUAL(
    //     mWrapper->RemoveBPFHashMap("test_map", &key),
    //     0
    // );
}

void eBPFWrapperUnittest::TestPerfBufferOperations() {
    APSARA_TEST_EQUAL(mWrapper->Init(), 0);

    auto sample_cb = [](void* ctx, int cpu, void* data, uint32_t size) {};
    auto lost_cb = [](void* ctx, int cpu, __u64 cnt) {};

    void* pb = mWrapper->CreatePerfBuffer("file_secure_output", 8, nullptr, sample_cb, lost_cb);
    APSARA_TEST_TRUE(pb != nullptr);

    mWrapper->DeletePerfBuffer(pb);
}

void eBPFWrapperUnittest::TestAttachOperations() {
    // APSARA_TEST_EQUAL(mWrapper->Init(), 0);

    // // 测试动态附加
    // std::vector<AttachProgOps> attach_ops = {
    //     {"test_prog", true}
    // };

    // APSARA_TEST_EQUAL(
    //     mWrapper->DynamicAttachBPFObject(attach_ops),
    //     0
    // );

    // // 测试动态分离
    // APSARA_TEST_EQUAL(
    //     mWrapper->DynamicDetachBPFObject(attach_ops),
    //     0
    // );
}

void eBPFWrapperUnittest::TestTailCall() {
    APSARA_TEST_EQUAL(mWrapper->Init(), 0);

    std::pair<const std::string, const std::vector<std::string>> tailCall
        = {"execve_calls", {"execve_rate", "execve_send"}};
    APSARA_TEST_EQUAL(mWrapper->SetTailCall(tailCall.first, tailCall.second), 0);
    // query
    int idx = 0;
    int progId = -1;
    int progFd = -1;

    APSARA_TEST_TRUE(mWrapper->LookupBPFHashMap("execve_calls", &idx, &progId) == 0);
    APSARA_TEST_TRUE(progId >= 0);
    progFd = mWrapper->GetBPFProgFdById(progId);
    APSARA_TEST_TRUE(progFd >= 0);

    idx = 1;
    progFd = -1;
    APSARA_TEST_TRUE(mWrapper->LookupBPFHashMap("execve_calls", &idx, &progId) == 0);
    APSARA_TEST_TRUE(progId >= 0);
    progFd = mWrapper->GetBPFProgFdById(progId);
    APSARA_TEST_TRUE(progFd >= 0);
}

UNIT_TEST_CASE(eBPFWrapperUnittest, TestInitialization);
// UNIT_TEST_CASE(eBPFWrapperUnittest, TestMapOperations);
UNIT_TEST_CASE(eBPFWrapperUnittest, TestPerfBufferOperations);
// UNIT_TEST_CASE(eBPFWrapperUnittest, TestAttachOperations);
UNIT_TEST_CASE(eBPFWrapperUnittest, TestTailCall);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
