/* ISX (Intelligent Systems) ROM/symbol file loader (Core/gb.c GB_load_isx). The loader only
 * takes a path, so the input is written to a private temp file first. */
#include "sb_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char path[512];
    if (sb_tmpfile(data, size, path, sizeof(path))) return 0;
    GB_gameboy_t *gb = sb_new(GB_MODEL_CGB_E);
    if (GB_load_isx(gb, path) == 0) {
        GB_reset(gb);
        sb_run_cycles(gb, 1);
        for (unsigned addr = 0; addr < 0x10000; addr += 0x1111) {
            GB_debugger_name_for_address(gb, (uint16_t)addr);
        }
    }
    unlink(path);
    sb_delete(gb);
    return 0;
}
