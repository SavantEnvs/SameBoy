/* Shared setup for the SameBoy fuzz harnesses (mayhem layer, not upstream code).
 * Every harness creates a fresh GB_gameboy_t per input and frees it afterwards, so no
 * emulator state survives between inputs. Process-global knobs (GB_random_set_enabled)
 * are set to the same value on every input. */
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "Core/gb.h"

static uint32_t sb_pixels[256 * 224];

static uint32_t sb_rgb(GB_gameboy_t *gb, uint8_t r, uint8_t g, uint8_t b)
{
    (void)gb;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void sb_vblank(GB_gameboy_t *gb, GB_vblank_type_t type) { (void)gb; (void)type; }

static void sb_log(GB_gameboy_t *gb, const char *string, GB_log_attributes_t attributes)
{
    (void)gb; (void)string; (void)attributes;
}

/* The default input callbacks read stdin; a harness must never block on (or consume) stdin. */
static char *sb_no_input(GB_gameboy_t *gb) { (void)gb; return NULL; }

static void sb_sample(GB_gameboy_t *gb, GB_sample_t *sample) { (void)gb; (void)sample; }

static const GB_model_t sb_models[] = {
    GB_MODEL_DMG_B, GB_MODEL_SGB_NTSC, GB_MODEL_SGB2, GB_MODEL_CGB_E, GB_MODEL_AGB_A, GB_MODEL_MGB,
    GB_MODEL_CGB_C, GB_MODEL_SGB_PAL,
};
#define SB_N_MODELS (sizeof(sb_models) / sizeof(sb_models[0]))

static int sb_known_model(GB_model_t m)
{
    static const GB_model_t all[] = {
        GB_MODEL_DMG_B, GB_MODEL_SGB_NTSC, GB_MODEL_SGB_PAL, GB_MODEL_SGB_NTSC_NO_SFC,
        GB_MODEL_SGB_PAL_NO_SFC, GB_MODEL_MGB, GB_MODEL_SGB2, GB_MODEL_SGB2_NO_SFC,
        GB_MODEL_CGB_0, GB_MODEL_CGB_A, GB_MODEL_CGB_B, GB_MODEL_CGB_C, GB_MODEL_CGB_D,
        GB_MODEL_CGB_E, GB_MODEL_AGB_A, GB_MODEL_GBP_A,
    };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) if (all[i] == m) return 1;
    return 0;
}

static void sb_setup(GB_gameboy_t *gb)
{
    GB_random_set_enabled(false);
    GB_set_pixels_output(gb, sb_pixels);
    GB_set_rgb_encode_callback(gb, sb_rgb);
    GB_set_vblank_callback(gb, sb_vblank);
    GB_set_log_callback(gb, sb_log);
    GB_set_input_callback(gb, sb_no_input);
    GB_set_async_input_callback(gb, sb_no_input);
    GB_set_sample_rate(gb, 8000);
    GB_apu_set_sample_callback(gb, sb_sample);
}

/* A minimal boot ROM (ld a,1 ; ldh [$50],a) so the boot ROM is unmapped and the cartridge
 * starts at $0100 the way it does after a real boot; with no boot ROM at all the core never
 * leaves boot mode, which disables e.g. cheats. */
static void sb_boot(GB_gameboy_t *gb)
{
    static uint8_t boot[0x900] = {0x3E, 0x01, 0xE0, 0x50};
    GB_load_boot_rom_from_buffer(gb, boot, sizeof(boot));
}

static GB_gameboy_t *sb_new(GB_model_t model)
{
    GB_gameboy_t *gb = GB_init(GB_alloc(), model);
    sb_setup(gb);
    sb_boot(gb);
    GB_reset(gb);
    return gb;
}

static void sb_delete(GB_gameboy_t *gb)
{
    GB_free(gb);
    GB_dealloc(gb);
}

/* Run for roughly `frames` frames worth of 8MHz-unit cycles. Bounded by cycles, not vblanks:
 * a program that turns the LCD off never produces a vblank. */
static void sb_run_cycles(GB_gameboy_t *gb, unsigned frames)
{
    uint64_t budget = (uint64_t)frames * 70224 * 2, done = 0;
    unsigned guard = 0;
    while (done < budget && guard++ < 2000000) {
        done += GB_run(gb);
    }
}

/* A small fixed cartridge: entry jumps to 0x160, which stores known values into WRAM and
 * cartridge RAM and then spins. `type`, `rom_size`, `ram_size` go into the header. */
static void sb_make_rom(uint8_t *rom, size_t size, uint8_t type, uint8_t rom_size, uint8_t ram_size)
{
    memset(rom, 0, size);
    static const uint8_t entry[] = {0x00, 0xC3, 0x60, 0x01};
    memcpy(rom + 0x100, entry, sizeof(entry));
    memcpy(rom + 0x134, "MAYHEMTEST", 10);
    rom[0x147] = type;
    rom[0x148] = rom_size;
    rom[0x149] = ram_size;
    static const uint8_t code[] = {
        0x3E, 0x42, 0xEA, 0x00, 0xC0,       /* ld a,$42 ; ld [$C000],a */
        0x3E, 0x0A, 0xEA, 0x00, 0x00,       /* ld a,$0A ; ld [$0000],a (RAM enable) */
        0x3E, 0x99, 0xEA, 0x00, 0xA0,       /* ld a,$99 ; ld [$A000],a */
        0x3E, 0x91, 0xE0, 0x40,             /* ld a,$91 ; ldh [$40],a (LCD on) */
        0x21, 0x01, 0xC0,                   /* ld hl,$C001 */
        0x34,                               /* inc [hl] */
        0x18, 0xFD,                         /* jr -3 */
    };
    memcpy(rom + 0x160, code, sizeof(code));
    uint8_t hc = 0;
    for (unsigned i = 0x134; i <= 0x14C; i++) hc = hc - rom[i] - 1;
    rom[0x14D] = hc;
}

/* Write `data` to a private temp file for the path-only loaders (GB_load_isx, GB_load_cheats,
 * GB_debugger_load_symbol_file). Returns 0 on success; caller unlinks `path`. */
static int sb_tmpfile(const uint8_t *data, size_t size, char *path, size_t path_size)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir) dir = "/tmp";
    snprintf(path, path_size, "%s/sameboy-fuzz-XXXXXX", dir);
    int fd = mkstemp(path);
    if (fd < 0) return -1;
    size_t off = 0;
    while (off < size) {
        ssize_t w = write(fd, data + off, size - off);
        if (w <= 0) { close(fd); unlink(path); return -1; }
        off += (size_t)w;
    }
    close(fd);
    return 0;
}
