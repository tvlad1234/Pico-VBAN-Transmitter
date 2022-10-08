#ifndef _VBAN_H
#define _VBAN_H

#include "pico/stdlib.h"

typedef uint8_t VBT_BYTE;
typedef uint32_t VBT_DWORD;

struct tagVBAN_HEADER
{
VBT_DWORD vban; // contains 'V' 'B', 'A', 'N'
VBT_BYTE format_SR; // SR index (see SRList above)
VBT_BYTE format_nbs; // nb sample per frame (1 to 256)
VBT_BYTE format_nbc; // nb channel (1 to 256)
VBT_BYTE format_bit; // mask = 0x07 (see DATATYPE table below)
char streamname[16]; // stream name
VBT_DWORD nuFrame; // growing frame number
};

typedef struct tagVBAN_HEADER T_VBAN_HEADER;
#define VBAN_HEADER_SIZE (4 + 4 + 16 +4)

#define VBAN_PROTOCOL_AUDIO 0x00
#define VBAN_PROTOCOL_SERIAL 0x20
#define VBAN_PROTOCOL_TXT 0x40
#define VBAN_PROTOCOL_SERVICE 0x60

#define VBAN_DATATYPE_BYTE8 0x00
#define VBAN_DATATYPE_INT16 0x01
#define VBAN_DATATYPE_INT24 0x02
#define VBAN_DATATYPE_INT32 0x03
#define VBAN_DATATYPE_FLOAT32 0x04
#define VBAN_DATATYPE_FLOAT64 0x05
#define VBAN_DATATYPE_12BITS 0x06
#define VBAN_DATATYPE_10BITS 0x07

// Construct VBAN header
void vban_construct_header(T_VBAN_HEADER *header, uint8_t SR_INDEX, uint8_t SUB_PROTOCOL, uint8_t nbSample, uint8_t nbChannel, uint8_t BIT_RES, char stName[]);

// Increment packet counter in header
void vban_update_packet_count(T_VBAN_HEADER *header);

// get SR_INDEX from sample rate in Hz
int vban_get_SR_from_list(long sr);

#endif