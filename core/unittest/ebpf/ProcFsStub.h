#include <cstddef>

#include <filesystem>
#include <fstream>
#include <string>

#include "ProcParser.h"
#include "common/ProcParser.h"
#include "common/TimeUtil.h"

using namespace logtail;

Proc CreateStubProc() {
    Proc proc;
    proc.pid = 1;
    static const char args[] = "\0arg1\0arg2";
    proc.comm = "test program";
    proc.cmdline = proc.comm + std::string(args, sizeof(args) - 1);
    proc.exe = "/usr/bin/test program";
    proc.cwd = "/home/user";
    proc.ppid = 0;
    proc.ktime = 0;

    // Create environ file with null separators
    static const char environ[] = "PATH=/usr/bin\0USER=root\0HOME=/root";
    proc.environ = std::string(environ, sizeof(environ) - 1);

    proc.realUid = 0;
    proc.effectiveUid = 0;
    proc.savedUid = 0;
    proc.fsUid = 0;

    proc.realGid = 0;
    proc.effectiveGid = 0;
    proc.savedGid = 0;
    proc.fsGid = 0;

    proc.auid = 0;

    proc.inheritable = 0;
    proc.effective = 0;
    proc.permitted = 0;

    proc.container_id = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";

    proc.cgroup_ns = 4026531835;
    proc.ipc_ns = 4026535741;
    proc.mnt_ns = 4026535739;
    proc.net_ns = 4026531992;
    proc.pid_ns = 4026535742;
    proc.pid_for_children_ns = 4026535742;
    proc.time_ns = 4026531834;
    proc.time_for_children_ns = 4026531834;
    proc.user_ns = 4026531837;
    proc.uts_ns = 4026535740;
    return proc;
}

class ProcFsStub {
    ProcFsStub(std::filesystem::path& procDir) : mProcDir(procDir) {}

    void SetUp() { std::filesystem::create_directories(mProcDir); }

    void CreatePidDir(const Proc& proc) {
        auto pidDir = mProcDir / std::to_string(proc.pid);
        std::filesystem::create_directories(pidDir);

        WriteStringWithNulls(pidDir / "cmdline", proc.cmdline);

        // Create comm file
        WriteStringWithNulls(pidDir / "comm", proc.comm);

        WriteStringWithNulls(pidDir / "environ", proc.environ);

        {
            std::ofstream stat(pidDir / "stat");
            stat << proc.pid << " (" << proc.comm << ") S " << proc.ppid
                 << " 1 1 0 -1 4194560 26161309 468512616 0 5596 6902 28376 5714130 192403 20 0 1 0 "
                 << proc.ktime * GetTicksPerSecond() / kNanoPerSeconds
                 << " 5578752 645 "
                    "18446744073709551615 4194304 5100836 140725119433184 0 0 0 65536 4 81922 0 0 0 17 10 0 0 0 0 0 "
                    "7200240 "
                    "7236240 11145216 140725119437764 140725119437864 140725119437864 140725119438832 0";
        }
        // Create status file
        {
            std::ofstream status(pidDir / "status");
            status << "Name:   " << proc.comm << "\n"
                   << "NStgid: " << proc.pid << "\n"
                   << "Uid:    " << proc.realUid << "\t" << proc.effectiveUid << "\t" << proc.savedUid << "\t"
                   << proc.fsUid << "\n"
                   << "Gid:    " << proc.realGid << "\t" << proc.effectiveGid << "\t" << proc.savedGid << "\t"
                   << proc.fsGid << "\n"
                   << std::hex << std::setfill('0') << "CapInh: " << std::setw(16) << proc.inheritable << "\n"
                   << "CapPrm: " << std::setw(16) << proc.permitted << "\n"
                   << "CapEff: " << std::setw(16) << proc.effective << "\n"
                   << std::dec << std::setfill(' ');
        }
        // std::ifstream status(pidDir / "status");
        // std::cerr << status.rdbuf();

        // Create loginuid file
        { std::ofstream(pidDir / "loginuid") << proc.auid; }
        // Create cgroup file
        { std::ofstream(pidDir / "cgroup") << "9:pids:/docker/" << proc.container_id; }
        // Create exe symlink
        if (!proc.exe.empty()) {
            std::filesystem::create_symlink(proc.exe, pidDir / "exe");
        }
        // Create cwd symlink
        if (!proc.cwd.empty()) {
            std::filesystem::create_symlink(proc.cwd, pidDir / "cwd");
        }
        // Create ns directory and net symlink
        auto nsDir = pidDir / "ns";
        std::filesystem::create_directories(nsDir);
        std::filesystem::create_symlink("cgroup:[" + std::to_string(proc.cgroup_ns) + "]", nsDir / "cgroup");
        std::filesystem::create_symlink("ipc:[" + std::to_string(proc.ipc_ns) + "]", nsDir / "ipc");
        std::filesystem::create_symlink("mnt:[" + std::to_string(proc.mnt_ns) + "]", nsDir / "mnt");
        std::filesystem::create_symlink("net:[" + std::to_string(proc.net_ns) + "]", nsDir / "net");
        std::filesystem::create_symlink("pid:[" + std::to_string(proc.pid_ns) + "]", nsDir / "pid");
        std::filesystem::create_symlink("pid_for_children:[" + std::to_string(proc.pid_for_children_ns) + "]",
                                        nsDir / "pid_for_children");
        std::filesystem::create_symlink("time:[" + std::to_string(proc.time_ns) + "]", nsDir / "time");
        std::filesystem::create_symlink("time_for_children:[" + std::to_string(proc.time_for_children_ns) + "]",
                                        nsDir / "time_for_children");
        std::filesystem::create_symlink("user:[" + std::to_string(proc.user_ns) + "]", nsDir / "user");
        std::filesystem::create_symlink("uts:[" + std::to_string(proc.uts_ns) + "]", nsDir / "uts");
    }

    void TearDown() { std::filesystem::remove_all(mProcDir); }

private:
    void WriteStringWithNulls(const std::filesystem::path& path, const std::string& data) {
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(data.data(), data.size());
    }

    std::filesystem::path mProcDir;
};
