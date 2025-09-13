#pragma once
#include "tusb_config.h"
// Terminal IDs of importance to the descriptors and handlers
// Unit numbers are arbitrary selected
#define TERMID_CLK          0x04
#define TERMID_SPK_IN       0x01
#define TERMID_SPK_FEAT     0x02
#define TERMID_SPK_OUT      0x03
#define TERMID_MIC_IN       0x11
#define TERMID_MIC_OUT      0x13

inline u16 volumeFactor = 0;
