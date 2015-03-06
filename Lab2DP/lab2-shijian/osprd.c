#include <linux/version.h>
#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>  /* printk() */
#include <linux/errno.h>   /* error codes */
#include <linux/types.h>   /* size_t */
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>  /* struct request */
#include <linux/wait.h>
#include <linux/file.h>
#include <linux/string.h>
// #include <asm/uaccess.h>

#include "spinlock.h"
#include "osprd.h"

/* The size of an OSPRD sector. */
#define SECTOR_SIZE	512

/* This flag is added to an OSPRD file's f_flags to indicate that the file
 * is locked. */
#define F_OSPRD_LOCKED	0x80000

/* eprintk() prints messages to the console.
 * (If working on a real Linux machine, change KERN_NOTICE to KERN_ALERT or
 * KERN_EMERG so that you are sure to see the messages.  By default, the
 * kernel does not print all messages to the console.  Levels like KERN_ALERT
 * and KERN_EMERG will make sure that you will see messages.) */
#define eprintk(format, ...) printk(KERN_NOTICE format, ## __VA_ARGS__)

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("CS 111 RAM Disk");
// EXERCISE: Pass your names into the kernel as the module's authors.
MODULE_AUTHOR("Shijian ZHENG");

#define OSPRD_MAJOR	222

/* This module parameter controls how big the disk will be.
 * You can specify module parameters when you load the module,
 * as an argument to insmod: "insmod osprd.ko nsectors=4096" */
static int nsectors = 32;
module_param(nsectors, int, 0);


typedef struct list_node
{
	int num;
	int begin;
	int end;
	int notified;
	struct list_node *next;
} list_node_t;

typedef struct num_list
{
	list_node_t *head;
	list_node_t *tail;
	int size;
} num_list_t;

void add_to_list(num_list_t *list, int number)
{
	if (list->tail == NULL)
	{
		list->tail = kmalloc(sizeof(list_node_t *), GFP_ATOMIC);
		list->tail->num = number;
		list->tail->next = NULL;
		list->head = list->tail;
		list->size++;
	}
	else
	{
		list->tail->next = kmalloc(sizeof(list_node_t *), GFP_ATOMIC);
		list->tail = list->tail->next;
		list->tail->num = number;
		list->tail->next = NULL;
		list->size++;
	}
}

void add_to_list_N(num_list_t *list, int number, int byte_begin, int byte_end)
{
	if (list->tail == NULL)
	{
		list->tail = kmalloc(sizeof(list_node_t *), GFP_ATOMIC);
		list->tail->num = number;
		list->tail->begin = byte_begin;
		list->tail->end = byte_end;
		list->tail->notified = 0;
		list->tail->next = NULL;
		list->head = list->tail;
		list->size++;
	}
	else
	{
		list->tail->next = kmalloc(sizeof(list_node_t *), GFP_ATOMIC);
		list->tail = list->tail->next;
		list->tail->num = number;
		list->tail->next = NULL;
		list->size++;
	}
}

void print_list(num_list_t *list)
{
	list_node_t *curr = list->head;
	if (curr != NULL)
	{
		eprintk("pid[%d] ", curr->num);
		curr = curr->next;
	}
	while (curr != NULL)
	{
		eprintk("-> pid[%d] ", curr->num);
		curr = curr->next;
	}
	eprintk("Size: %d\n", list->size);
	
}

void remove_from_list(num_list_t *list, int number)
{
	if (list->head->num == number)
	{
		list_node_t * temp = list->head;
		list->head = list->head->next;
		if (list->head == NULL) 
		{
			list->tail = NULL;
		}
		list->size--;
		kfree(temp);
	}
	else
	{
		list_node_t *prev = list->head;
		list_node_t *curr = prev->next;
		while(curr != NULL)
		{
			if (curr->num == number)
			{
				prev->next = curr->next;
				if (prev->next == NULL)
				{
					list->tail = prev;
				}
				list->size--;
				kfree(curr);
				break;
			}
			prev = prev->next;
			curr = curr->next;
		}
	}
}

