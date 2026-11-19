/* GBS (Game Boy Sound) file loader (Core/gb.c GB_load_gbs_from_buffer): header parse, the
 * synthesized player stub, then the tune plays on the emulated CPU/APU. */
#include "sb_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    GB_gameboy_t *gb = sb_new(GB_MODEL_CGB_E);
    GB_gbs_info_t info;
    if (GB_load_gbs_from_buffer(gb, data, size, &info) == 0) {
        GB_gbs_switch_track(gb, info.first_track);
        sb_run_cycles(gb, 2);
        if (info.track_count) {
            GB_gbs_switch_track(gb, (uint8_t)((info.first_track + 1) % info.track_count));
            sb_run_cycles(gb, 1);
        }
    }
    sb_delete(gb);
    return 0;
}
