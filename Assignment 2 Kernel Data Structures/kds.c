#include <linux/slab.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/rbtree.h>
#include <linux/hashtable.h>
#include <linux/radix-tree.h>
#include <linux/xarray.h>
#include <linux/bitmap.h>
#include <linux/moduleparam.h>

MODULE_LICENSE ("GPL");
MODULE_AUTHOR("Vipul Tiwari <viptiwari@cs.stonybrook.edu>");
MODULE_DESCRIPTION("Kernel module for frequently used data structures");

//Defining node for rb tree
struct my_node{
	struct rb_node node;
	int data;
};

//Defining root node for rb tree
struct rb_root mytree = RB_ROOT;

//Defining Hashtable with bits=14
DEFINE_HASHTABLE(my_hashtable,14);

//Defining Bitmap with bit 1024
DECLARE_BITMAP(my_bitmap,1024);

//Registering parameter of the module
static char* int_str;
module_param(int_str,charp,S_IRUSR | S_IWUSR);

void display(char *string_copy){
	char *tokens;
	long res;
	while((tokens=strsep(&string_copy," ")) != NULL ) {
		if(kstrtol(tokens,10,&res) == 0){
			printk(KERN_INFO "tokens are : %d\n",(int)res);
		}
	}
}

//Display List
void displayList(char *buffer){
	char *tokens;
	long ldata;
	struct k_list{
		struct list_head list;
		int data;
	};
	
	struct k_list *list_node,*entry,*next;
	struct k_list mylist;

	//Creating the linked list
	INIT_LIST_HEAD(&mylist.list);
	printk(KERN_INFO "CREATE LINKED LIST\n");
	while((tokens=strsep(&buffer," ")) != NULL){
		if(kstrtol(tokens,10,&ldata)==0){
			printk(KERN_INFO "Inserting data in Linked List : %d\n",(int)ldata);
			list_node=kmalloc(sizeof(struct k_list),GFP_KERNEL);
			list_node->data=(int)ldata;
			list_add(&(list_node->list),&(mylist.list));
		}
	}

	//Iterating the Linked List
	printk(KERN_INFO "ITERATE THE LINKED LIST\n");
	list_for_each_entry_reverse(entry,&mylist.list,list){
		printk(KERN_INFO "Elements of the List : %d\n",entry->data);
	}

	//Deleting the items from the list
	printk(KERN_INFO "DELETION IN LINKED LIST");
	list_for_each_entry_safe(entry,next,&mylist.list,list){
		printk(KERN_INFO "Deleting the data from the list : %d\n",entry->data);
		list_del(&entry->list);
	}

}

// Helper function to insert data into RB TREE
int insert_node(struct rb_root *root,struct my_node *curr){
	struct rb_node *parent=NULL;
	struct rb_node **new=&(root->rb_node);
	while(*new){
		struct my_node *this = container_of(*new,struct my_node,node);

		parent=*new;
		if(curr->data < this->data)
			new=&((*new)->rb_left);
		else if(curr->data > this->data)
			new=&((*new)->rb_right);
		else
			return -1;
	}
	rb_link_node(&curr->node,parent,new);
	rb_insert_color(&curr->node,root);
	return 1;
}

//Helper function to search node by key in RB TREE
struct my_node *search_node(struct rb_root *root,int val){
	struct rb_node *node=root->rb_node;
	while(node){
		struct my_node *curr=container_of(node,struct my_node,node);
		if(val < curr->data)
			node=node->rb_left;
		else if(val > curr->data)
			node=node->rb_right;
		else
			return curr;
	}
	return NULL;
}

//Helper function to free node
void free_node(struct my_node *node){
	if(node!=NULL){
		if(node->data){
			node->data=0;
		}
		kfree(node);
		node=NULL;
	}
} 

