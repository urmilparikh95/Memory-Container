//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2018
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
//     Core of Kernel Module for Processor Container
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
#include <linux/kthread.h>

struct container_list
{
    __u64 cid;
    struct process_list* process_head;
    struct object_list* object_head;
    struct mutex_list* mutex_head;
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

extern struct mutex container_mutex;
// extern struct mutex object_mutex;
extern struct container_list* start;


int memory_container_mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long a;
    mutex_lock(&container_mutex);
    struct container_list* temp_container = start;
    struct process_list* temp_proc;
    int flag=0;
    while(temp_container != NULL)
    {
        flag = 0;
        temp_proc = temp_container->process_head;
        while(temp_proc != NULL)
        {
            if(temp_proc->pid == current->pid)
            {
                flag = 1;
                break;
            }
            temp_proc = temp_proc->next;
        }
        if(flag == 1)
        {
            break;
        }
        temp_container = temp_container->next;
    }
    /*printk(KERN_INFO "m cid = %llu pid = %d..\n",temp_container->cid,temp_proc->pid);*/
    struct object_list* temp_object = temp_container->object_head;
    flag = 0;
    while(temp_object != NULL)
    {
        flag = 0;
        if(temp_object->oid == vma->vm_pgoff)
        {
            flag = 1;
            a = temp_object->v_addr;
            break;
        }
        temp_object = temp_object->next;
    }
    if(flag == 0)
    {
        a = kmalloc((vma->vm_end - vma->vm_start)*sizeof(char), GFP_KERNEL);
        temp_object = (struct object_list*)kmalloc(sizeof(struct object_list), GFP_KERNEL);
        temp_object->oid = vma->vm_pgoff;
        temp_object->v_addr = a;
        temp_object->next = NULL;
        struct object_list* temp2 = temp_container->object_head;
        if(temp2 == NULL)
        {
            temp_container->object_head = temp_object;
        }
        else
        {
            while(temp2->next != NULL)
            {
                temp2 = temp2->next;
            }
            temp2->next = temp_object;
        }
    }
    mutex_unlock(&container_mutex);
    /*a = kmalloc((vma->vm_end - vma->vm_start)*sizeof(char), GFP_KERNEL);*/
    unsigned long pfn = virt_to_phys((void *)a)>>PAGE_SHIFT;
    remap_pfn_range(vma, vma->vm_start, pfn, vma->vm_end - vma->vm_start, vma->vm_page_prot);
    return 0;
}


int memory_container_lock(struct memory_container_cmd __user *user_cmd)
{
    // printk(KERN_INFO "pid = %d entered lock..\n", current->pid);
    // int a = current->pid;
    // printk(KERN_INFO "pid = %d a = %d..\n", current->pid, a);
    // int i;
    // for(i = 0; i < 10000; i++)
    // {

    // }
    // printk(KERN_INFO "pid = %d a = %d..\n", current->pid, a);
    // printk(KERN_INFO "pid = %d exited lock..\n", current->pid);
    // mutex_lock(&object_mutex);
    struct mutex* tm;
    struct memory_container_cmd *kernel_cmd = (struct memory_container_cmd*)kmalloc(sizeof(struct memory_container_cmd), GFP_KERNEL);
    copy_from_user(kernel_cmd, user_cmd, sizeof(*user_cmd));
    mutex_lock(&container_mutex);
    struct container_list* temp_container = start;
    struct process_list* temp_proc;
    int flag=0;
    while(temp_container != NULL)
    {
        flag = 0;
        temp_proc = temp_container->process_head;
        while(temp_proc != NULL)
        {
            if(temp_proc->pid == current->pid)
            {
                flag = 1;
                break;
            }
            temp_proc = temp_proc->next;
        }
        if(flag == 1)
        {
            break;
        }
        temp_container = temp_container->next;
    }
    struct mutex_list* temp_mutex = temp_container->mutex_head;
    flag = 0;
    while(temp_mutex != NULL)
    {
        flag = 0;
        if(temp_mutex->oid == kernel_cmd->oid)
        {
            flag = 1;
            tm = temp_mutex->m;
            break;
        }
        temp_mutex = temp_mutex->next;
    }
    if(flag == 0)
    {
        tm = (struct mutex*) kmalloc(sizeof(struct mutex), GFP_KERNEL);
        temp_mutex = (struct mutex_list*)kmalloc(sizeof(struct mutex_list), GFP_KERNEL);
        temp_mutex->oid = kernel_cmd->oid;
        temp_mutex->m = tm;
        temp_mutex->next = NULL;
        struct mutex_list* temp2 = temp_container->mutex_head;
        if(temp2 == NULL)
        {
            temp_container->mutex_head = temp_mutex;
        }
        else
        {
            while(temp2->next != NULL)
            {
                temp2 = temp2->next;
            }
            temp2->next = temp_mutex;
        }
        mutex_init(tm);
    }
    mutex_unlock(&container_mutex);
    mutex_lock(tm);
    return 0;
}


