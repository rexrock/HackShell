/*
 * * author：liyang
 * * encode: utf-8
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/namei.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <net/sock.h>
#include <net/netlink.h>
#include <net/net_namespace.h>
#include <linux/string.h>
#include "nl_hack.h"

u32 dst_pid = 0; /* 用户进程的PID */
struct sock *nl_sk = NULL;
static struct kprobe kp =
{
  .symbol_name = "sys_execve", /* 要劫持的系统调用名称 */
};

/* 用于释放getname返回的内存，编译的时候提示符号未定义，只好拷贝到这里了*/
void final_putname(struct filename *name)
{
    if (name->separate) {
        __putname(name->name);
        kfree(name);
    } else {
        __putname(name);
    }
}

/* 用于调试， 寄存器打印 */
void printk_address(unsigned long address, int reliable)
{
    pr_cont(" [<%p>] %s%pB\n",
        (void *)address, reliable ? "" : "? ", (void *)address);
}

/* 用于调试， 寄存器打印*/
void __show_regs(struct pt_regs *regs, int all)
{
    unsigned long cr0 = 0L, cr2 = 0L, cr3 = 0L, cr4 = 0L, fs, gs, shadowgs;
    unsigned long d0, d1, d2, d3, d6, d7;
    unsigned int fsindex, gsindex;
    unsigned int ds, cs, es;

    printk(KERN_DEFAULT "RIP: %04lx:[<%016lx>] ", regs->cs & 0xffff, regs->ip);
    printk_address(regs->ip, 1);
    printk(KERN_DEFAULT "RSP: %04lx:%016lx  EFLAGS: %08lx\n", regs->ss,
            regs->sp, regs->flags);
    printk(KERN_DEFAULT "RAX: %016lx RBX: %016lx RCX: %016lx\n",
           regs->ax, regs->bx, regs->cx);
    printk(KERN_DEFAULT "RDX: %016lx RSI: %016lx RDI: %016lx\n",
           regs->dx, regs->si, regs->di);
    printk(KERN_DEFAULT "RBP: %016lx R08: %016lx R09: %016lx\n",
           regs->bp, regs->r8, regs->r9);
    printk(KERN_DEFAULT "R10: %016lx R11: %016lx R12: %016lx\n",
           regs->r10, regs->r11, regs->r12);
    printk(KERN_DEFAULT "R13: %016lx R14: %016lx R15: %016lx\n",
           regs->r13, regs->r14, regs->r15);

    asm("movl %%ds,%0" : "=r" (ds));
    asm("movl %%cs,%0" : "=r" (cs));
    asm("movl %%es,%0" : "=r" (es));
    asm("movl %%fs,%0" : "=r" (fsindex));
    asm("movl %%gs,%0" : "=r" (gsindex));

    rdmsrl(MSR_FS_BASE, fs);
    rdmsrl(MSR_GS_BASE, gs);
    rdmsrl(MSR_KERNEL_GS_BASE, shadowgs);

    if (!all)
        return;

    cr0 = read_cr0();
    cr2 = read_cr2();
    cr3 = read_cr3();
    cr4 = read_cr4();

    printk(KERN_DEFAULT "FS:  %016lx(%04x) GS:%016lx(%04x) knlGS:%016lx\n",
           fs, fsindex, gs, gsindex, shadowgs);
    printk(KERN_DEFAULT "CS:  %04x DS: %04x ES: %04x CR0: %016lx\n", cs, ds,
            es, cr0);
    printk(KERN_DEFAULT "CR2: %016lx CR3: %016lx CR4: %016lx\n", cr2, cr3,
            cr4);

    get_debugreg(d0, 0);
    get_debugreg(d1, 1);
    get_debugreg(d2, 2);
    printk(KERN_DEFAULT "DR0: %016lx DR1: %016lx DR2: %016lx\n", d0, d1, d2);
    get_debugreg(d3, 3);
    get_debugreg(d6, 6);
    get_debugreg(d7, 7);
    printk(KERN_DEFAULT "DR3: %016lx DR6: %016lx DR7: %016lx\n", d3, d6, d7);
}

/* 处理用户进程发来的消息，主要是提取用户进程的PID，目前只支持一个用户进程的通信 */
void netlink_recv_hack(struct sk_buff *__skb)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    skb = skb_get(__skb);
    if(skb && NLMSG_SPACE(0) <= skb->len)
    {
        nlh = nlmsg_hdr(skb);
        if (nlh && NLMSG_TYPE_HACK_READY == nlh->nlmsg_type) {
            /* 只从指定类型的消息中提取pid */
            dst_pid = nlh->nlmsg_pid;
            printk(KERN_INFO "dst_pid is %d\n", dst_pid);
        }
        kfree_skb(skb);
    }
}

/* 将截取到的命令发送给用户进程 */
void netlink_send_hack(const struct filename * fname)
{
    if (dst_pid)
    {
        struct sk_buff *skb;
        struct nlmsghdr *nlh;
        size_t slen = strnlen(fname->name, NLMSG_SPACE(NLMSG_SIZE));

        skb = alloc_skb(NLMSG_SPACE(NLMSG_SIZE), GFP_KERNEL);
        if(!skb)
        {
            printk(KERN_ERR "alloc skb failed\n");
            return;
        }

        nlh = nlmsg_put(skb, 0, 0, 0, NLMSG_SPACE(NLMSG_SIZE), 0);
        nlh->nlmsg_type = NLMSG_TYPE_HACK_EXECVE;

        NETLINK_CB(skb).portid = dst_pid;
        NETLINK_CB(skb).dst_group = 0;

        memcpy(NLMSG_DATA(nlh), fname->name, slen + 1);
        netlink_unicast(nl_sk, skb, dst_pid, MSG_DONTWAIT);
    }
}

/* 注册在指令执行之前的探测点 */
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    int error;
    struct filename *fname;

    fname = getname((char __user *) regs->bx);
    error = PTR_ERR(fname);
    if (!IS_ERR(fname))
    {
        printk(KERN_INFO "file to execve: %s\n", fname->name);
        netlink_send_hack(fname);
        final_putname(fname);
    }
    
    return 0;
}

/* 注册在指令执行之后的探测点 */
static void handler_post(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{  
}

/* 内存访问错误的时候不需要处理 */
static int handler_fault(struct kprobe *p, struct pt_regs *regs, int trapnr)
{
    return 0;
}

static int __init kprobe_init(void)
{
    int ret;
    /* 注册netlink套接字 */
    struct netlink_kernel_cfg cfg = {
        .input      = netlink_recv_hack,
    };
    nl_sk = netlink_kernel_create(&init_net, NETLINK_HACK_EXECVE, &cfg);
    if(!nl_sk)
    {
        printk(KERN_ERR "create netlink kernel socket failed\n");
        return -ENOMEM;
    }

    kp.pre_handler = handler_pre;
    kp.post_handler = handler_post;
    kp.fault_handler = handler_fault;

    ret = register_kprobe(&kp);
    if (ret < 0) 
    {
        printk(KERN_INFO "register_kprobe failed, returned %d\n", ret);
        return ret;
    }
    printk(KERN_INFO "Planted kprobe at %p\n", kp.addr);
    return 0;
}

static void __exit kprobe_exit(void)
{
    netlink_kernel_release(nl_sk);
    unregister_kprobe(&kp);
    printk(KERN_INFO "kprobe at %p unregistered\n", kp.addr);
}

module_init(kprobe_init)
module_exit(kprobe_exit)
MODULE_LICENSE("GPL");