void clear_list(num_list_t *list)
{
	list_node_t *prev = list->head;
	list_node_t *curr;
	while(prev != NULL)
	{
		curr = prev->next;
		kfree(prev);
		prev = curr;
	}
	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
}

int next_valid_ticket(num_list_t *invalid_list, int ticket)
{
	int valid_ticket = ticket + 1;
	list_node_t *curr = invalid_list->head;
	while (curr != NULL)
	{
		if (curr->num == valid_ticket)
		{
			valid_ticket++;
			curr = invalid_list->head;
			remove_from_list(invalid_list, curr->num);
		}
		else
			curr = curr->next;
	}
	return valid_ticket;
}

int is_notified(num_list_t *wait_list, int pid)
{
	list_node_t *curr = wait_list->head;
	while (curr != NULL)
	{
		if (curr->num == pid)
		{
			return curr->notified;
		}
		else
			curr = curr->next;
	}
	return 0;
}

void notify_process(num_list_t *wait_list, int write_begin, int write_end)
{
	list_node_t *curr = wait_list->head;
	while (curr != NULL)
	{
		if (curr->begin <= write_end && curr->end >= write_begin)
		{
			curr->notified = 1;
		}
		curr = curr->next;
	}
}


/* The internal representation of our device. */
typedef struct osprd_info {
	uint8_t *data;                  // The data array. Its size is
	                                // (nsectors * SECTOR_SIZE) bytes.

	osp_spinlock_t mutex;           // Mutex for synchronizing access to
					// this block device

	unsigned ticket_head;		// Currently running ticket for
					// the device lock

	unsigned ticket_tail;		// Next available ticket for
					// the device lock

	wait_queue_head_t blockq;       // Wait queue for tasks blocked on
					// the device lock

	/* HINT: You may want to add additional fields to help
	         in detecting deadlock. */
	num_list_t *write_locking_pids;
	num_list_t *read_locking_pids;
	num_list_t *invalid_ticket;

	//Variables for notification;
	wait_queue_head_t blockq_N;     // Wait queue for tasks blocked until being notified;
	num_list_t *wait_notif_pids;
	unsigned ticket_head_N;
	unsigned ticket_tail_N;
	num_list_t *invalid_ticket_N;

	// The following elements are used internally; you don't need
	// to understand them.
	struct request_queue *queue;    // The device request queue.
	spinlock_t qlock;		// Used internally for mutual
	                                //   exclusion in the 'queue'.
	struct gendisk *gd;             // The generic disk.
} osprd_info_t;

#define NOSPRD 4
static osprd_info_t osprds[NOSPRD];


// Declare useful helper functions

/*
 * file2osprd(filp)
 *   Given an open file, check whether that file corresponds to an OSP ramdisk.
 *   If so, return a pointer to the ramdisk's osprd_info_t.
 *   If not, return NULL.
 */
static osprd_info_t *file2osprd(struct file *filp);

/*
 * for_each_open_file(task, callback, user_data)
 *   Given a task, call the function 'callback' once for each of 'task's open
 *   files.  'callback' is called as 'callback(filp, user_data)'; 'filp' is
 *   the open file, and 'user_data' is copied from for_each_open_file's third
 *   argument.
 */
static void for_each_open_file(struct task_struct *task,
			       void (*callback)(struct file *filp,
						osprd_info_t *user_data),
			       osprd_info_t *user_data);


/*
 * osprd_process_request(d, req)
 *   Called when the user reads or writes a sector.
 *   Should perform the read or write, as appropriate.
 */
static void osprd_process_request(osprd_info_t *d, struct request *req)
{
	if (!blk_fs_request(req)) {
		end_request(req, 0);
		return;
	}

	// EXERCISE: Perform the read or write request by copying data between
	// our data array and the request's buffer.
	// Hint: The 'struct request' argument tells you what kind of request
	// this is, and which sectors are being read or written.
	// Read about 'struct request' in <linux/blkdev.h>.
	// Consider the 'req->sector', 'req->current_nr_sectors', and
	// 'req->buffer' members, and the rq_data_dir() function.

	// Recognize the type of request: READ or WRITE;
	unsigned int requestType = rq_data_dir(req);
	uint8_t *data_ptr = d->data + (req->sector) * SECTOR_SIZE;

	if (requestType == READ)
	{
		memcpy((void *)req->buffer, (void *)data_ptr, req->current_nr_sectors * SECTOR_SIZE);
	}
	else if (requestType == WRITE)
	{
		memcpy((void *)data_ptr, (void *)req->buffer, req->current_nr_sectors * SECTOR_SIZE);
	}

	end_request(req, 1);
}


