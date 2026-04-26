#pragma once
#include <vector>
#include <string>

#ifdef _WIN32
# include <winsock2.h>
# include <windows.h>
# define SC_PRIexitcode "lu"
# define SC_PROCESS_NONE NULL
# define SC_EXIT_CODE_NONE -1UL // max value as unsigned long
typedef HANDLE sc_pid;
typedef DWORD sc_exit_code;
typedef HANDLE sc_pipe;
typedef HANDLE sc_pid;
typedef SSIZE_T ssize_t;
#else
# include <sys/types.h>
# define SC_PRIexitcode "d"
# define SC_PROCESS_NONE -1
# define SC_EXIT_CODE_NONE -1
typedef pid_t sc_pid;
typedef int sc_exit_code;
typedef int sc_pipe;
#endif


enum sc_process_result {
    SC_PROCESS_SUCCESS,
    SC_PROCESS_ERROR_GENERIC,
    SC_PROCESS_ERROR_MISSING_BINARY,
};

#define SC_PROCESS_NO_STDOUT (1 << 0)
#define SC_PROCESS_NO_STDERR (1 << 1)


enum sc_process_result
    sc_process_execute_p(const std::vector<std::string> commands, HANDLE* handle, unsigned flags,
        HANDLE* pin, HANDLE* pout, HANDLE* perr);


static bool
build_cmd(std::string& cmd, size_t len, const std::vector<std::string>& commands);

ssize_t
sc_pipe_read(HANDLE pipe, char* data, size_t len);

ssize_t
sc_pipe_read_all(sc_pipe pipe, char* data, size_t len);

bool
sc_process_terminate(HANDLE handle);

void
sc_pipe_close(HANDLE pipe);

void
sc_process_close(HANDLE handle);

sc_exit_code
sc_process_wait(HANDLE handle, bool close);