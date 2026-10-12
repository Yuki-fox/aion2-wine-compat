/* AION 2 / CrossOver 26.3 Rosetta compatibility fix.
 * Implements the observed x86-64 -> x86-32 CPUID boundary transition.
 * Does not patch game files or suppress the resulting Windows exception.
 * No timers, background threads, log files or process termination. */
#include <signal.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/ucontext.h>
#include <dlfcn.h>
#include <string.h>
static void (*handlers[NSIG])(int, siginfo_t *, void *);
static const unsigned char compat_trampoline[] = {
    0x53,0x55,0x57,0x56,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,
    0x49,0x89,0x20,0x66,0x8c,0xd0,0x50,0x52,0x9c,0x48,0x8b,0x41,0x08,
    0x50,0x48,0x8b,0x01,0x50,0x48,0xcf,0x41,0x5f,0x41,0x5e,0x41,0x5d,
    0x41,0x5c,0x5e,0x5f,0x5d,0x5b,0xc3
};

/* Execute this narrow, verified 64->32 trampoline and CPUID in software.
 * No game instructions or result flags are replaced. The resulting 32-bit
 * EIP wrap to zero is delivered as the original execute access violation. */
static int emulate_compat_cpuid(int sig, siginfo_t *info, ucontext_t *uc)
{
    if (sig != SIGILL || uc->uc_mcontext->__ss.__cs != 0x2b) return 0;
    uint64_t rip=uc->uc_mcontext->__ss.__rip, rsp=uc->uc_mcontext->__ss.__rsp;
    uint64_t rcx=uc->uc_mcontext->__ss.__rcx, r8=uc->uc_mcontext->__ss.__r8;
    uint64_t compat_rsp=uc->uc_mcontext->__ss.__rdx;
    if ((rip&4095)>4096-sizeof(compat_trampoline) || rsp<65536 ||
        rcx<rsp || rcx>rsp+4096 || (rcx&7) || !r8 || (r8&7) ||
        compat_rsp<65536 || compat_rsp>0xffff0000ull) return 0;
    volatile const unsigned char *code=(void *)(uintptr_t)rip;
    for (unsigned i=0;i<sizeof(compat_trampoline);i++) if(code[i]!=compat_trampoline[i])return 0;
    const uint64_t *target=(void *)(uintptr_t)rcx;
    if (target[0]!=0xfffffffeull || target[1]!=0x23) return 0;
    volatile const unsigned char *cpuid=(void *)(uintptr_t)0xfffffffeull;
    if (cpuid[0]!=0x0f || cpuid[1]!=0xa2) return 0;
    uint64_t *stack=(void *)(uintptr_t)rsp;
    stack[-1]=uc->uc_mcontext->__ss.__rbx; stack[-2]=uc->uc_mcontext->__ss.__rbp;
    stack[-3]=uc->uc_mcontext->__ss.__rdi; stack[-4]=uc->uc_mcontext->__ss.__rsi;
    stack[-5]=uc->uc_mcontext->__ss.__r12; stack[-6]=uc->uc_mcontext->__ss.__r13;
    stack[-7]=uc->uc_mcontext->__ss.__r14; stack[-8]=uc->uc_mcontext->__ss.__r15;
    *(uint64_t *)(uintptr_t)r8=rsp-64;
    uint32_t eax=0xfffffffeu, ebx, ecx=(uint32_t)rcx, edx;
    __asm__ volatile("cpuid" : "+a"(eax),"=b"(ebx),"+c"(ecx),"=d"(edx));
    uc->uc_mcontext->__ss.__rax=eax; uc->uc_mcontext->__ss.__rbx=ebx;
    uc->uc_mcontext->__ss.__rcx=ecx; uc->uc_mcontext->__ss.__rdx=edx;
    uc->uc_mcontext->__ss.__rip=0;
    uc->uc_mcontext->__ss.__rsp=(uint32_t)compat_rsp;
    uc->uc_mcontext->__es.__trapno=14;
    uc->uc_mcontext->__es.__err=20;
    uc->uc_mcontext->__es.__faultvaddr=0;
    siginfo_t translated=*info;
    translated.si_signo=SIGSEGV; translated.si_code=SEGV_MAPERR; translated.si_addr=0;
    handlers[SIGSEGV](SIGSEGV,&translated,uc);
    uintptr_t frame=uc->uc_mcontext->__ss.__rsp;
    if (frame<compat_rsp && compat_rsp-frame<=8192 &&
        *(uint64_t *)(frame+0xf8)==0 && *(uint64_t *)(frame+0x98)==compat_rsp)
        *(uint16_t *)(frame+0x38)=0x23;
    return 1;
}

static void probe_handler(int sig, siginfo_t *info, void *context)
{
    if (emulate_compat_cpuid(sig,info,context)) return;
    handlers[sig](sig,info,context);
}

static int probe_sigaction(int sig, const struct sigaction *action, struct sigaction *old)
{
    if (action && (action->sa_flags & SA_SIGINFO) &&
        (sig == SIGSEGV || sig == SIGBUS || sig == SIGILL)) {
        Dl_info image;
        if (dladdr((const void *)action->sa_sigaction, &image) && image.dli_fname &&
            strstr(image.dli_fname, "ntdll.so")) {
            struct sigaction wrapped = *action;
            void (*previous)(int, siginfo_t *, void *) = handlers[sig];
            handlers[sig] = action->sa_sigaction;
            wrapped.sa_sigaction = probe_handler;
            int result = sigaction(sig, &wrapped, old);
            if (result) handlers[sig] = previous;
            if (!result && old && old->sa_sigaction == probe_handler) old->sa_sigaction = previous;
            return result;
        }
    }
    return sigaction(sig, action, old);
}
__attribute__((used, section("__DATA,__interpose")))
static const struct { const void *replacement; const void *original; } interpose = {
    (const void *)probe_sigaction, (const void *)sigaction
};
