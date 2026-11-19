/* Debugger command interpreter (Core/debugger.c): command dispatch, the expression lexer and
 * evaluator, breakpoints/watchpoints, the disassembler and the info commands, plus tab
 * completion. Each input line is one command, as typed at the debugger prompt. No command
 * touches files or the host, so none needs to be filtered out. */
#include "sb_common.h"

#define MAX_LINES 64

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    GB_gameboy_t *gb = sb_new(GB_MODEL_CGB_E);
    static uint8_t rom[0x8000];
    sb_make_rom(rom, sizeof(rom), 0x13 /* MBC3+RAM+BATTERY */, 0x00, 0x03);
    GB_load_rom_from_buffer(gb, rom, sizeof(rom));
    sb_run_cycles(gb, 1);

    char *text = malloc(size + 1);
    memcpy(text, data, size);
    text[size] = 0;
    char *save = NULL;
    unsigned n = 0;
    for (char *line = strtok_r(text, "\n", &save); line && n < MAX_LINES; line = strtok_r(NULL, "\n", &save), n++) {
        char *complete = strdup(line);
        uintptr_t context = 0;
        for (unsigned i = 0; i < 8; i++) {
            char *copy = strdup(complete);
            char *result = GB_debugger_complete_substring(gb, copy, &context);
            free(copy);
            if (!result) break;
            free(result);
        }
        free(complete);
        char *cmd = strdup(line);
        GB_debugger_execute_command(gb, cmd); /* destroys its input */
        free(cmd);
    }
    free(text);
    /* Let breakpoints/watchpoints/step commands take effect. */
    sb_run_cycles(gb, 1);
    sb_delete(gb);
    return 0;
}