//Displaying RB TREE
void display_rbtree(char *buffer){
	char *tokens,*lookup_val,*lookup_string,*delete_val,*delete_string;
	int len;
	long ldata;

	struct my_node *temp,*key_node;
	for(len=0;buffer[len]!='\0';len++);
	lookup_string=kmalloc(len+1,GFP_KERNEL);
	strscpy(lookup_string,buffer,len+1);
	delete_string=kmalloc(len+1,GFP_KERNEL);
	strscpy(delete_string,buffer,len+1);

       //Creating a Red Black Tree
	printk(KERN_INFO "CREATING A RBTREE\n");
	while((tokens=strsep(&buffer," ")) != NULL ){
		if(kstrtol(tokens,10,&ldata)==0){
			temp=(struct my_node *)kmalloc(sizeof(struct my_node),GFP_KERNEL);
			temp->data=(int)ldata;
			printk(KERN_INFO "Inserting data into rbtree : %d\n",(int)ldata);
			insert_node(&mytree,temp);
		}
	}

	//Iterating Red Black Tree
	printk(KERN_INFO "LOOKUP IN A RB TREE\n");
	ldata=0;
	while((lookup_val=strsep(&lookup_string," ")) != NULL){
		if(kstrtol(lookup_val,10,&ldata)==0){
			struct my_node *curr=search_node(&mytree,(int)ldata);
			if(curr){
				printk(KERN_INFO "Key : %d\n",curr->data);
			}
		}
	}

	//Deleting the items from Rb tree
	printk(KERN_INFO "DELETION IN A RB TREE\n");
	ldata=0;
	while((delete_val=strsep(&delete_string," ")) != NULL){
		if(kstrtol(delete_val,10,&ldata)==0){
			printk(KERN_INFO "Deleting the data from RB Tree : %d\n",(int)ldata);
			key_node=search_node(&mytree,(int)ldata);
			if(key_node){
				rb_erase(&key_node->node,&mytree);
				free_node(key_node);
			}
		}
	}

	//freeing up the allocated strings
	kfree(lookup_string);
	kfree(delete_string);

}

// Displaying HashTable
void display_hash(char *buffer){
	char *tokens,*lookup_val,*lookup_string;
	long ldata;
	int len,key;
	unsigned bkt;
	struct k_table{
		int data;
		struct hlist_node node;
	};
	struct k_table *curr;
	struct k_table *hashnode;

	//INITIALISING HASH TABLE
	hash_init(my_hashtable);

	for(len=0;buffer[len]!='\0';len++);
	lookup_string=kmalloc(len+1,GFP_KERNEL);
	strscpy(lookup_string,buffer,len+1);
	

	//Insertion in a HashTAble
	printk(KERN_INFO "INSERTION IN HASHTABLE\n");
	while((tokens=strsep(&buffer," ")) != NULL){
		//printk("tokens %s",tokens);
		if(kstrtol(tokens,10,&ldata)==0){
			hashnode=kmalloc(sizeof(struct k_table ),GFP_KERNEL);
			hashnode->data=(int)ldata;
			key=(int)ldata;
			hash_add(my_hashtable,&hashnode->node,key);
			printk(KERN_INFO "Inserting the data in HashTable : %d\n",(int)ldata);
		}
	}

	//Iterate the hash table and print the inserted numbers
	printk(KERN_INFO "ITERATE THE HASHTABLE");
	hash_for_each(my_hashtable,bkt,curr,node){
		printk(KERN_INFO "data present in the HashTable : %d\n", curr->data);
	}

	//look up the inserted numbers and print them out
	ldata=0;
	key=0;
	printk(KERN_INFO "LOOK UP IN A HASHTABLE\n");
	while((lookup_val=strsep(&lookup_string," ")) != NULL){
		printk(KERN_INFO "lookupval %s\n",lookup_val);
		if(kstrtol(lookup_val,10,&ldata)==0){
			key=(int)ldata;
			hash_for_each_possible(my_hashtable,curr,node,key){
				printk(KERN_INFO "looking up the value of Key : %d\n",curr->data);
			}
		}
	}

	//Deleting inserted numbers from hash table
	printk(KERN_INFO "DELETION IN A HASHTABLE\n");
	hash_for_each(my_hashtable,bkt,curr,node){
		printk(KERN_INFO "Deleting the data from Hashtable : %d\n",curr->data);
		curr->data=0;
		hash_del(&curr->node);
		kfree(curr);
	}

	//freeing up the allocated string
	kfree(lookup_string);
	

}


