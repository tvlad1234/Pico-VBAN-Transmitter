#include "pico/stdlib.h"
#include "vban.h"

#define VBAN_SR_MAXNUMBER 21
static long VBAN_SRList[VBAN_SR_MAXNUMBER] =
    {6000, 12000, 24000, 48000, 96000, 192000, 384000,
     8000, 16000, 32000, 64000, 128000, 256000, 512000,
     11025, 22050, 44100, 88200, 176400, 352800, 705600};

int vban_get_SR_from_list(long sr)
{
    for(int i=0; i< VBAN_SR_MAXNUMBER; i++)
        if(sr == VBAN_SRList[i])
            return i;
    return -1;
}

void vban_construct_header(T_VBAN_HEADER *header, uint8_t SR_INDEX, uint8_t SUB_PROTOCOL, uint8_t nbSample, uint8_t nbChannel, uint8_t BIT_RES, char stName[])
{
    header->vban = 'NABV';                  // identification
    header->format_SR = SR_INDEX;           // sample rate
    header->format_SR |= SUB_PROTOCOL << 5; // sub-protocol
    header->format_nbs = nbSample-1;
    header->format_nbc = nbChannel-1;
    header->format_bit = BIT_RES;
    strcpy(header->streamname, stName);
}

void vban_update_packet_count(T_VBAN_HEADER *header)
{
    header->nuFrame++;
}
