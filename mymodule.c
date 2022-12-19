#include <linux/list.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/pid.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>
#include <linux/time.h>

 
 
static int pid = -1;

module_param(pid, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );


void DFS(struct task_struct *task)
{   
    struct task_struct *child;
    struct list_head *list;

    printk("%d %d %llu", task->pid,task->parent->pid,task->start_time);
    
    list_for_each(list, &task->children) {
        child = list_entry(list, struct task_struct, sibling);
        DFS(child);
    }
}


int simple_init(void)
{
    struct pid *pid_struct = find_get_pid(pid);
    struct task_struct *init_task = pid_task(pid_struct, PIDTYPE_PID);

    DFS(init_task);

    return 0;
}


void simple_exit(void)
{

}


module_init(simple_init);
module_exit(simple_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Huseyin & Numan");
MODULE_DESCRIPTION("PID MODULE");

