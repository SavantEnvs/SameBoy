/* Cheat search (Core/cheat_search.c) and its filter-expression evaluator
 * (GB_debugger_evaluate_cheat_filter in Core/debugger.c). Byte 0 picks the data type; each
 * following line is one filter expression, applied between short runs of the ROM. */
#include "sb_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1) return -1;
    static const GB_cheat_search_data_type_t types[] = {
        GB_CHEAT_SEARCH_DATA_TYPE_8BIT, GB_CHEAT_SEARCH_DATA_TYPE_16BIT,
        GB_CHEAT_SEARCH_DATA_TYPE_16BIT_BE,
    };
    GB_cheat_search_data_type_t type = types[data[0] % 3];
    data++; size--;
    GB_gameboy_t *gb = sb_new(GB_MODEL_CGB_E);
    static uint8_t rom[0x8000];
    sb_make_rom(rom, sizeof(rom), 0x13, 0x00, 0x03);
    GB_load_rom_from_buffer(gb, rom, sizeof(rom));
    sb_run_cycles(gb, 1);
    GB_cheat_search_reset(gb);
    char *text = malloc(size + 1);
    memcpy(text, data, size);
    text[size] = 0;
    char *save = NULL;
    unsigned n = 0;
    for (char *line = strtok_r(text, "\n", &save); line && n < 16; line = strtok_r(NULL, "\n", &save), n++) {
        GB_cheat_search_filter(gb, line, type);
        size_t count = GB_cheat_search_result_count(gb);
        if (count && count < 0x10000) {
            GB_cheat_search_result_t *results = malloc(count * sizeof(*results));
            GB_cheat_search_get_results(gb, results);
            free(results);
        }
        GB_run(gb);
    }
    free(text);
    sb_delete(gb);
    return 0;
}
