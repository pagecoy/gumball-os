#include <stdint.h>
#include "idt.h"
#include "io.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

extern void isr33(void);

static void idt_set_gate(int n, uint32_t handler) {
    idt[n].offset_low  = handler & 0xFFFF;
    idt[n].offset_high = (handler >> 16) & 0xFFFF;
    idt[n].selector    = 0x08;
    idt[n].zero        = 0;
    idt[n].type_attr   = 0x8E;
}

static void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFD);
    outb(0xA1, 0xFF);
}

void idt_init(void) {
    pic_remap();

    idt_set_gate(33, (uint32_t) isr33);

    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t) &idt;
    __asm__ volatile ("lidt %0" : : "m"(idtp));

    __asm__ volatile ("sti");
}