// This function is called when a /dev/osprdX file is opened.
// You aren't likely to need to change this.
static int osprd_open(struct inode *inode, struct file *filp)
{
	// Always set the O_SYNC flag. That way, we will get writes immediately
	// instead of waiting for them to get through write-back caches.
	filp->f_flags |= O_SYNC;
	return 0;
}


// This function is called when a /dev/osprdX file is finally closed.
// (If the file descriptor was dup2ed, this function is called only when the
// last copy is closed.)
static int osprd_close_last(struct inode *inode, struct file *filp)
{
	if (filp) {
		osprd_info_t *d = file2osprd(filp);
		int filp_writable = filp->f_mode & FMODE_WRITE;

		// EXERCISE: If the user closes a ramdisk file that holds
		// a lock, release the lock.  Also wake up blocked processes
		// as appropriate.

		// Your code here.
		osp_spin_lock(&(d->mutex));
		// When the file is closed;
		if ((filp->f_flags & F_OSPRD_LOCKED) != 0)
		{
			if (filp_writable)
			{
				remove_from_list(d->write_locking_pids, current->pid);
				// eprintk("Write done!\n");
				// print_list(d->write_locking_pids);
			}
			else
			{
				remove_from_list(d->read_locking_pids, current->pid);
				// eprintk("Read done!\n");
				// print_list(d->read_locking_pids);
			}
			// eprintk("Locking command release!\n");
			wake_up_all(&d->blockq);
			wake_up_all(&d->blockq_N);
		}
		// When the module is loaded or the file with no lock request is closed;
		else
		{
			// eprintk("I am in BBB!\n");
		}
		osp_spin_unlock(&(d->mutex));

		// This line avoids compiler warnings; you may remove it.
		(void) filp_writable, (void) d;

	}

	return 0;
}


/*
 * osprd_lock
 */

/*
 * osprd_ioctl(inode, filp, cmd, arg)
 *   Called to perform an ioctl on the named file.
 */
