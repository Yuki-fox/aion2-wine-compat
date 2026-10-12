#include "../compat_cpuid.c"
#include <stdio.h>
#include <sys/mman.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <assert.h>

static uint64_t expected_rsp;
static int dispatched;
static void fake_wine_handler(int sig, siginfo_t *info, void *context) {
    ucontext_t *uc=context;
    assert(sig==SIGSEGV && info->si_addr==NULL);
    assert(uc->uc_mcontext->__ss.__rip==0);
    assert(uc->uc_mcontext->__ss.__rsp==expected_rsp);
    assert(uc->uc_mcontext->__es.__err==20);
    uintptr_t frame=expected_rsp-0x600;
    memset((void *)frame,0,0x600);
    *(uint64_t *)(frame+0x98)=expected_rsp;
    *(uint64_t *)(frame+0xf8)=0;
    uc->uc_mcontext->__ss.__rsp=frame;
    uc->uc_mcontext->__ss.__rip=0x12345000;
    dispatched++;
}
int main(void) {
    mach_vm_address_t end_page=0xfffff000;
    assert(mach_vm_allocate(mach_task_self(),&end_page,4096,VM_FLAGS_FIXED)==KERN_SUCCESS);
    *(uint16_t *)(uintptr_t)0xfffffffe=0xa20f;
    unsigned char *low=mmap((void*)0x40000000,65536,PROT_READ|PROT_WRITE,MAP_ANON|MAP_PRIVATE,-1,0);
    assert(low!=MAP_FAILED && (uintptr_t)low<0xffff0000);
    expected_rsp=(uintptr_t)low+60000;
    uint64_t original_stack[1024]={0}, saved_rsp=0;
    uint64_t *rsp=original_stack+512, *target=original_stack+600;
    target[0]=0xfffffffe; target[1]=0x23;
    struct __darwin_mcontext64 mc={0}; ucontext_t uc={0};uc.uc_mcontext=&mc;
    mc.__ss.__rip=(uintptr_t)compat_trampoline; mc.__ss.__rsp=(uintptr_t)rsp;
    mc.__ss.__rcx=(uintptr_t)target; mc.__ss.__r8=(uintptr_t)&saved_rsp;
    mc.__ss.__rdx=expected_rsp;mc.__ss.__cs=0x2b;
    mc.__ss.__rbx=101;mc.__ss.__rbp=102;mc.__ss.__rdi=103;mc.__ss.__rsi=104;
    mc.__ss.__r12=105;mc.__ss.__r13=106;mc.__ss.__r14=107;mc.__ss.__r15=108;
    siginfo_t info={0};handlers[SIGSEGV]=fake_wine_handler;
    assert(emulate_compat_cpuid(SIGILL,&info,&uc)==1);
    assert(dispatched==1 && saved_rsp==(uintptr_t)(rsp-8));
    for(int i=1;i<=8;i++)assert(rsp[-i]==(uint64_t)(100+i));
    assert(*(uint16_t *)(uintptr_t)(mc.__ss.__rsp+0x38)==0x23);
    assert(mc.__ss.__rip==0x12345000);
    /* A different signal/context is left untouched. */
    assert(emulate_compat_cpuid(SIGSEGV,&info,&uc)==0 && dispatched==1);
    puts("32-bit CPUID EIP wrap, saved registers, stack and exception context: PASS");
    return 0;
}
