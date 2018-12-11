/*
 * PC DMA controller defines and functions
 * Copyright (C) 2018 René Rebe, ExactCODE; Germany.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Alternatively, commercial licensing options are available from the
 * copyright holder ExactCODE GmbH Germany.
 */

static uint8_t addr_reg[] = { 0, 2, 4, 6, 0xc0, 0xc4, 0xc8, 0xcc };
static uint8_t count_reg[] = { 1, 3, 5, 7, 0xc2, 0xc6, 0xca, 0xce };
static uint8_t page_reg[] = { 0x87, 0x83, 0x81, 0x82, 0x8f, 0x8b, 0x89, 0x8a };
static uint8_t flipflop_reg[] = {0xa, 0xd8};
static uint8_t disable_reg[] =  {0xb, 0xd4};
static uint8_t mode_reg[] = {0xc, 0xd6};
static uint8_t clear_reg[] =  {0xe, 0xdc};

void dma_setup(uint8_t dma, uint32_t realaddr, uint32_t dma_size)
{
  printf("dma_setup: %d, %x, %x\n", dma, realaddr, dma_size);
  
  outb(disable_reg[dma >> 2], dma | 4); // disable channel
  uint8_t mode =
    (1<<6) | // single-cycle
    (0<<5) | // address increment
    (1<<4) | // auto-init dma
    (2<<2) | // read
    (dma & 3); // channel #
  outb(mode_reg[dma >> 2], mode);
  
  // set address
  // set page
  outb(page_reg[dma], realaddr >> 16);
  
  if (dma > 3) {
    // 16-bit in words
    realaddr >>= 1;
    dma_size >>= 1;
  }
  
  outb(flipflop_reg[dma >> 2], 0); // prepare to send 16-bit value
  outb(addr_reg[dma], realaddr & 0xff);
  outb(addr_reg[dma], (realaddr >> 8) & 0xff);
  
  outb(flipflop_reg[dma >> 2], 0); // prepare to send 16-bit value
  outb(count_reg[dma], (dma_size - 1) & 0xff);
  outb(count_reg[dma], (dma_size - 1) >> 8);
  
  outb(clear_reg[dma >> 2], 0);	// clear write mask
  outb(disable_reg[dma >> 2], dma & ~4);
}

void dma_disable(uint8_t dma)
{
  outb(disable_reg[dma >> 2], dma | 4); // disable dma channel
}
