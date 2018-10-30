//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Memory Container
//
////////////////////////////////////////////////////////////////////////

#include "memory_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>

struct container_list
{
    __u64 cid;
    struct process_list* process_head;
    struct object_list* object_head;
    struct container_list* next;
};

struct process_list
{
    int pid;
    struct process_list* next;
};

struct object_list
{
    __u64 oid;
    unsigned long v_addr;
    struct object_list* next;
};

struct mutex_list
{
    __u64 oid;
    struct mutex* m;
    struct mutex_list* next;
};

extern struct miscdevice memory_container_dev;

struct container_list* start;
struct mutex container_mutex;
struct mutex object_mutex;


int memory_container_init(void)
{
    int ret;

    if ((ret = misc_register(&memory_container_dev)))
    {
        printk(KERN_ERR "Unable to register \"memory_container\" misc device\n");
        return ret;
    }

    mutex_init(&container_mutex);
    mutex_init(&object_mutex);
    start = NULL;

    printk(KERN_ERR "\"memory_container\" misc device installed\n");
    printk(KERN_ERR "\"memory_container\" version 0.1\n");
    return ret;
}


void memory_container_exit(void)
{
    misc_deregister(&memory_container_dev);
}