int osprd_ioctl(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	osprd_info_t *d = file2osprd(filp);	// device info
	int r = 0;			// return value: initially 0

	// is file open for writing?
	int filp_writable = (filp->f_mode & FMODE_WRITE) != 0;

	// This line avoids compiler warnings; you may remove it.
	(void) filp_writable, (void) d;

	// Set 'r' to the ioctl's return value: 0 on success, negative on error

	if (cmd == OSPRDIOCACQUIRE) {

		// Request a write lock;
		if (filp_writable)
		{
			osp_spin_lock(&(d->mutex));
			//Assign a ticket for this process;
			unsigned local_ticket = d->ticket_head;
			d->ticket_head++;
			osp_spin_unlock(&(d->mutex));
			wake_up_all(&d->blockq_N);

			// eprintk("W:WriteSize: %d ReadSize: %d localTicket: %d ticket_tail: %d\n",
			// 	d->write_locking_pids->size, d->read_locking_pids->size, local_ticket, d->ticket_tail);

			// wait_event_interruptible(d->blockq, d->ticket_tail == local_ticket
			// 														&& d->write_locking_pids->size == 0
			// 														&& d->read_locking_pids->size == 0);

			if (wait_event_interruptible(d->blockq, d->ticket_tail == local_ticket
																					&& d->write_locking_pids->size == 0
																					&& d->read_locking_pids->size == 0))
			{
				if (d->ticket_tail == local_ticket)
				{
					//grant ticket to next alive process;
					d->ticket_tail = next_valid_ticket(d->invalid_ticket, d->ticket_tail);
				}
				else
				{
					add_to_list(d->invalid_ticket, local_ticket);
				}
				r = -ERESTARTSYS;
			}


			osp_spin_lock(&(d->mutex));
			//This process acquires a writing lock
			filp->f_flags |= F_OSPRD_LOCKED;
			add_to_list(d->write_locking_pids, current->pid);
			// print_list(d->write_locking_pids);
			//grant ticket to next alive process;
			d->ticket_tail = next_valid_ticket(d->invalid_ticket, d->ticket_tail);
			osp_spin_unlock(&(d->mutex));
			wake_up_all(&d->blockq);
		}
		else
		{
			osp_spin_lock(&(d->mutex));
			//Assign a ticket for this process;
			unsigned local_ticket = d->ticket_head;
			d->ticket_head++;
			osp_spin_unlock(&(d->mutex));
			wake_up_all(&d->blockq_N);

			// eprintk("R:WriteSize: %d ReadSize: %d localTicket: %d ticket_tail: %d\n",
			// 	d->write_locking_pids->size, d->read_locking_pids->size, local_ticket, d->ticket_tail);

			// wait_event_interruptible(d->blockq, d->ticket_tail == local_ticket
			// 														&& d->write_locking_pids->size == 0);

			if (wait_event_interruptible(d->blockq, d->ticket_tail == local_ticket
																						&& d->write_locking_pids->size == 0))
			{
				osp_spin_lock(&(d->mutex));
				// eprintk("READ[%d] is killed!\n", local_ticket);
				if (d->ticket_tail == local_ticket)
				{
					//grant ticket to next alive process;
					d->ticket_tail = next_valid_ticket(d->invalid_ticket, d->ticket_tail);
				}
				else
				{
					add_to_list(d->invalid_ticket, local_ticket);
				}
				osp_spin_unlock(&(d->mutex));
				return -ERESTARTSYS;
			}
			osp_spin_lock(&(d->mutex));
			//This process acquires a reading lock
			filp->f_flags |= F_OSPRD_LOCKED;
			add_to_list(d->read_locking_pids, current->pid);
			// print_list(d->read_locking_pids);
			//grant ticket to next alive process;
			d->ticket_tail = next_valid_ticket(d->invalid_ticket, d->ticket_tail);
			osp_spin_unlock(&(d->mutex));
			wake_up_all(&d->blockq);
		}

		// Your code here (instead of the next two lines).
		// eprintk("Attempting to acquire\n");
		// r = -ENOTTY;

	} else if (cmd == OSPRDIOCTRYACQUIRE) {

		// Your code here (instead of the next two lines).
		if (filp_writable)
		{
			osp_spin_lock(&(d->mutex));
			//Assign a ticket for this process;
			unsigned local_ticket = d->ticket_head;
			d->ticket_head++;
			osp_spin_unlock(&(d->mutex));
			wake_up_all(&d->blockq_N);

			// eprintk("WriteSize: %d ReadSize: %d localTicket: %d ticket_tail: %d\n",
			// 	d->write_locking_pids->size, d->read_locking_pids->size, local_ticket, d->ticket_tail);

			if (!(d->ticket_tail == local_ticket && d->write_locking_pids->size == 0 && d->read_locking_pids->size == 0))
			{
				d->ticket_tail = next_valid_ticket(d->invalid_ticket, d->ticket_tail);
				return -EBUSY;
			}

			osp_spin_lock(&(d->mutex));
			//This process acquires a writing lock
			filp->f_flags |= F_OSPRD_LOCKED;
			add_to_list(d->write_locking_pids, current->pid);
			// print_list(d->write_locking_pids);
			//grant ticket to next alive process;
			d->ticket_tail = next_valid_ticket(d->invalid_ticket, d->ticket_tail);
			osp_spin_unlock(&(d->mutex));
			wake_up_all(&d->blockq);
		}
		else
		{
			osp_spin_lock(&(d->mutex));
			//Assign a ticket for this process;
			unsigned local_ticket = d->ticket_head;
			d->ticket_head++;
			osp_spin_unlock(&(d->mutex));
			wake_up_all(&d->blockq_N);

			// eprintk("WriteSize: %d ReadSize: %d localTicket: %d ticket_tail: %d\n",
			// 	d->write_locking_pids->size, d->read_locking_pids->size, local_ticket, d->ticket_tail);

			if (!(d->ticket_tail == local_ticket && d->write_locking_pids->size == 0))
			{
				d->ticket_tail = next_valid_ticket(d->invalid_ticket, d->ticket_tail);
				return -EBUSY;
			}

			osp_spin_lock(&(d->mutex));
			//This process acquires a reading lock
			filp->f_flags |= F_OSPRD_LOCKED;
			add_to_list(d->read_locking_pids, current->pid);
			// print_list(d->read_locking_pids);
			//grant ticket to next alive process;
			d->ticket_tail = next_valid_ticket(d->invalid_ticket, d->ticket_tail);
			osp_spin_unlock(&(d->mutex));
			wake_up_all(&d->blockq);
		}

	} else if (cmd == OSPRDIOCRELEASE) {

		// Your code here (instead of the next line).
		osp_spin_lock(&(d->mutex));
		if((filp->f_flags & F_OSPRD_LOCKED) == 0)
			r = -EINVAL;
		else if (filp_writable)
		{
			clear_list(d->write_locking_pids);
		}
		else
		{
			clear_list(d->read_locking_pids);
		}
		filp->f_flags &= !F_OSPRD_LOCKED;
		osp_spin_unlock(&(d->mutex));

	} else if (cmd == OSPRDIOCNOTIFICATION) {
		// eprintk("Process[%d] is waiting the notification!\n", current->pid);
		// osp_spin_lock(&(d->mutex));
		// d->num_of_wait++;
		// osp_spin_unlock(&(d->mutex));
		// wait_event_interruptible(d->blockq_N, d->notification_arrived);
		// //When the process receive the notification;
		// eprintk("Process[%d] has received the notification!\n", current->pid);

		// osp_spin_lock(&(d->mutex));
		// if ((--d->num_of_wait) == 0)
		// {
		// 	d->notification_arrived = 0;
		// }
		// osp_spin_unlock(&(d->mutex));
		int *my_data;
		my_data = (int *) arg;
		int byte_begin = my_data[0];
		int byte_end = my_data[1];
		osp_spin_lock(&d->mutex);
		//Assign a ticket for this process;
		unsigned local_ticket_N = d->ticket_head_N;
		d->ticket_head_N++;
		// eprintk("Byte_begin: %d  Byte_end: %d\n", byte_begin, byte_end);
		add_to_list_N(d->wait_notif_pids, current->pid, byte_begin, byte_end);
		osp_spin_unlock(&d->mutex);

		int temp;
		temp = wait_event_interruptible(d->blockq_N, d->ticket_tail_N == local_ticket_N
																					&& is_notified(d->wait_notif_pids, current->pid));
		// eprintk("Return value: %d\n", temp);

		if (temp)
		{
			// eprintk("ticket_tail: %d; local_ticket: %d; is_notified: %d\n",
			// 	d->ticket_tail_N, local_ticket_N, is_notified(d->wait_notif_pids, current->pid));
			// eprintk("Jump into wrong place!\n");
			osp_spin_lock(&(d->mutex));
			// eprintk("READ[%d] is killed!\n", local_ticket);
			if (d->ticket_tail_N == local_ticket_N)
			{
				//grant ticket to next alive process;
				d->ticket_tail_N = next_valid_ticket(d->invalid_ticket_N, d->ticket_tail_N);
			}
			else
			{
				add_to_list(d->invalid_ticket_N, local_ticket_N);
			}
			osp_spin_unlock(&(d->mutex));
			return -ERESTARTSYS;
		}

		// eprintk("Process[%d] received the notification!\n", current->pid);
		remove_from_list(d->wait_notif_pids, current->pid);
		// d->ticket_tail_N++;
		d->ticket_tail_N = next_valid_ticket(d->invalid_ticket_N, d->ticket_tail_N);

	} else if (cmd == OSPRDIOCWAKENOTIFICATION) {
		int *my_data2;
		my_data2 = (int *) arg;
		int offset = my_data2[0];
		int size = my_data2[1];
		int write_begin = offset;
		int write_end = offset + size -1;
		// eprintk("Write_begin: %d  Write_end: %d\n", write_begin, write_end);
		osp_spin_lock(&d->mutex);
		notify_process(d->wait_notif_pids, write_begin, write_end);
		osp_spin_unlock(&d->mutex);
		// wake_up_all(&d->blockq_N);

	} else
		r = -ENOTTY; /* unknown command */
	return r;
}


