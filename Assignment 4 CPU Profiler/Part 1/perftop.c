#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/limits.h>
#include <linux/sched.h>

atomic_t pre_count = ATOMIC_INIT(0);
atomic_t post_count = ATOMIC_INIT(0);
atomic_t context_switch_count = ATOMIC_INIT(0);

/* private data for each instance */
struct user_data {
 struct task_struct *prev_task;
};
/* pre-event handler */
static int entry_pick_next_fair(struct kretprobe_instance *ri,struct pt_regs *regs){
  struct user_data *task_data;
  struct task_struct *prev=(struct task_struct*)(regs->si);
  task_data=(struct user_data *)ri->data;
  if(task_data!=NULL){
    task_data->prev_task=prev;
  }
  atomic_inc(&pre_count);
  return 0;
}
/* post-event handler*/
static int ret_pick_next_fair(struct kretprobe_instance *ri, struct pt_regs *regs){
  struct user_data *task_data;
  struct task_struct *next_task;
  task_data=(struct user_data *)ri->data;
  next_task= (struct task_struct *)(regs->ax);
  if(task_data != NULL && task_data->prev_task != NULL && next_task != NULL){
    if(next_task != task_data->prev_task){
      atomic_inc(&context_switch_count);
    }
  }
  atomic_inc(&post_count);
  return 0;
}

static int perftop_show(struct seq_file *m, void *v) {
  seq_printf(m, "Hello World\n");
  seq_printf(m, "Pre-count : %d\n",atomic_read(&pre_count));
  seq_printf(m, "Post_count : %d\n",atomic_read(&post_count));
  seq_printf(m, "Context_Switch_Count : %d\n",atomic_read(&context_switch_count));
  return 0;
}

static int perftop_open(struct inode *inode, struct  file *file) {
  return single_open(file, perftop_show, NULL);
}

static const struct proc_ops perftop_fops = {
  .proc_open = perftop_open,
  .proc_read = seq_read,
  .proc_lseek = seq_lseek,
  .proc_release = single_release,
};

static struct kretprobe my_kretprobe = {
	.handler  = ret_pick_next_fair,
	.entry_handler = entry_pick_next_fair,
	.data_size = sizeof(struct user_data),
	.maxactive = 20,
};

static int __init perftop_init(void) {
  int ret;  
  proc_create("perftop", 0, NULL, &perftop_fops);
  my_kretprobe.kp.symbol_name="pick_next_task_fair";

  ret= register_kretprobe(&my_kretprobe);
  if(ret < 0){
    printk(KERN_INFO "register_kretprobe failed,returned %d\n",ret);
    return ret;
  }
  printk(KERN_INFO "Planted Kretprobe at %p\n",my_kretprobe.kp.addr);
  return 0;
}

static void __exit perftop_exit(void) {
  unregister_kretprobe(&my_kretprobe);
  printk(KERN_INFO "kretprobe at %p unregistered\n",my_kretprobe.kp.addr);
  remove_proc_entry("perftop", NULL);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vipul Tiwari <viptiwari@cs.stonybrook.edu>");
MODULE_DESCRIPTION("Kernel module to monitor the pick_next_task_fair of CFS");
module_init(perftop_init);
module_exit(perftop_exit);
