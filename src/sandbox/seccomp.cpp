#ifdef SANDBOX_SECCOMP

#include "core/Core.hpp" // just for ARRLEN

#include <stdio.h>
#include <stdlib.h>

#include <sys/prctl.h>

#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <sys/ptrace.h>

#include "seccomp-bpf.h"

// our own wrapper for the seccomp() syscall
// TODO: should this be conditional on a feature check or something?
static inline int seccomp(unsigned int operation, unsigned int flags, void *args) {
    return syscall(__NR_seccomp, operation, flags, args);
}

#define DENY_SYSCALL(name) \
    BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, __NR_##name, 0, 1), \
    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_KILL_PROCESS)

#define filter whitelist

static __attribute__((unused)) sock_filter whitelist[] = {
    VALIDATE_ARCHITECTURE,
    EXAMINE_SYSCALL,

    // memory management
    ALLOW_SYSCALL(mmap),
    ALLOW_SYSCALL(munmap),
    ALLOW_SYSCALL(mprotect),
    ALLOW_SYSCALL(madvise),
    ALLOW_SYSCALL(brk),

    // basic file IO
    ALLOW_SYSCALL(open),
    ALLOW_SYSCALL(openat),
    ALLOW_SYSCALL(read),
    ALLOW_SYSCALL(write),
    ALLOW_SYSCALL(close),
    ALLOW_SYSCALL(stat),
    ALLOW_SYSCALL(fstat),
    ALLOW_SYSCALL(fsync), // maybe
    ALLOW_SYSCALL(creat), // maybe; for DB journal
    ALLOW_SYSCALL(unlink), // for DB journal

    // more IO
    ALLOW_SYSCALL(pread64),
    ALLOW_SYSCALL(pwrite64),
    ALLOW_SYSCALL(fdatasync),

    // misc libc things
    ALLOW_SYSCALL(getcwd),
    ALLOW_SYSCALL(getpid),
    ALLOW_SYSCALL(geteuid),
    ALLOW_SYSCALL(fcntl),
    ALLOW_SYSCALL(exit),
    ALLOW_SYSCALL(exit_group),

    // threading
    ALLOW_SYSCALL(futex),

    // networking
    ALLOW_SYSCALL(poll),
    ALLOW_SYSCALL(accept),
    ALLOW_SYSCALL(setsockopt),
    ALLOW_SYSCALL(sendto),
    ALLOW_SYSCALL(recvfrom),
    ALLOW_SYSCALL(shutdown),

    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_KILL_PROCESS)
};

static __attribute__((unused)) sock_filter blacklist[] = {
    VALIDATE_ARCHITECTURE,
    EXAMINE_SYSCALL,

    // examples of undesirable syscalls
    DENY_SYSCALL(execve),
    DENY_SYSCALL(fork),
    DENY_SYSCALL(vfork),
    DENY_SYSCALL(clone),
    DENY_SYSCALL(connect),
    DENY_SYSCALL(listen),
    DENY_SYSCALL(bind),
    DENY_SYSCALL(kill),
    DENY_SYSCALL(settimeofday),
    // etc

    // default-permit mode
    BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_ALLOW)
};

static sock_fprog prog = {
    ARRLEN(filter), filter
};

void sandbox_init() {}

void sandbox_start() {
    printf("[INFO] Starting SECCOMP-BPF sandbox...\n");

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        perror("prctl (privs)");
        exit(1);
    }

    //if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) < 0) {
    if (seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC, &prog) < 0) {
        perror("seccomp");
        exit(1);
    }
}

#endif // SANDBOX_SECCOMP
