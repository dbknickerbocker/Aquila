#include <core/system.h>
#include <core/string.h>
#include <core/arch.h>

#include <sys/proc.h>
#include <sys/sched.h>

#include <bits/errno.h>

int arch_proc_fork(thread_t *thread, proc_t *fork)
{
    int err = 0;
    x86_proc_t   *pparch = thread->owner->arch;
    x86_thread_t *ptarch = thread->arch;
    x86_proc_t   *fparch = NULL;
    x86_thread_t *ftarch = NULL;

    if (!(fparch = kmalloc(sizeof(x86_proc_t)))) {
        /* Failed to allocate fork process arch structure */
        err = -ENOMEM;
        goto free_resources;
    }

    if (!(ftarch = kmalloc(sizeof(x86_thread_t)))) {
        /* Failed to allocate fork thread arch structure */
        err = -ENOMEM;
        goto free_resources;
    }

    uintptr_t cur_proc_map = pparch->map;
    uintptr_t new_proc_map = arch_get_frame();
    
    if (!new_proc_map) {
        /* Failed to allocate paging structure */
        err = -ENOMEM;
        goto free_resources;
    }

    arch_mm_fork(cur_proc_map, new_proc_map);

    /* Setup kstack */
    uintptr_t fkstack_base = (uintptr_t) kmalloc(KERN_STACK_SIZE);
    ftarch->kstack = fkstack_base + KERN_STACK_SIZE;

    /* Copy registers */
    struct x86_regs *fork_regs = (void *) (ftarch->kstack - (ptarch->kstack - (uintptr_t) ptarch->regs));
    ftarch->regs = fork_regs;

    /* Copy kstack */
    memcpy((void *) fkstack_base, (void *) (ptarch->kstack - KERN_STACK_SIZE), KERN_STACK_SIZE);

    extern void x86_fork_return();
#if ARCH_BITS==32
    ftarch->eip = (uintptr_t) x86_fork_return;
    ftarch->esp = (uintptr_t) fork_regs;
#else
    ftarch->rip = (uintptr_t) x86_fork_return;
    ftarch->rsp = (uintptr_t) fork_regs;
#endif

    fparch->map = new_proc_map;
    fork->arch  = fparch;

    thread_t *fthread = (thread_t *) fork->threads.head->value;
    fthread->arch = ftarch;

    ftarch->fpu_enabled = 0;
    ftarch->fpu_context = NULL;

    return 0;

free_resources:
    if (fparch)
        kfree(fparch);
    if (ftarch)
        kfree(ftarch);
    return err;
}
