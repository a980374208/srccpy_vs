#pragma once
#include "sc_intr.h"
#include <vector>
#include "sc_process.h"
#include "adb_device.h"


#define SC_ADB_NO_STDOUT (1 << 0)
#define SC_ADB_NO_STDERR (1 << 1)
#define SC_ADB_NO_LOGERR (1 << 2)

#define SC_ADB_SILENT (SC_ADB_NO_STDOUT | SC_ADB_NO_STDERR | SC_ADB_NO_LOGERR)

enum sc_adb_device_selector_type {
    SC_ADB_DEVICE_SELECT_ALL,
    SC_ADB_DEVICE_SELECT_SERIAL,
    SC_ADB_DEVICE_SELECT_USB,
    SC_ADB_DEVICE_SELECT_TCPIP,
};

struct sc_adb_device_selector {
    enum sc_adb_device_selector_type type;
    std::string serial;
};


bool sc_adb_init();
std::string
sc_adb_get_executable(void);

sc_pid sc_adb_execute(const std::vector<std::string> &commands, unsigned flags);

static sc_pid sc_adb_execute_p(const std::vector<std::string> &commands, unsigned flags, sc_pipe* pout);

bool sc_adb_start_server(sc_intr* intr, unsigned flags);

static bool
sc_adb_list_devices(sc_intr& intr, unsigned flags,
    sc_vec_adb_devices& out_vec);

bool sc_adb_select_device(class sc_intr& intr,
    const struct sc_adb_device_selector* selector,
    unsigned flags, struct sc_adb_device* out_device);

static bool
process_check_success_intr(sc_intr& intr, sc_pid pid, const char* name,
    unsigned flags);

static bool
process_check_success_internal(sc_pid pid, const char* name, bool close,
    unsigned flags);