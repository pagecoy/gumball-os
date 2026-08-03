.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 16
stack_bottom:
.skip 16384 /* 16Kb stack, just marking it */
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    mov $stack_top, %esp   /* sets up the stack so the whole thing can run */
    push %ebx 
    push %eax
    call kernel_main        /* calls the kernel, handing off control */
    cli                      /* if kernel_main ever returns, halt forever */
1:  hlt
    jmp 1b

.size _start, . - _start

/* --- Keyboard interrupt stub --- */
.global isr33
.type isr33, @function
isr33:
    pusha
    call keyboard_handler
    movb $0x20, %al
    outb %al, $0x20
    popa
    iret
.size isr33, . - isr33

/* --- Load our own GDT and start using it --- */
.global gdt_flush
.type gdt_flush, @function
gdt_flush:
    mov 4(%esp), %eax
    lgdt (%eax)              /* tell the CPU where our new GDT lives */

    mov $0x10, %ax            /* 0x10 = our data segment selector */
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    ljmp $0x08, $.gdt_flush_done
.gdt_flush_done:
    ret
.size gdt_flush, . - gdt_flush