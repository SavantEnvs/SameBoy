/* Save-state loader: native SameBoy states and BESS (Core/save_state.c), then emulation
 * continues from the loaded state and the state is saved back out.
 * Seeds: state_*.sav are full native states (83-159 KB; native states always carry the whole
 * RAM+VRAM, so they cannot be small). bess_*.sav are the same states as BESS-only files with the
 * RAM/VRAM/cart-RAM/SGB-border buffers declared empty (the loader zero-fills short buffers), under
 * 1.5 KB, so a fuzzer with a small input-length limit (libFuzzer's default -max_len=4096) still
 * gets loadable inputs. */
#include "sb_common.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    GB_model_t model = GB_MODEL_CGB_E;
    GB_model_t state_model;
    if (GB_get_state_model_from_buffer(data, size, &state_model) == 0 && sb_known_model(state_model)) {
        model = state_model;
    }
    GB_gameboy_t *gb = sb_new(model);
    static uint8_t rom[0x8000];
    sb_make_rom(rom, sizeof(rom), 0x10 /* MBC3+TIMER+RAM+BATTERY */, 0x00, 0x03);
    GB_load_rom_from_buffer(gb, rom, sizeof(rom));
    if (GB_load_state_from_buffer(gb, data, size) == 0) {
        sb_run_cycles(gb, 2);
        size_t out_size = GB_get_save_state_size(gb);
        uint8_t *out = malloc(out_size);
        GB_save_state_to_buffer(gb, out);
        free(out);
    }
    sb_delete(gb);
    return 0;
}
