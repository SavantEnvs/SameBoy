/* Cartridge loader + emulation: header/MBC detection (Core/mbc.c), then the CPU, PPU, APU,
 * timers, SGB packets and the MBC write handlers run the input as a program. Byte 0 picks the
 * model and options; the rest is the ROM image. */
#include "sb_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    uint8_t opts = data[0];
    data++; size--;
    GB_gameboy_t *gb = sb_new(sb_models[opts % SB_N_MODELS]);
    if (opts & 0x08) GB_set_rewind_length(gb, 1);
    GB_load_rom_from_buffer(gb, data, size);
    if (opts & 0x10) {
        /* Loading a second ROM into the same instance re-runs the cartridge configuration
         * (and the RAM reallocation it does) on an already-configured emulator. */
        GB_load_rom_from_buffer(gb, data, size);
    }
    GB_reset(gb);
    sb_run_cycles(gb, 3);
    size_t state_size = GB_get_save_state_size(gb);
    uint8_t *state = malloc(state_size);
    GB_save_state_to_buffer(gb, state);
    sb_run_cycles(gb, 1);
    GB_load_state_from_buffer(gb, state, state_size);
    free(state);
    if (opts & 0x08) {
        sb_run_cycles(gb, 2);
        GB_rewind_pop(gb);
    }
    sb_delete(gb);
    return 0;
}