void display_radixtree(char *buffer){
	char *tokens,*lookup_val,*lookup_string;
	int len,k,token_count;
	long ldata;
	void *curr;
	void **result,**slot;
	int i,key_to_delete,return_val;
	long key;
	int *temp;
	//Creating a Radix tree 
	struct radix_tree_iter iter;
	struct radix_tree_root my_radtree;
	INIT_RADIX_TREE(&my_radtree,GFP_KERNEL);

	for(len=0;buffer[len]!='\0';len++);
	lookup_string=kmalloc(len+1,GFP_KERNEL);
	strscpy(lookup_string,buffer,len+1);
	
	token_count=0;
	//Inserting data in radixtree
	printk(KERN_INFO "INSERTION IN RADIX TREE\n");
	while((tokens=strsep(&buffer," ")) != NULL ){
		if(kstrtol(tokens,10,&ldata)==0){
			key=ldata;
			temp=(int *)kmalloc(sizeof(int),GFP_KERNEL);
			*temp=key;
			radix_tree_preload(GFP_KERNEL);
			return_val=radix_tree_insert(&my_radtree,key,temp);
			if(return_val!=0)
				printk(KERN_INFO "The data already exists in radixtree : %d\n",(int)ldata);
			else
				printk(KERN_INFO "Inserting the data in radixtree : %d\n",(int)ldata);
			radix_tree_preload_end();
		}
		token_count++;
	}

	//Lookup the data and set tag in radixtree and print them out
	ldata=0;
	printk(KERN_INFO "LOOKUP AND SETTING TAG IN RADIX TREE\n");
	while((lookup_val=strsep(&lookup_string," ")) != NULL){
		if(kstrtol(lookup_val,10,&ldata)==0){
			if((int)ldata % 2 > 0){
				radix_tree_tag_set(&my_radtree,ldata,1);
			}
			curr=radix_tree_lookup(&my_radtree,ldata);
			if(curr){
				printk(KERN_INFO "Looking up the value of Key : %ld\n",*(long *)curr);
			}
		}
	}

	//Retrieving the tagged numbers and printing them out
	printk(KERN_INFO "PRINTING THE TAGGED NUMBERS IN RADIX TREE\n");
	result = kmalloc(token_count * sizeof(void *), GFP_KERNEL);
	k=radix_tree_gang_lookup_tag(&my_radtree,result,1,token_count,1);


	for(i=0;i<k;i++){
		printk(KERN_INFO "Key tagged are : %d\n",**(int **)result);
		result++;
	}

	
	//Deleting elements from radix tree
	printk(KERN_INFO "DELETION IN A RADIX TREE\n");
	radix_tree_for_each_slot(slot,&my_radtree,&iter,0){
		key_to_delete=**(int **)slot;
		printk(KERN_INFO "Deleting data in Radix tree : %d\n",key_to_delete);
		radix_tree_delete(&my_radtree,key_to_delete);
	}

	//freeing up the allocated item
	kfree(lookup_string);
}

void display_xarray(char *buffer){
	char *tokens,*tag_val,*tag_string;
	int len,token_count;
	long ldata;
	xa_mark_t mark;
	int key,key_to_delete;
	void *entry,*container;
	int *item;
	unsigned long index;
	
	//creating xarray
	struct xarray my_xarray;
	xa_init(&my_xarray);
	

	for(len=0;buffer[len]!='\0';len++);
	tag_string=kmalloc(len+1,GFP_KERNEL);
	strscpy(tag_string,buffer,len+1);
	//delete_string=kmalloc(len+1,GFP_KERNEL);
	//strscpy(delete_string,buffer,len+1);
	token_count=0;

	//Inserting elements in XARRAY
	printk(KERN_INFO "INSERTION IN XARRAY\n");
	while((tokens=strsep(&buffer," ")) != NULL ){
		if(kstrtol(tokens,10,&ldata)==0){
			key=(int)ldata;
			item=(int *)kmalloc(sizeof(int),GFP_KERNEL);
			*item=key;
			printk(KERN_INFO "Inserting data in xarray : %d\n",key);
			xa_store(&my_xarray,key,item,GFP_KERNEL);
		}
		token_count++;
	}

	//look up the inserted numbers and print them out
	index=0;
	printk(KERN_INFO "LOOKUP IN XARRAY\n");
	xa_for_each(&my_xarray,index,entry){
		printk(KERN_INFO "Looking up the value of key in xarray :  %d\n",*(int *)entry);
	}
       
	//Tag odd numbers in radix tree
	ldata=0;
	mark=0;
	while((tag_val=strsep(&tag_string," ")) != NULL ){
		
		if(kstrtol(tag_val,10,&ldata)==0){
			key=(int)ldata;
			if((int)ldata % 2 > 0){
				xa_set_mark(&my_xarray,ldata,mark);
			}
		}
	}
	
	//print the marked numbers
	index=0;
	printk(KERN_INFO "MARKED NUMBERS IN XARRAY\n");
	xa_for_each_marked(&my_xarray,index,entry,mark){
		printk(KERN_INFO "Marked data in xarray : %d\n",*(int *)entry);
	}
      
        //Deleting the data from xarray
	ldata=0;
	index=0;
	printk(KERN_INFO "DELETION IN XARRAY\n");
	xa_for_each(&my_xarray,index,entry){
		key_to_delete=*(int *)entry;
		printk(KERN_INFO "Deleting data from Xarray : %d\n",key_to_delete);
		container=xa_erase(&my_xarray,key_to_delete);
		kfree(container);
	}
	//freeing up the allocated items
	kfree(tag_string);

}

