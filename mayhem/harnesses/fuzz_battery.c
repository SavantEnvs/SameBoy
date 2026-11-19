/* Battery save (.sav) loader (Core/gb.c GB_load_battery_from_buffer): cartridge RAM plus the
 * RTC trailers (VBA/BGB MBC3 RTC, HuC3, TPP1), then emulation and saving it back. Byte 0 picks
 * which cartridge the save is loaded for; the rest is the .sav file. */
#include "sb_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    static const uint8_t types[] = {0x10, 0x13, 0x03, 0x06, 0x1B, 0x1E, 0x22, 0xFE, 0xFF, 0xBC, 0xFC, 0x0F};
    uint8_t sel = data[0];
    data++; size--;
    uint8_t type = types[sel % sizeof(types)];
    GB_gameboy_t *gb = sb_new(sb_models[(sel >> 4) % SB_N_MODELS]);
    static uint8_t rom[0x8000];
    sb_make_rom(rom, sizeof(rom), type, 0x00, 0x03);
    if (type == 0xBC) { /* TPP1: magic, 8KiB RAM, all features (battery, RTC, rumble) */
        rom[0x149] = 0xC1; rom[0x14A] = 0x65; rom[0x152] = 0x01; rom[0x153] = 0x0F;
    }
    GB_load_rom_from_buffer(gb, rom, sizeof(rom));
    GB_load_battery_from_buffer(gb, data, size);
    sb_run_cycles(gb, 2);
    int out_size = GB_save_battery_size(gb);
    if (out_size > 0) {
        uint8_t *out = malloc(out_size);
        GB_save_battery_to_buffer(gb, out, out_size);
        free(out);
    }
    sb_delete(gb);
    return 0;
}
