/* Known-answer functional checks for libsameboy's Core (mayhem layer; upstream ships no unit
 * tests that run without the rgbds-built boot ROMs). Prints one `name=value` line per check;
 * mayhem/test.sh compares them with mayhem/selftest.expected. */
#include "harnesses/sb_common.h"

static char logbuf[65536];
static size_t loglen;
static void capture_log(GB_gameboy_t *gb, const char *s, GB_log_attributes_t a)
{
    (void)gb; (void)a;
    size_t n = strlen(s);
    if (loglen + n < sizeof(logbuf)) { memcpy(logbuf + loglen, s, n); loglen += n; logbuf[loglen] = 0; }
}

static unsigned fnv(const uint8_t *p, size_t n)
{
    unsigned h = 2166136261u;
    while (n--) { h ^= *p++; h *= 16777619u; }
    return h;
}

int main(void)
{
    static uint8_t rom[0x8000];
    sb_make_rom(rom, sizeof(rom), 0x10, 0x00, 0x03);
    GB_gameboy_t *gb = sb_new(GB_MODEL_CGB_E);
    GB_load_rom_from_buffer(gb, rom, sizeof(rom));
    sb_run_cycles(gb, 4);
    printf("rom_wram=%02x\n", GB_read_memory(gb, 0xC000));
    printf("rom_cartram=%02x\n", GB_read_memory(gb, 0xA000));
    printf("lcdc=%02x\n", GB_read_memory(gb, 0xFF40));

    /* Save state round trip */
    size_t ss = GB_get_save_state_size(gb);
    uint8_t *state = malloc(ss);
    GB_save_state_to_buffer(gb, state);
    GB_model_t m = 0;
    printf("state_model_rc=%d\n", GB_get_state_model_from_buffer(state, ss, &m));
    printf("state_model=%03x\n", (unsigned)m);
    GB_write_memory(gb, 0xC000, 0x11);
    printf("state_load_rc=%d\n", GB_load_state_from_buffer(gb, state, ss));
    printf("state_wram=%02x\n", GB_read_memory(gb, 0xC000));
    printf("state_bad_rc_nonzero=%d\n", GB_load_state_from_buffer(gb, state, 16) != 0);
    free(state);

    /* Battery */
    int bs = GB_save_battery_size(gb);
    printf("battery_size=%d\n", bs);
    uint8_t *bat = calloc(1, bs);
    GB_save_battery_to_buffer(gb, bat, bs);
    printf("battery_byte0=%02x\n", bat[0]);
    bat[1] = 0x5A;
    GB_load_battery_from_buffer(gb, bat, bs);
    printf("battery_reload=%02x\n", GB_read_memory(gb, 0xA001));
    free(bat);

    /* Debugger expressions and commands */
    uint16_t r = 0, b = 0;
    printf("eval_arith=%d,%u\n", GB_debugger_evaluate(gb, "1+2*3", &r, &b), r);
    printf("eval_hex=%d,%u\n", GB_debugger_evaluate(gb, "$10 << 2", &r, &b), r);
    printf("eval_mem=%d,%02x\n", GB_debugger_evaluate(gb, "[$c000]", &r, &b), r);
    GB_set_log_callback(gb, capture_log);
    loglen = 0; logbuf[0] = 0;
    char cmd[] = "disassemble/1 $160";
    GB_debugger_execute_command(gb, cmd);
    for (char *q = logbuf; *q; q++) if (*q == '\n') *q = '|';
    printf("disasm=%s\n", logbuf);
    loglen = 0; logbuf[0] = 0;
    char cmd2[] = "print 6*7";
    GB_debugger_execute_command(gb, cmd2);
    for (char *q = logbuf; *q; q++) if (*q == '\n') *q = '|';
    printf("print=%s\n", logbuf);
    GB_set_log_callback(gb, sb_log);

    /* Symbols */
    char path[512];
    const char *sym = "; comment\n00:0160 start\n00:0170 loop\n";
    sb_tmpfile((const uint8_t *)sym, strlen(sym), path, sizeof(path));
    GB_debugger_load_symbol_file(gb, path);
    unlink(path);
    const char *name = GB_debugger_name_for_address(gb, 0x160);
    printf("sym_name=%s\n", name ? name : "(null)");
    printf("sym_eval=%d,%x\n", GB_debugger_evaluate(gb, "loop + 1", &r, &b), r);

    /* Cheats: GameShark 01VVLLHH writes VV to HHLL */
    const GB_cheat_t *c = GB_import_cheat(gb, "017701C0", "t", true);
    printf("cheat_import=%d\n", c != NULL);
    printf("cheat_bad=%d\n", GB_import_cheat(gb, "zzz", "t", true) != NULL);
    size_t nc = 0;
    GB_get_cheats(gb, &nc);
    printf("cheat_count=%zu\n", nc);
    GB_set_cheats_enabled(gb, true);
    sb_run_cycles(gb, 1);
    printf("cheat_applied=%02x\n", GB_read_memory(gb, 0xC001));
    GB_remove_all_cheats(gb);

    /* Cheat search */
    GB_cheat_search_reset(gb);
    GB_cheat_search_filter(gb, "new == $42", GB_CHEAT_SEARCH_DATA_TYPE_8BIT);
    size_t cs = GB_cheat_search_result_count(gb);
    printf("cheat_search_nonzero=%d\n", cs > 0);
    printf("cheat_search_bad=%d\n", GB_cheat_search_filter(gb, "((", GB_CHEAT_SEARCH_DATA_TYPE_8BIT));
    sb_delete(gb);

    /* GBS header */
    static uint8_t gbs[0x70 + 0x100];
    memset(gbs, 0, sizeof(gbs));
    memcpy(gbs, "GBS", 3);
    gbs[3] = 1; gbs[4] = 3; gbs[5] = 1;
    gbs[6] = 0x00; gbs[7] = 0x04; /* load 0x400 */
    gbs[8] = 0x00; gbs[9] = 0x04; /* init */
    gbs[10] = 0x01; gbs[11] = 0x04; /* play */
    gbs[12] = 0xFE; gbs[13] = 0xFF;
    memcpy(gbs + 0x10, "Fuzz Tune", 9);
    memcpy(gbs + 0x30, "Mayhem", 6);
    gbs[0x70] = 0xC9; gbs[0x71] = 0xC9; /* ret ; ret */
    gb = sb_new(GB_MODEL_CGB_E);
    GB_gbs_info_t info;
    memset(&info, 0, sizeof(info));
    printf("gbs_rc=%d\n", GB_load_gbs_from_buffer(gb, gbs, sizeof(gbs), &info));
    printf("gbs_info=%u,%u,%s,%s\n", info.track_count, info.first_track, info.title, info.author);
    printf("gbs_bad_rc_nonzero=%d\n", GB_load_gbs_from_buffer(gb, gbs, 8, &info) != 0);
    sb_delete(gb);

    /* DMG run: deterministic frame contents */
    gb = sb_new(GB_MODEL_DMG_B);
    GB_load_rom_from_buffer(gb, rom, sizeof(rom));
    sb_run_cycles(gb, 3);
    printf("dmg_wram=%02x\n", GB_read_memory(gb, 0xC000));
    printf("dmg_frame=%08x\n", fnv((const uint8_t *)sb_pixels, 160 * 144 * 4));
    sb_delete(gb);
    printf("done=1\n");
    return 0;
}
