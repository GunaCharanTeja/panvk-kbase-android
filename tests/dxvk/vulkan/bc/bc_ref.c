/* Host-only reference for bc_verify.py: Mesa's CPU BPTC decoder
 * (util/format/texcompress_bptc_tmp.h). Never linked into the driver.
 * usage: bc_ref <bc6u|bc6s|bc7> <blocks_w> <blocks_h> < blocks > rgba_f32 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "util/format/texcompress_bptc_tmp.h"

/* Standalone build: no CPU detection, software half conversion. */
struct _util_cpu_caps_state_t _util_cpu_caps_state = {.detect_done = 1};
void _util_cpu_detect_once(void) {}

int
main(int argc, char **argv)
{
   if (argc < 4)
      return 2;
   int bw = atoi(argv[2]), bh = atoi(argv[3]);
   int is7 = !strcmp(argv[1], "bc7");
   int sgn = !strcmp(argv[1], "bc6s");
   uint8_t blk[16];
   static float out[4096 * 4 * 16];
   int w = bw * 4;

   if (bw * bh * 16 > 4096)
      return 2;
   for (int by = 0; by < bh; by++)
      for (int bx = 0; bx < bw; bx++) {
         if (fread(blk, 1, 16, stdin) != 16)
            return 1;
         for (int t = 0; t < 16; t++) {
            float *px = &out[((by * 4 + t / 4) * w + bx * 4 + t % 4) * 4];
            if (is7) {
               uint8_t c[4];
               fetch_rgba_unorm_from_block(blk, c, t);
               for (int k = 0; k < 4; k++)
                  px[k] = c[k] / 255.0f;
            } else {
               fetch_rgb_float_from_block(blk, px, t, sgn);
            }
         }
      }
   fwrite(out, sizeof(float), bw * bh * 16 * 4, stdout);
   return 0;
}
