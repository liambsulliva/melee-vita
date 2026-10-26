/* SPDX-License-Identifier: GPL-3.0-or-later */
/* The calls the game code makes for Slippi online play
 * (src/melee/slippi/slippi_platform.h), passed on to the network client.
 * The online CSS calls are in slippi_ui.c. */
#include "slippi_internal.h"

#include <melee/slippi/slippi_platform.h>

#include <psp2/kernel/threadmgr.h>

void mp_platform_slippi_log(const char* text)
{
    sp_log("%s", text);
}

void mp_platform_slippi_idle(void)
{
    sceKernelDelayThread(1000);
}

int mp_platform_slippi_net_status(void)
{
    return slippi_net_status();
}

void mp_platform_slippi_net_poll(void)
{
    slippi_net_poll();
}

int mp_platform_slippi_net_match_block(unsigned char* out)
{
    return slippi_net_match_block(out);
}

int mp_platform_slippi_net_send_inputs(int frame, int delay,
                                       int finalized_frame,
                                       unsigned checksum,
                                       const unsigned char* local_pad,
                                       unsigned char* remote_pad)
{
    return slippi_net_send_inputs(frame, delay, finalized_frame, checksum,
                                  local_pad, remote_pad);
}

int mp_platform_slippi_net_remote_checksum_frame(void)
{
    return slippi_net_remote_checksum_frame();
}

unsigned mp_platform_slippi_net_remote_checksum_value(void)
{
    return slippi_net_remote_checksum_value();
}

int mp_platform_slippi_net_ping_ms(void)
{
    return slippi_net_ping_ms();
}

int mp_platform_slippi_net_time_offset_us(void)
{
    return slippi_net_time_offset_us();
}
