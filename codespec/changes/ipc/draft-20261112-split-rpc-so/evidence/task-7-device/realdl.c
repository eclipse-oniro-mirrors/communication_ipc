/* Device-side probe: production lazy-load path of libipc_rpc.z.so and the
 * memory split between an IPC-only process and an RPC-using process.
 *
 * usage: realdl <abs path to libipc_single.z.so> <ipc|rpc>
 *   ipc -> only asks for IF_PROT_DEFAULT (binder); RPC library must stay unmapped
 *   rpc -> asks for IF_PROT_DATABUS; RPC library must be dlopen'ed from the
 *          core library's own directory and the ops result cached
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef void *(*get_remote_invoker_fn)(int);

static long read_pss_kb(void)
{
    FILE *fp = fopen("/proc/self/smaps_rollup", "r");
    char line[256];
    long pss = -1;
    if (fp == NULL) {
        return -1;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "Pss:", 4) == 0) {
            pss = strtol(line + 4, NULL, 10);
            break;
        }
    }
    fclose(fp);
    return pss;
}

static int maps_contains(const char *needle, int print)
{
    FILE *fp = fopen("/proc/self/maps", "r");
    char line[512];
    int hit = 0;
    if (fp == NULL) {
        return -1;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, needle) != NULL) {
            if (print) {
                printf("MAPS_HIT %s", line);
            }
            hit = 1;
        }
    }
    fclose(fp);
    return hit;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("usage: %s <abs path to libipc_single.z.so> <ipc|rpc>\n", argv[0]);
        return 2;
    }
    int rpcMode = strcmp(argv[2], "rpc") == 0;
    long pssBefore = read_pss_kb();
    int rpcBefore = maps_contains("libipc_rpc", 0);
    printf("MODE=%s\n", rpcMode ? "rpc" : "ipc");
    printf("PSS_BEFORE_KB=%ld\n", pssBefore);
    printf("RPC_MAPPED_BEFORE=%d\n", rpcBefore);

    void *core = dlopen(argv[1], RTLD_NOW | RTLD_NODELETE | RTLD_LOCAL);
    if (core == NULL) {
        printf("DLOPEN_CORE_FAIL=%s\n", dlerror());
        return 3;
    }
    get_remote_invoker_fn getRemoteInvoker =
        (get_remote_invoker_fn)dlsym(core, "_ZN4OHOS17IPCThreadSkeleton16GetRemoteInvokerEi");
    if (getRemoteInvoker == NULL) {
        printf("DLSYM_FAIL=%s\n", dlerror());
        return 4;
    }

    /* 0 = IF_PROT_DEFAULT/BINDER, 1 = IF_PROT_DATABUS */
    void *invoker1 = getRemoteInvoker(rpcMode ? 1 : 0);
    void *invoker2 = getRemoteInvoker(rpcMode ? 1 : 0);
    long pssAfter = read_pss_kb();
    int rpcAfter = maps_contains("libipc_rpc", 1);

    printf("INVOKER_FIRST=%p\n", invoker1);
    printf("INVOKER_SECOND=%p\n", invoker2);
    printf("PSS_AFTER_KB=%ld\n", pssAfter);
    printf("PSS_DELTA_KB=%ld\n", (pssBefore >= 0 && pssAfter >= 0) ? (pssAfter - pssBefore) : -1);
    printf("RPC_MAPPED_AFTER=%d\n", rpcAfter);

    int ok;
    if (rpcMode) {
        ok = (invoker1 != NULL) && (invoker1 == invoker2) && (rpcAfter == 1);
    } else {
        /* Binder invoker may be unavailable in the emulator; the split claim is
         * only that asking for the default protocol never maps the RPC image. */
        ok = (rpcAfter == 0);
    }
    printf("RESULT=%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}