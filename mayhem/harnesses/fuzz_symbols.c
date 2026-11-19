/* Debugger symbol file (.sym) parser (Core/debugger.c GB_debugger_load_symbol_file) and the
 * symbol maps it fills (Core/symbol_hash.c), then lookups and expression evaluation against
 * them. The loader only takes a path, so the input goes to a private temp file. */
#include "sb_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    char path[512];
    if (sb_tmpfile(data, size, path, sizeof(path))) return 0;
    GB_gameboy_t *gb = sb_new(GB_MODEL_CGB_E);
    static uint8_t rom[0x8000];
    sb_make_rom(rom, sizeof(rom), 0x13, 0x00, 0x03);
    GB_load_rom_from_buffer(gb, rom, sizeof(rom));
    GB_debugger_load_symbol_file(gb, path);
    unlink(path);
    for (unsigned addr = 0; addr < 0x10000; addr += 0x0FF1) {
        GB_debugger_name_for_address(gb, (uint16_t)addr);
        GB_debugger_describe_address(gb, (uint16_t)addr, (uint16_t)(addr >> 14), false, true);
    }
    uint16_t result, bank;
    GB_debugger_evaluate(gb, "main", &result, &bank);
    GB_debugger_evaluate(gb, "start + 1", &result, &bank);
    sb_run_cycles(gb, 1);
    GB_debugger_clear_symbols(gb);
    sb_delete(gb);
    return 0;
}