// Initialize internal fields for an osprd_info_t.

static void osprd_setup(osprd_info_t *d)
{
	/* Initialize the wait queue. */
	init_waitqueue_head(&d->blockq);
	osp_spin_lock_init(&d->mutex);
	d->ticket_head = d->ticket_tail = 0;
	/* Add code here if you add fields to osprd_info_t. */
	d->write_locking_pids = kmalloc(sizeof(num_list_t*), GFP_ATOMIC);
	d->write_locking_pids->size = 0;
	d->write_locking_pids->head = NULL;
	d->write_locking_pids->tail = NULL;
	d->read_locking_pids = kmalloc(sizeof(num_list_t*), GFP_ATOMIC);
	d->read_locking_pids->size = 0;
	d->read_locking_pids->head = NULL;
	d->read_locking_pids->tail = NULL;
	d->invalid_ticket = kmalloc(sizeof(num_list_t*), GFP_ATOMIC);
	d->invalid_ticket->size = 0;
	d->invalid_ticket->head = NULL;
	d->invalid_ticket->tail = NULL;
	// Variables for notification;
	init_waitqueue_head(&d->blockq_N);
	d->wait_notif_pids = kmalloc(sizeof(num_list_t*), GFP_ATOMIC);
	d->wait_notif_pids->size = 0;
	d->wait_notif_pids->head = NULL;
	d->wait_notif_pids->tail = NULL;
	d->ticket_head_N = d->ticket_tail_N = 0;
	d->invalid_ticket_N = kmalloc(sizeof(num_list_t*), GFP_ATOMIC);
	d->invalid_ticket_N->size = 0;
	d->invalid_ticket_N->head = NULL;
	d->invalid_ticket_N->tail = NULL;
}


