#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/limits.h>
#include <linux/sched.h>
#include <linux/hashtable.h>
#include <linux/rbtree.h>
#include <linux/ptrace.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/string.h>

atomic_t pre_count = ATOMIC_INIT(0);
atomic_t post_count = ATOMIC_INIT(0);
atomic_t context_switch_count = ATOMIC_INIT(0);

/* private data for each instance */
struct user_data {
 struct task_struct *prev_task;
};
//Defining node for hash Table

struct k_hash{
int data;
unsigned long long tsc;
struct hlist_node node;
};

//Defining node for rb_tree
struct my_node{
struct rb_node node;
int data;
unsigned long long tsc;
};

//Defining root node for rb tree
struct rb_root mytree = RB_ROOT;

//Defining Hashtable with bits=16
DEFINE_HASHTABLE(my_hashtable,16);

DEFINE_SPINLOCK(lock);
static unsigned long long get_start_tsc_fromhashtable(int id){
struct k_hash *curr;
struct hlist_node *tmp;
int bkt;
int key=id;
hash_for_each_safe(my_hashtable,bkt,tmp,curr,node){
if(curr->data==key)
return curr->tsc;
}
return 0;
}

unsigned long long lookup_rb_tree_for_tsc(struct rb_root *mytree,int pid){
struct rb_node *node;
for(node = rb_first(mytree); node; node = rb_next(node)){
struct my_node *temp=rb_entry(node,struct my_node,node);
if(temp->data == pid){
	return temp->tsc;
}
}
return 0;
}

struct my_node *search_rb_tree_for_entry(struct rb_root *root,int val){
struct rb_node *node;
for(node = rb_first(root); node; node=rb_next(node)){
	struct my_node *curr = rb_entry(node,struct my_node,node);
	if(curr->data == val){
	return curr;
	}
}
return NULL;
}

void remove_from_rb_tree(struct rb_root *root,int pid){
struct my_node *key_node;
key_node=search_rb_tree_for_entry(&mytree,pid);
if(key_node){
	rb_erase(&key_node->node,&mytree);
	kfree(key_node);
}
} 

int insert_node(struct rb_root *root,struct my_node *curr){
struct rb_node *parent=NULL;
struct rb_node **new=&(root->rb_node);
while(*new){
struct my_node *this = container_of(*new,struct my_node,node);
parent=*new;
if(curr->tsc < this->tsc)
new=&((*new)->rb_left);
else if(curr->tsc > this->tsc)
new=&((*new)->rb_right);
else
return -1;
}
rb_link_node(&curr->node,parent,new);
rb_insert_color(&curr->node,root);
return 1;
}

void insert_rb_tree(int pid,unsigned long long tsc){
struct my_node *temp_node=(struct my_node *)kmalloc(sizeof(struct my_node),GFP_ATOMIC);
temp_node->data=pid;
temp_node->tsc=tsc;
insert_node(&mytree,temp_node);
}

static void remove_from_hashtable(int val){
struct k_hash *curr;
struct hlist_node *tmp;
int bkt;
hash_for_each_safe(my_hashtable,bkt,tmp,curr,node){
if(curr->data == val){
hash_del(&curr->node);
kfree(curr);
}
}
}

void delete_hash_table(void){
struct k_hash *curr;
struct hlist_node *tmp;
int bkt;
hash_for_each_safe(my_hashtable,bkt,tmp,curr,node){
curr->data=0;
hash_del(&curr->node);
kfree(curr);
}
printk(KERN_INFO "Hashtable deleted\n");
}

void delete_rb_tree(struct rb_root *root){
struct rb_node *node;
for(node=rb_first(root); node ;node = rb_next(node)){
struct my_node *temp_rbnode=rb_entry(node, struct my_node,node);
rb_erase(&(temp_rbnode->node),&mytree);
kfree(temp_rbnode);
}
printk(KERN_INFO "RB Tree deleted\n");
}

void display_rb_tree(struct seq_file *m){
 struct rb_node *node_rb;
 struct my_node *temp_node;
 int count=0;
  if(!rb_first(&mytree)){
  printk(KERN_INFO "No Rb Tree node\n");
  }
  for(node_rb = rb_last(&mytree);node_rb;node_rb=rb_prev(node_rb)){
	  count++;
	  temp_node=rb_entry(node_rb,struct my_node,node);
	  seq_printf(m,"PID = %d, Time(total tsc) = %llu\n",temp_node->data,temp_node->tsc);
	  if(count>9){
	  break;
	  }
    }
}


static void insert_into_hashtable(int pid,unsigned long long updated_tsc){
struct k_hash *temp_hashnode=kmalloc(sizeof(struct k_hash ),GFP_ATOMIC);
temp_hashnode->data=pid;
temp_hashnode->tsc=updated_tsc;
remove_from_hashtable(pid);
hash_add(my_hashtable,&temp_hashnode->node,pid);
}


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
  unsigned long long total_tsc=0;
  task_data=(struct user_data *)ri->data;
  next_task= (struct task_struct *)(regs->ax);
  if(task_data != NULL && task_data->prev_task != NULL && next_task != NULL){
    if(next_task != task_data->prev_task){
	    
	 int bkt,key;
	 struct k_hash *curr;
	 struct hlist_node *tmp;
	 unsigned long long curr_tsc = rdtsc();
	 unsigned long long start_tsc=get_start_tsc_fromhashtable(task_data->prev_task->pid);
	 spin_lock(&lock);
	 if(start_tsc != 0){
	 //Deleting the prev_task from the hashtable
	 key = task_data->prev_task->pid;
	 hash_for_each_safe(my_hashtable,bkt,tmp,curr,node){
	 if(curr->data==key){
	 curr->data=0;
	 hash_del(&curr->node);
	 kfree(curr);
	 }
	 }	
	 total_tsc=curr_tsc - start_tsc;
         total_tsc =total_tsc + lookup_rb_tree_for_tsc(&mytree,task_data->prev_task->pid);
	 //Removing the earlier value of taskpid from rb_tree
	 remove_from_rb_tree(&mytree,task_data->prev_task->pid);
	 insert_rb_tree(task_data->prev_task->pid,total_tsc);
	 }
	insert_into_hashtable(next_task->pid,curr_tsc);
	spin_unlock(&lock);	
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
  spin_lock(&lock);
  display_rb_tree(m);
  spin_unlock(&lock);
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
  delete_hash_table();
  delete_rb_tree(&mytree);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vipul Tiwari <viptiwari@cs.stonybrook.edu>");
MODULE_DESCRIPTION("Kernel module to monitor the pick_next_task_fair of CFS");
module_init(perftop_init);
module_exit(perftop_exit);
