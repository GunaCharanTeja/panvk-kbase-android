/* Tiny read-only kbase probe: version check + gpuprops dump. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

struct vc { uint16_t major, minor; };
struct gp { uint64_t buf; uint32_t size, flags; };

int main(void)
{
   int fd = open("/dev/mali0", O_RDWR | O_CLOEXEC);
   if (fd < 0) { perror("open"); return 1; }
   struct vc v = { 11, 0 };
   int r = ioctl(fd, _IOWR(0x80, 0, struct vc), &v);
   printf("VERSION_CHECK(jm,nr0) r=%d major=%u minor=%u\n", r, v.major, v.minor);
   if (r) {
      v.major = 1; v.minor = 0;
      r = ioctl(fd, _IOWR(0x80, 52, struct vc), &v);
      printf("VERSION_CHECK(csf,nr52) r=%d major=%u minor=%u\n", r, v.major, v.minor);
   }
   uint32_t flags = 0;
   r = ioctl(fd, _IOW(0x80, 1, uint32_t), &flags);
   printf("SET_FLAGS r=%d\n", r);
   struct gp g = { 0, 0, 0 };
   int sz = ioctl(fd, _IOW(0x80, 3, struct gp), &g);
   printf("GPUPROPS size=%d\n", sz);
   if (sz <= 0) return 1;
   uint8_t *b = calloc(1, sz);
   g.buf = (uintptr_t)b; g.size = sz;
   if (ioctl(fd, _IOW(0x80, 3, struct gp), &g) < 0) { perror("gpuprops"); return 1; }
   for (int o = 0; o + 4 <= sz;) {
      uint32_t k = *(uint32_t *)(b + o); o += 4;
      int t = k & 3, n = 1 << t; uint64_t val = 0;
      for (int i = 0; i < n; i++) val |= (uint64_t)b[o + i] << (8 * i);
      o += n;
      printf("prop %u = 0x%llx\n", k >> 2, (unsigned long long)val);
   }
   return 0;
}
