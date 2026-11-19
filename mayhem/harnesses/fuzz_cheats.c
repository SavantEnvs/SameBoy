/* Cheats (Core/cheats.c): byte 0 bit 0 selects the binary cheat-file loader (GB_load_cheats,
 * via a private temp file — it only takes a path) or the GameShark/Game Genie text importer
 * (GB_import_cheat, one code per line). The cheats are then applied while the ROM runs. */
#include "sb_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    uint8_t mode = data[0];
    data++; size--;
    GB_gameboy_t *gb = sb_new(GB_MODEL_CGB_E);
    static uint8_t rom[0x8000];
    sb_make_rom(rom, sizeof(rom), 0x13, 0x00, 0x03);
    GB_load_rom_from_buffer(gb, rom, sizeof(rom));
    if (mode & 1) {
        char path[512];
        if (sb_tmpfile(data, size, path, sizeof(path)) == 0) {
            GB_load_cheats(gb, path, (mode & 2) != 0);
            unlink(path);
        }
    }
    else {
        char *text = malloc(size + 1);
        memcpy(text, data, size);
        text[size] = 0;
        char *save = NULL;
        unsigned n = 0;
        for (char *line = strtok_r(text, "\n", &save); line && n < 64; line = strtok_r(NULL, "\n", &save), n++) {
            GB_import_cheat(gb, line, "fuzz", true);
        }
        free(text);
    }
    GB_set_cheats_enabled(gb, true);
    sb_run_cycles(gb, 1);
    size_t count = 0;
    GB_get_cheats(gb, &count);
    GB_remove_all_cheats(gb);
    sb_delete(gb);
    return 0;
}
