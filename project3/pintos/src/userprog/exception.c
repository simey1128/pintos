#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/pte.h"

#include "threads/palloc.h"


static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);
static bool is_stack(uint32_t *fault_addr, void *esp);
static bool
install_page (void *upage, void *kpage, bool writable);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
   bool not_present;  /* True: not-present page, false: writing r/o page. */
   bool write;        /* True: access was write, false: access was read. */
   bool user;         /* True: access by user, false: access by kernel. */
   void *fault_addr;  /* Fault address. */

   /* Obtain faulting address, the virtual address that was
      accessed to cause the fault.  It may point to code or to
      data.  It is not necessarily the address of the instruction
      that caused the fault (that's f->eip).
      See [IA32-v2a] "MOV--Move to/from Control Registers" and
      [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
      (#PF)". */
   asm ("movl %%cr2, %0" : "=r" (fault_addr));

   /* Turn interrupts back on (they were only off so that we could
      be assured of reading CR2 before it changed). */
   intr_enable ();

   /* Count page faults. */
   page_fault_cnt++;

   /* Determine cause. */
   not_present = (f->error_code & PF_P) == 0;
   write = (f->error_code & PF_W) != 0;
   user = (f->error_code & PF_U) != 0;

   if(fault_addr==NULL || !user || is_kernel_vaddr(fault_addr)) {
      exit(-1);
   }

   if(!not_present){
     exit(-1);
   }
   struct vpage_entry *vte = get_vte(fault_addr);
   uint32_t *kpage = kalloc(PAL_USER);
   if(vte == NULL){
      if(is_stack(fault_addr, f->esp)){
         load_stack_segment(fault_addr, f->esp);
         return;
      }
      exit(-1);
   }

   if(vte->type == SUP_PAGE){
      if(!lazy_load_segment(kpage, vte)){
         kfree(kpage);
         exit(-1);
      }
   }
   if(vte->type == MAP_PAGE){
      if(!load_mapped_file(kpage, vte)){
         kfree(kpage);
         exit(-1);
      }
   }
   if(vte->type == REM_PAGE){
      swap_in(kpage, thread_current()->pagedir, vte);
   }

   if(!install_page(vte->vaddr, kpage, vte->writable)){
      kfree(kpage);
      exit(-1);
   }
   vte -> on_frame = true;
   return;
  
   if(fault_addr > PHYS_BASE - 0x800000 && fault_addr <= thread_current() -> stack_boundary){
      load_stack_segment(fault_addr, f->esp);
      return;
   }
}



int
lazy_load_segment (struct vpage_entry* vte, uint32_t *kpage){
   file_seek (vte->file, vte->ofs);
   if (file_read (vte->file, kpage, vte->read_bytes) != (int) vte->read_bytes)
   {
      // palloc_free_page (kpage);
      return false; 
   }
   memset (kpage + vte->read_bytes, 0, vte->zero_bytes);

   return true;
}

int load_mapped_file(struct vpage_entry *vte, uint32_t *kpage){
   int read_bytes = file_read_at(vte -> file, (void *)kpage, vte->read_bytes, vte->ofs);
   if (read_bytes > PGSIZE){
      palloc_free_page (kpage);
      return false;
   }
   memset (kpage + read_bytes, 0, PGSIZE - read_bytes);

   return true;
}

void load_stack_segment(uint32_t* fault_addr, void* f_esp){
   uint32_t *upage = (uint32_t *)((uint32_t)fault_addr & 0xfffff000);
   uint32_t *f_esp_page = (uint32_t *)((uint32_t)f_esp & 0xfffff000);
   uint32_t *stack_boundary = thread_current()->stack_boundary;
   struct thread *t = thread_current();
   
   if(fault_addr < f_esp-4){
      exit(-1);
   }

   uint32_t *kpage = kalloc(PAL_USER | PAL_ZERO);
   struct vpage_entry *vte = malloc(sizeof(*vte));
   
   vte -> type = REM_PAGE;
   vte -> vaddr = fault_addr;
   vte -> upage = upage;
   vte -> writable = true;
   vte -> on_frame = true;
   vte -> swap_disable = false;

   list_push_back(&t -> vpage_table, &vte->elem);

   if(!install_page(upage, kpage, true)){
      kfree(vte->upage);
      // free(vte);
      exit(-1);
   }
}

static bool is_stack(uint32_t *fault_addr, void *esp){
   if(fault_addr >= PHYS_BASE - 0x800000 && fault_addr <= thread_current() -> stack_boundary && fault_addr >= esp - 32)
      return true;
   return false;
}


static bool
install_page (void *upage, void *kpage, bool writable)
{
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
   // if(pagedir_get_page (thread_current()->pagedir, upage) != NULL)
   //    printf("Page aleady exists\n");
   // if(!pagedir_set_page (thread_current()->pagedir, upage, kpage, writable))
   //    printf("Fail of pagedir_set_page\n");
  return (pagedir_get_page (thread_current()->pagedir, upage) == NULL
          && pagedir_set_page (thread_current()->pagedir, upage, kpage, writable));
}
