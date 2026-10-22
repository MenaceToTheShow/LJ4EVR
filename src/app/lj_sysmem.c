/* lj_sysmem.c - see lj_sysmem.h. Windows GlobalMemoryStatusEx, macOS host_statistics64
 * (free + inactive pages) and hw.memsize, elsewhere sysconf. */
#include "app/lj_sysmem.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
int lj_sysmem_query(uint64_t *avail, uint64_t *total)
{
    MEMORYSTATUSEX m;
    m.dwLength = sizeof m;
    if (!GlobalMemoryStatusEx(&m)) return -1;
    *avail = m.ullAvailPhys; *total = m.ullTotalPhys;
    return 0;
}
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
int lj_sysmem_query(uint64_t *avail, uint64_t *total)
{
    vm_statistics64_data_t vs;
    mach_msg_type_number_t n = HOST_VM_INFO64_COUNT;
    vm_size_t page = 0;
    uint64_t mem = 0;
    size_t len = sizeof mem;
    if (host_page_size(mach_host_self(), &page) != KERN_SUCCESS || host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vs, &n) != KERN_SUCCESS) return -1;
    if (sysctlbyname("hw.memsize", &mem, &len, NULL, 0) != 0) return -1;
    *avail = ((uint64_t)vs.free_count + (uint64_t)vs.inactive_count) * (uint64_t)page; *total = mem;
    return 0;
}
#else
#include <unistd.h>
int lj_sysmem_query(uint64_t *avail, uint64_t *total)
{
    long pg = sysconf(_SC_PAGESIZE), av = sysconf(_SC_AVPHYS_PAGES), ph = sysconf(_SC_PHYS_PAGES);
    if (pg <= 0 || av < 0 || ph <= 0) return -1;
    *avail = (uint64_t)av * (uint64_t)pg; *total = (uint64_t)ph * (uint64_t)pg;
    return 0;
}
#endif