/*****************************************************************************/
/*         THERE IS NO NEED TO UNDERSTAND ANY CODE BELOW THIS LINE!          */
/*                                                                           */
/*****************************************************************************/

// Process a list of requests for a osprd_info_t.
// Calls osprd_process_request for each element of the queue.

static void osprd_process_request_queue(request_queue_t *q)
{
	osprd_info_t *d = (osprd_info_t *) q->queuedata;
	struct request *req;

	while ((req = elv_next_request(q)) != NULL)
		osprd_process_request(d, req);
}


// Some particularly horrible stuff to get around some Linux issues:
// the Linux block device interface doesn't let a block device find out
// which file has been closed.  We need this information.

static struct file_operations osprd_blk_fops;
static int (*blkdev_release)(struct inode *, struct file *);

static int _osprd_release(struct inode *inode, struct file *filp)
{
	if (file2osprd(filp))
		osprd_close_last(inode, filp);
	return (*blkdev_release)(inode, filp);
}

static int _osprd_open(struct inode *inode, struct file *filp)
{
	if (!osprd_blk_fops.open) {
		memcpy(&osprd_blk_fops, filp->f_op, sizeof(osprd_blk_fops));
		blkdev_release = osprd_blk_fops.release;
		osprd_blk_fops.release = _osprd_release;
	}
	filp->f_op = &osprd_blk_fops;
	return osprd_open(inode, filp);
}


// The device operations structure.

static struct block_device_operations osprd_ops = {
	.owner = THIS_MODULE,
	.open = _osprd_open,
	// .release = osprd_release, // we must call our own release
	.ioctl = osprd_ioctl
};


// Given an open file, check whether that file corresponds to an OSP ramdisk.
// If so, return a pointer to the ramdisk's osprd_info_t.
// If not, return NULL.