int memory_container_unlock(struct memory_container_cmd __user *user_cmd)
{
    // mutex_unlock(&object_mutex);
    struct mutex* tm;
    struct memory_container_cmd *kernel_cmd = (struct memory_container_cmd*)kmalloc(sizeof(struct memory_container_cmd), GFP_KERNEL);
    copy_from_user(kernel_cmd, user_cmd, sizeof(*user_cmd));
    mutex_lock(&container_mutex);
    struct container_list* temp_container = start;
    struct process_list* temp_proc;
    int flag=0;
    while(temp_container != NULL)
    {
        flag = 0;
        temp_proc = temp_container->process_head;
        while(temp_proc != NULL)
        {
            if(temp_proc->pid == current->pid)
            {
                flag = 1;
                break;
            }
            temp_proc = temp_proc->next;
        }
        if(flag == 1)
        {
            break;
        }
        temp_container = temp_container->next;
    }
    struct mutex_list* temp_mutex = temp_container->mutex_head;
    while(temp_mutex != NULL)
    {
        if(temp_mutex->oid == kernel_cmd->oid)
        {
            tm = temp_mutex->m;
            break;
        }
        temp_mutex = temp_mutex->next;
    }
    mutex_unlock(&container_mutex);
    mutex_unlock(tm);
    return 0;
}


int memory_container_delete(struct memory_container_cmd __user *user_cmd)
{
    mutex_lock(&container_mutex);
    struct container_list* temp_container = start;
    struct container_list* prev_container = NULL;
    struct process_list* temp_proc;
    int flag = 0;
    while(temp_container != NULL)
    {
        flag = 0;
        temp_proc = temp_container->process_head;
        while(temp_proc != NULL)
        {
            if(temp_proc->pid == current->pid)
            {
                flag = 1;
                break;
            }
            temp_proc = temp_proc->next;
        }
        if(flag == 1)
        {
            break;
        }
        prev_container = temp_container;
        temp_container = temp_container->next;
    }
    if(current->pid == temp_proc->pid)
    {
        if(temp_proc == temp_container->process_head && temp_container->process_head->next == NULL)
        {
            /*if(prev_container == NULL)
            {
                start = start->next;
            }
            else
            {
                prev_container->next = temp_container->next;
            }
            */
            kfree(temp_proc);
            temp_container->process_head = NULL;
            /*kfree(temp_container);*/
        }
        else if(temp_proc == temp_container->process_head)
        {
            temp_container->process_head = temp_container->process_head->next;
            kfree(temp_proc);
        }
        else
        {
            struct process_list* temp = temp_container->process_head;
            while(temp->next != temp_proc)
            {
                temp = temp->next;
            }
            temp->next = temp_proc->next;
            kfree(temp_proc);
        }
    }
    mutex_unlock(&container_mutex);
    return 0;
}


