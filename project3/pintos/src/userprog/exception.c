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

//   if(fault_addr==NULL) printf("fault_addr==NULL\n");
//   if(!user) printf("!user\n");
//   if(is_kernel_vaddr(fault_addr)) printf("is_kernel_vaddr(fault_addr)\n");

   // printf("fault_addr: %p\n", fault_addr);


  if(fault_addr==NULL || !user || is_kernel_vaddr(fault_addr)) {
   //   PANIC("exception validty");
     exit(-1);
  }

   // 1. stack grow
   // printf("fault_addr: %p, f->esp: %p\n", fault_addr, f->esp);
   if(fault_addr > PHYS_BASE - 0x800000 
            && fault_addr <= thread_current() -> stack_boundary){
      if(fault_addr < f->esp) exit(-1);
      uint32_t *upage = (uint32_t *)((uint32_t)fault_addr & 0xfffff000);
      uint8_t *kpage = palloc_get_page(PAL_USER);

      if (kpage == NULL){
         reclaim(upage);
         kpage = palloc_get_page(PAL_USER);
      }

      fid_t fid = falloc(kpage, upage);
      thread_current() -> stack_boundary = upage;

      struct swap_entry* se = get_swap_entry(thread_current()->pagedir, upage);

      if(se==NULL); // 
      else swap_in(kpage, se);

      if (!install_page (upage, kpage, true)) 
      {
      palloc_free_page (kpage);
      
      PANIC("syscall, install_page error");
      }
      return;
   }

   // 2. mapped file
   struct mmap_entry *me = get_me(fault_addr);
   if(me != NULL){
      int tmp = load_mapped_file(me, fault_addr);
      if(tmp == -1)
         exit(-1);
      return;
   }
   
   // 3. spte에 존재하는 경우
   struct thread *t = thread_current();
   struct spage_entry* spte = get_spte((uint32_t)fault_addr&0xfffff000);
   if(spte == NULL) {
      // PANIC("spte error");
      exit(-1);
   }

   //4. load spte
   if(!lazy_load_segment(spte)) {
      // PANIC("lazy_load_segment error");
      exit(-1);
   }

   //3. page tabel entry update(valid bit)
   pagedir_set_present(thread_current()->pagedir, fault_addr, true);

  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */
//   printf ("Page fault at %p: %s error %s page in %s context.\n",
//           fault_addr,
//           not_present ? "not present" : "rights violation",
//           write ? "writing" : "reading",
//           user ? "user" : "kernel");
//   kill (f);
}



int
lazy_load_segment (struct spage_entry* spte){
   // printf("spte->read_bytes: %d\n", spte->read_bytes);
   // printf("spte->zero_bytes: %d\n", spte->zero_bytes);
   // printf("writable: %d\n", spte->writable);
   file_seek (spte->file, spte->ofs);  

   uint32_t *kpage = palloc_get_page (PAL_USER);
   if (kpage == NULL){
      reclaim(spte->upage);
      kpage = palloc_get_page(PAL_USER);
   }

   fid_t fid = falloc(kpage, spte->upage);
   if(fid == -1)
      return false;  

   struct swap_entry* se = get_swap_entry(thread_current()->pagedir, spte->upage);

   if(se == NULL){ // page is not swapped out
      if (file_read (spte->file, kpage, spte->read_bytes) != (int) spte->read_bytes)
      {
         palloc_free_page (kpage);
         return false; 
      }
      memset (kpage + spte->read_bytes, 0, spte->zero_bytes);
   }

   else swap_in(kpage, se);
   

   if (!install_page (spte->upage, kpage, spte->writable)) 
        {
          palloc_free_page (kpage);
          return false;
        }

   return true;
}

int load_mapped_file(struct mmap_entry *me, uint32_t *uaddr){
   
   uint8_t *kpage = palloc_get_page(PAL_USER);
   uint32_t *upage = (uint32_t)uaddr & 0xfffff000;
   if(kpage == NULL){
      reclaim(upage);
      kpage = palloc_get_page(PAL_USER);
   }

   fid_t fid = falloc(kpage, upage);
   if(fid == -1)
      return false;  

   struct swap_entry* se = get_swap_entry(thread_current()->pagedir, upage);

   if(se == NULL){ // page is not swapped out
      int read_bytes = file_read_at(me -> file, (void *)kpage, PGSIZE, upage - me->start_addr);
      if (read_bytes > PGSIZE){
         palloc_free_page (kpage);
         return false;
      }
      memset (kpage + read_bytes, 0, PGSIZE - read_bytes);
   } else swap_in(kpage, se);
   

   if (!install_page (upage, kpage, true)) {
      palloc_free_page (kpage);
      return false;
   }

   return true;
}


static bool
install_page (void *upage, void *kpage, bool writable)
{
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (thread_current()->pagedir, upage) == NULL
          && pagedir_set_page (thread_current()->pagedir, upage, kpage, writable));
}