static osprd_info_t *file2osprd(struct file *filp)
{
	if (filp) {
		struct inode *ino = filp->f_dentry->d_inode;
		if (ino->i_bdev
		    && ino->i_bdev->bd_disk
		    && ino->i_bdev->bd_disk->major == OSPRD_MAJOR
		    && ino->i_bdev->bd_disk->fops == &osprd_ops)
			return (osprd_info_t *) ino->i_bdev->bd_disk->private_data;
	}
	return NULL;
}


// Call the function 'callback' with data 'user_data' for each of 'task's
// open files.

static void for_each_open_file(struct task_struct *task,
		  void (*callback)(struct file *filp, osprd_info_t *user_data),
		  osprd_info_t *user_data)
{
	int fd;
	task_lock(task);
	spin_lock(&task->files->file_lock);
	{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 13)
		struct files_struct *f = task->files;
#else
		struct fdtable *f = task->files->fdt;
#endif
		for (fd = 0; fd < f->max_fds; fd++)
			if (f->fd[fd])
				(*callback)(f->fd[fd], user_data);
	}
	spin_unlock(&task->files->file_lock);
	task_unlock(task);
}


// Destroy a osprd_info_t.

static void cleanup_device(osprd_info_t *d)
{
	wake_up_all(&d->blockq);
	if (d->gd) {
		del_gendisk(d->gd);
		put_disk(d->gd);
	}
	if (d->queue)
		blk_cleanup_queue(d->queue);
	if (d->data)
		vfree(d->data);
}


// Initialize a osprd_info_t.

static int setup_device(osprd_info_t *d, int which)
{
	memset(d, 0, sizeof(osprd_info_t));

	/* Get memory to store the actual block data. */
	if (!(d->data = vmalloc(nsectors * SECTOR_SIZE)))
		return -1;
	memset(d->data, 0, nsectors * SECTOR_SIZE);

	/* Set up the I/O queue. */
	spin_lock_init(&d->qlock);
	if (!(d->queue = blk_init_queue(osprd_process_request_queue, &d->qlock)))
		return -1;
	blk_queue_hardsect_size(d->queue, SECTOR_SIZE);
	d->queue->queuedata = d;

	/* The gendisk structure. */
	if (!(d->gd = alloc_disk(1)))
		return -1;
	d->gd->major = OSPRD_MAJOR;
	d->gd->first_minor = which;
	d->gd->fops = &osprd_ops;
	d->gd->queue = d->queue;
	d->gd->private_data = d;
	snprintf(d->gd->disk_name, 32, "osprd%c", which + 'a');
	set_capacity(d->gd, nsectors);
	add_disk(d->gd);

	/* Call the setup function. */
	osprd_setup(d);

	return 0;
}

static void osprd_exit(void);


// The kernel calls this function when the module is loaded.
// It initializes the 4 osprd block devices.

static int __init osprd_init(void)
{
	int i, r;

	// shut up the compiler
	(void) for_each_open_file;
#ifndef osp_spin_lock
	(void) osp_spin_lock;
	(void) osp_spin_unlock;
#endif

	/* Register the block device name. */
	if (register_blkdev(OSPRD_MAJOR, "osprd") < 0) {
		printk(KERN_WARNING "osprd: unable to get major number\n");
		return -EBUSY;
	}

	/* Initialize the device structures. */
	for (i = r = 0; i < NOSPRD; i++)
		if (setup_device(&osprds[i], i) < 0)
			r = -EINVAL;

	if (r < 0) {
		printk(KERN_EMERG "osprd: can't set up device structures\n");
		osprd_exit();
		return -EBUSY;
	} else
		return 0;
}


// The kernel calls this function to unload the osprd module.
// It destroys the osprd devices.

static void osprd_exit(void)
{
	int i;
	for (i = 0; i < NOSPRD; i++)
		cleanup_device(&osprds[i]);
	unregister_blkdev(OSPRD_MAJOR, "osprd");
}


// Tell Linux to call those functions at init and exit time.
module_init(osprd_init);    //When the module is loaded, call osprd_init;
module_exit(osprd_exit);    //When the module is closed, call osprd_exit;