int memory_container_create(struct memory_container_cmd __user *user_cmd)
{
    struct process_list* temp_proc = (struct process_list*)kmalloc(sizeof(struct process_list), GFP_KERNEL);
    struct memory_container_cmd *kernel_cmd = (struct memory_container_cmd*)kmalloc(sizeof(struct memory_container_cmd), GFP_KERNEL);

    temp_proc->pid = current->pid;
    temp_proc->next = NULL;

    copy_from_user(kernel_cmd, user_cmd, sizeof(*user_cmd));
    mutex_lock(&container_mutex);
    if(start == NULL)
    {
        struct container_list* temp_container = (struct container_list*)kmalloc(sizeof(struct container_list), GFP_KERNEL);
        temp_container->cid = kernel_cmd->cid;
        temp_container->process_head = temp_proc;
        temp_container->object_head = NULL;
        temp_container->mutex_head = NULL;
        temp_container->next = NULL;
        start = temp_container;
        /*printk(KERN_INFO "cid = %llu pid = %d..\n",temp_container->cid,temp_container->process_head->pid);*/
    }
    else
    {
        int flag = 0;
        struct container_list* c = start;
        while(c != NULL)
        {
            if(c->cid == kernel_cmd->cid)
            {
                flag = 1;
                struct process_list* temp2 = c->process_head;
                if(temp2 == NULL)
                {
                    c->process_head = temp_proc;
                }
                else
                {
                    while(temp2->next != NULL)
                    {
                        temp2 = temp2->next;
                    }
                    temp2->next = temp_proc;
                }
                /*printk(KERN_INFO "cid = %llu pid = %d..\n",c->cid,temp_proc->pid);*/
                break;
            }
            c = c->next;
        }
        if(flag == 0)
        {
            struct container_list* temp_container = (struct container_list*)kmalloc(sizeof(struct container_list), GFP_KERNEL);
            temp_container->cid = kernel_cmd->cid;
            temp_container->process_head = temp_proc;
            temp_container->object_head = NULL;
            temp_container->mutex_head = NULL;
            temp_container->next = NULL;
            c = start;
            while(c->next != NULL)
            {
                c = c->next;
            }
            c->next = temp_container;
            /*printk(KERN_INFO "cid = %llu pid = %d..\n",temp_container->cid,temp_container->process_head->pid);*/
        }
    }
    mutex_unlock(&container_mutex);
    return 0;
}


int memory_container_free(struct memory_container_cmd __user *user_cmd)
{
    struct memory_container_cmd *kernel_cmd = (struct memory_container_cmd*)kmalloc(sizeof(struct memory_container_cmd), GFP_KERNEL);
    copy_from_user(kernel_cmd, user_cmd, sizeof(*user_cmd));
    mutex_lock(&container_mutex);
    struct container_list* temp_container = start;
    struct process_list* temp_proc;
    int flag=0;
    while(temp_container != NULL)
    {
        flag = 0;
        temp_proc = temp_container->process_head;
        while(temp_proc != NULL)
        {
            if(temp_proc->pid == current->pid)
            {
                flag = 1;
                break;
            }
            temp_proc = temp_proc->next;
        }
        if(flag == 1)
        {
            break;
        }
        temp_container = temp_container->next;
    }
    struct object_list* temp_object = temp_container->object_head;
    if(temp_object->oid == kernel_cmd->oid)
    {
        temp_container->object_head = temp_container->object_head->next;
        kfree(temp_object->v_addr);
        kfree(temp_object);
    }
    else
    {
        while(temp_object->next != NULL)
        {
            if(temp_object->next->oid == kernel_cmd->oid)
            {
                struct object_list* to = temp_object->next;
                temp_object->next = temp_object->next->next;
                kfree(to->v_addr);
                kfree(to);
                break;
            }
            temp_object = temp_object->next;
        }
    }
    mutex_unlock(&container_mutex);
    return 0;
}


/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int memory_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case MCONTAINER_IOCTL_CREATE:
        return memory_container_create((void __user *)arg);
    case MCONTAINER_IOCTL_DELETE:
        return memory_container_delete((void __user *)arg);
    case MCONTAINER_IOCTL_LOCK:
        return memory_container_lock((void __user *)arg);
    case MCONTAINER_IOCTL_UNLOCK:
        return memory_container_unlock((void __user *)arg);
    case MCONTAINER_IOCTL_FREE:
        return memory_container_free((void __user *)arg);
    default:
        return -ENOTTY;
    }
}
