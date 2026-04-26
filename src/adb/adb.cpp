#include "adb.h"
#include "env.h"
#include <vector>
#include "process_intr.h"
#include "assert.h"
#include "adb_parser.h"

#define SC_ADB_COMMAND(...) { sc_adb_get_executable(), __VA_ARGS__ }


static std::string adb_executable = "";
bool sc_adb_init()
{

    adb_executable = sc_get_env("ADB");
    if (!adb_executable.empty()) {
        //LOGD("Using adb: %s", adb_executable.c_str());
        return true;
    }

#if !defined(PORTABLE) || defined(_WIN32)
    adb_executable = "adb";
#else
    // For portable builds, use the absolute path to the adb executable
    // in the same directory as scrcpy (except on Windows, where "adb"
    // is sufficient)
    adb_executable = sc_file_get_local_path("adb");
    if (!adb_executable) {
        // Error already logged
        return false;
    }

    LOGD("Using adb (portable): %s", adb_executable);
#endif
    return true;
}

std::string sc_adb_get_executable(void)
{
    return adb_executable;
}

sc_pid sc_adb_execute(const std::vector<std::string> &commands, unsigned flags)
{
    return sc_adb_execute_p(commands, flags, NULL);
}

sc_pid sc_adb_execute_p(const std::vector<std::string>& commands, unsigned flags, sc_pipe* pout)
{
    unsigned process_flags = 0;
    if (flags & SC_ADB_NO_STDOUT) {
        process_flags |= SC_PROCESS_NO_STDOUT;
    }
    if (flags & SC_ADB_NO_STDERR) {
        process_flags |= SC_PROCESS_NO_STDERR;
    }

    sc_pid pid;
    enum sc_process_result r =
        sc_process_execute_p(commands, &pid, process_flags, NULL, pout, NULL);
    if (r != SC_PROCESS_SUCCESS) {
        // If the execution itself failed (not the command exit code), log the
        // error in all cases
        //show_adb_err_msg(r, argv);
        pid = SC_PROCESS_NONE;
    }

    return pid;
}

bool sc_adb_start_server(sc_intr* intr, unsigned flags)
{
    std::vector<std::string> commands = SC_ADB_COMMAND("start-server");
    sc_pid pid = sc_adb_execute(commands, flags);

    return false;
}

bool sc_adb_list_devices(sc_intr& intr, unsigned flags, sc_vec_adb_devices& out_vec)
{
    std::vector<std::string> commands = SC_ADB_COMMAND("devices", "-l");

#define BUFSIZE 65536
    std::vector<char> buf(BUFSIZE);

    sc_pipe pout;
    sc_pid pid = sc_adb_execute_p(commands, flags, &pout);
    if (pid == SC_PROCESS_NONE) {
        // LOGE("Could not execute adb devices -l");
        return false;
    }

    size_t r = sc_pipe_read_all_intr(intr, pid, pout, buf.data(), BUFSIZE - 1);
    sc_pipe_close(pout);

	bool ok = process_check_success_intr(intr, pid, "adb devices -l", flags);

    if (!ok) {
        return false;
    }

    if (r == -1 || r >= BUFSIZE - 1) {
        // LOGE(" too large");
        return false;
    }
    
    ok = sc_adb_parse_devices(std::string_view(buf.data(), r), out_vec);

    return ok;
}



bool sc_adb_select_device(sc_intr& intr, const sc_adb_device_selector* selector, unsigned flags, sc_adb_device* out_device)
{
    sc_vec_adb_devices vec;
    bool ok = sc_adb_list_devices(intr, flags, vec);

    return false;
}

bool process_check_success_intr(sc_intr& intr, sc_pid pid, const char* name, unsigned flags)
{
    if (!intr.set_process(pid)) {
        // Already interrupted
        return false;
    }

    // Always pass close=false, interrupting would be racy otherwise
    bool ret = process_check_success_internal(pid, name, false, flags);


    intr.set_process(SC_PROCESS_NONE);
    // Close separately
    sc_process_close(pid);

    return ret;
}

bool process_check_success_internal(sc_pid pid, const char* name, bool close, unsigned flags)
{
    bool log_errors = !(flags & SC_ADB_NO_LOGERR);

    if (pid == SC_PROCESS_NONE) {
        if (log_errors) {
            //LOGE("Could not execute \"%s\"", name);
        }
        return false;
    }
    sc_exit_code exit_code = sc_process_wait(pid, close);
    if (exit_code) {
        if (log_errors) {
            if (exit_code != SC_EXIT_CODE_NONE) {
                //LOGE("\"%s\" returned with value %" SC_PRIexitcode, name,
                //    exit_code);
            }
            else {
                //LOGE("\"%s\" exited unexpectedly", name);
            }
        }
        return false;
    }
    return true;
}