void display_bitmap(char *buffer){
	int bits;
	char *tokens;
	int token_count;
	long ldata;
	int bit;
	bits=1024;

	//Inserting bits in bitmap
	printk(KERN_INFO "INSERTION IN BITMAP\n");
	while((tokens=strsep(&buffer," ")) != NULL ){
		
		if(kstrtol(tokens,10,&ldata)==0){
			set_bit((int)ldata,(volatile unsigned long *)&my_bitmap);
			printk(KERN_INFO "Inserted bit in bit_map : %ld\n",ldata);
		}
		token_count++;
	}
        
	//Print the set bits
	printk(KERN_INFO "PRINTING SET BITS IN BITMAP\n");
	for_each_set_bit(bit,(unsigned long *)&my_bitmap,1024){
		printk("The set bits are : %d\n",bit);
	}

	//clear the bits
	printk(KERN_INFO "CLEARING THE BITS IN BITMAP\n");
	for_each_set_bit(bit,(unsigned long *)&my_bitmap,1024){
		printk("Clearing the bit : %d\n",bit);
		clear_bit(bit,(volatile unsigned long *)&my_bitmap);
	}

	

}

static int __init kds_init(void){
	int str_len;
	char *string_temp,*string_copy,*string_rb,*string_radix,*string_xarray,*string_bmap;
	char *string_hash;
	for(str_len=0;int_str[str_len]!='\0';str_len++);    
	printk(KERN_INFO "Module loaded ....\n");
	printk("The string passed to module is : %s", int_str);
	string_copy=kmalloc(str_len+1,GFP_KERNEL);
	strscpy(string_copy,int_str,str_len+1);
	printk(KERN_INFO "--------------Printing the Tokens of int_str-------------");
	display(string_copy);
	kfree(string_copy);
	printk(KERN_INFO "-----------------------Linked List-----------------------");
	string_temp=kmalloc(str_len+1,GFP_KERNEL);
	strscpy(string_temp,int_str,str_len+1);
	displayList(string_temp);
	kfree(string_temp);
	printk(KERN_INFO "-----------------------Red Black Trees--------------------");
	string_rb=kmalloc(str_len+1,GFP_KERNEL);
	strscpy(string_rb,int_str,str_len+1);
	display_rbtree(string_rb);
	kfree(string_rb);
	printk(KERN_INFO "-----------------------Hash Table-------------------------");
	string_hash=kmalloc(str_len+1,GFP_KERNEL);
	strscpy(string_hash,int_str,str_len+1);
	display_hash(string_hash);
	kfree(string_hash);
	printk(KERN_INFO "------------------------Radix Tree-------------------------");
	string_radix=kmalloc(str_len+1,GFP_KERNEL);
	strscpy(string_radix,int_str,str_len+1);
	display_radixtree(string_radix);
	kfree(string_radix);
	printk(KERN_INFO "----------------------XArray-------------------------------");
	string_xarray=kmalloc(str_len+1,GFP_KERNEL);
	strscpy(string_xarray,int_str,str_len+1);
	display_xarray(string_xarray);
	kfree(string_xarray);
	printk(KERN_INFO "---------------------Bit Map-------------------------------");
	string_bmap=kmalloc(str_len+1,GFP_KERNEL);
	strscpy(string_bmap,int_str,str_len+1);
	display_bitmap(string_bmap);
	kfree(string_bmap);
	return 0;
}
static void __exit kds_exit(void){
	printk(KERN_INFO "Module exiting ...\n");
}
module_init(kds_init);
module_exit(kds_exit);
