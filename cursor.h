/*
 * Generic, simple mouse cursor shape.
 * Copyright (C) 2018 René Rebe, ExactCODE; Germany.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * Alternatively, commercial licensing options are available from the
 * copyright holder ExactCODE GmbH Germany.
 */

// TODO:
// * bits packed

#define cursor cursor_8_16

static const uint8_t cursor_8_16[16][8] = {
  {2, 0, 0, 0, 0, 0, 0, 0},
  {2, 2, 0, 0, 0, 0, 0, 0},
  {2, 1, 2, 0, 0, 0, 0, 0},
  {2, 1, 1, 2, 0, 0, 0, 0},
  {2, 1, 1, 1, 2, 0, 0, 0},
  {2, 1, 1, 1, 1, 2, 0, 0},
  {2, 1, 1, 1, 1, 1, 2, 0},
  {2, 1, 1, 1, 1, 1, 1, 2},
  {2, 1, 1, 1, 1, 1, 2, 2},
  {2, 1, 1, 1, 1, 2, 0, 0},
  {2, 1, 2, 2, 1, 2, 0, 0},
  {2, 2, 0, 0, 2, 1, 2, 0},
  {0, 0, 0, 0, 2, 1, 2, 0},
  {0, 0, 0, 0, 0, 2, 1, 2},
  {0, 0, 0, 0, 0, 2, 1, 2},
  {0, 0, 0, 0, 0, 0, 2, 0},
};

static const uint8_t cursor_10_18[18][10] = {
  {2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {2, 2, 0, 0, 0, 0, 0, 0, 0, 0},
  {2, 1, 2, 0, 0, 0, 0, 0, 0, 0},
  {2, 1, 1, 2, 0, 0, 0, 0, 0, 0},
  {2, 1, 1, 1, 2, 0, 0, 0, 0, 0},
  {2, 1, 1, 1, 1, 2, 0, 0, 0, 0},
  {2, 1, 1, 1, 1, 1, 2, 0, 0, 0},
  {2, 1, 1, 1, 1, 1, 1, 2, 0, 0},
  {2, 1, 1, 1, 1, 1, 1, 1, 2, 0},
  {2, 1, 1, 1, 1, 1, 1, 1, 1, 2},
  {2, 1, 1, 1, 1, 1, 2, 2, 2, 2},
  {2, 1, 1, 2, 1, 1, 2, 2, 0, 0},
  {2, 1, 2, 0, 2, 1, 1, 2, 0, 0},
  {2, 2, 0, 0, 0, 2, 1, 1, 2, 0},
  {0, 0, 0, 0, 0, 2, 1, 1, 2, 0},
  {0, 0, 0, 0, 0, 0, 2, 1, 1, 2},
  {0, 0, 0, 0, 0, 0, 2, 1, 1, 2},
  {0, 0, 0, 0, 0, 0, 0, 2, 2, 0},
};

static const uint8_t cursor_11_19[19][11] = {
  {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {2, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0},
  {2, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0},
  {2, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0},
  {2, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0},
  {2, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0},
  {2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0},
  {2, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0},
  {2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
  {2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2},
  {2, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0},
  {2, 1, 1, 2, 2, 1, 1, 2, 0, 0, 0},
  {2, 1, 2, 0, 2, 1, 1, 2, 0, 0, 0},
  {2, 2, 0, 0, 0, 2, 1, 1, 2, 0, 0},
  {2, 0, 0, 0, 0, 2, 1, 1, 2, 0, 0},
  {0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 0},
  {0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 0},
  {0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0},
};
