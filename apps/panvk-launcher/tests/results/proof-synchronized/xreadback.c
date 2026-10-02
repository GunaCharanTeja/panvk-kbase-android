/* XGetImage of the Termux:X11 root. Minimal decls so this builds with
 * the NDK against imagefs libX11 (no host glibc headers). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned long XID;
typedef XID Window;
typedef XID Drawable;
typedef unsigned long Atom;
typedef unsigned long VisualID;
typedef struct _XDisplay Display;
typedef struct _XImage XImage;

typedef struct {
    int x, y;
    int width, height;
    int border_width;
    int depth;
    void *visual;
    Window root;
    int class;
    int bit_gravity;
    int win_gravity;
    int backing_store;
    unsigned long backing_planes;
    unsigned long backing_pixel;
    int save_under;
    long colormap;
    int map_installed;
    int map_state;
    long all_event_masks;
    long your_event_mask;
    long do_not_propagate_mask;
    int override_redirect;
    void *screen;
} XWindowAttributes;

struct _XImage {
    int width, height;
    int xoffset;
    int format;
    char *data;
    int byte_order;
    int bitmap_unit;
    int bitmap_bit_order;
    int bitmap_pad;
    int depth;
    int bytes_per_line;
    int bits_per_pixel;
    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
    unsigned long obdata;
    struct funcs {
        XImage *(*create_image)();
        int (*destroy_image)(XImage *);
        unsigned long (*get_pixel)(XImage *, int, int);
        int (*put_pixel)(XImage *, int, int, unsigned long);
        XImage *(*sub_image)(XImage *, int, int, unsigned int, unsigned int);
        int (*add_pixel)(XImage *, long);
    } f;
};

enum { IsUnmapped = 0, IsUnviewable = 1, IsViewable = 2, ZPixmap = 2 };

Display *XOpenDisplay(const char *);
int XCloseDisplay(Display *);
Window XDefaultRootWindow(Display *);
int XGetWindowAttributes(Display *, Window, XWindowAttributes *);
int XFetchName(Display *, Window, char **);
int XQueryTree(Display *, Window, Window *, Window *, Window **, unsigned int *);
int XFree(void *);
XImage *XGetImage(Display *, Drawable, int, int, unsigned int, unsigned int, unsigned long, int);
int XDestroyImage(XImage *);

static unsigned long pix(XImage *im, int x, int y)
{
    return im->f.get_pixel(im, x, y);
}

static void dump(const char *path, XImage *im)
{
    FILE *f = fopen(path, "wb");
    int y, orange = 0, n = 0;
    if (!f) {
        perror(path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", im->width, im->height);
    for (y = 0; y < im->height; y++) {
        int x;
        for (x = 0; x < im->width; x++) {
            unsigned long p = pix(im, x, y);
            unsigned char rgb[3];
            int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
            rgb[0] = (unsigned char)r;
            rgb[1] = (unsigned char)g;
            rgb[2] = (unsigned char)b;
            if (r > 200 && g > 80 && g < 180 && b < 100)
                orange++;
            n++;
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    printf("WROTE %s %dx%d bpp=%d masks=%lx/%lx/%lx orange=%d/%d\n",
           path, im->width, im->height, im->bits_per_pixel,
           im->red_mask, im->green_mask, im->blue_mask, orange, n);
}

static void grab_win(Display *d, Window w, const XWindowAttributes *a, const char *tag)
{
    char path[256];
    XImage *im;

    if (a->width < 32 || a->height < 32 || a->map_state != IsViewable)
        return;
    im = XGetImage(d, w, 0, 0, (unsigned)a->width, (unsigned)a->height, ~0ul, ZPixmap);
    if (!im) {
        printf("GRAB fail id=0x%lx\n", (unsigned long)w);
        return;
    }
    snprintf(path, sizeof(path), "%s-0x%lx.ppm", tag, (unsigned long)w);
    dump(path, im);
    XDestroyImage(im);
}

static void walk(Display *d, Window w, int depth, const char *tag)
{
    Window root, parent, *kids = NULL;
    unsigned n = 0, i;
    XWindowAttributes a;
    char *name = NULL;

    if (!XGetWindowAttributes(d, w, &a))
        return;
    XFetchName(d, w, &name);
    printf("WIN depth=%d id=0x%lx map=%d %dx%d+%d+%d name=\"%s\"\n",
           depth, (unsigned long)w, a.map_state == IsViewable,
           a.width, a.height, a.x, a.y, name ? name : "");
    grab_win(d, w, &a, tag);
    if (name)
        XFree(name);
    if (!XQueryTree(d, w, &root, &parent, &kids, &n))
        return;
    for (i = 0; i < n; i++)
        walk(d, kids[i], depth + 1, tag);
    if (kids)
        XFree(kids);
}

int main(int argc, char **argv)
{
    const char *dpyname = getenv("DISPLAY");
    const char *out = argc > 1 ? argv[1] : "/data/local/tmp/xread.ppm";
    Display *d;
    Window root;
    XWindowAttributes a;
    XImage *im;

    if (!dpyname)
        dpyname = ":0";
    d = XOpenDisplay(dpyname);
    if (!d) {
        fprintf(stderr, "XOpenDisplay(%s) failed\n", dpyname);
        return 1;
    }
    root = XDefaultRootWindow(d);
    walk(d, root, 0, out);
    if (!XGetWindowAttributes(d, root, &a)) {
        fprintf(stderr, "root attrs failed\n");
        return 2;
    }
    printf("ROOT %dx%d depth=%d\n", a.width, a.height, a.depth);
    im = XGetImage(d, root, 0, 0, (unsigned)a.width, (unsigned)a.height, ~0ul, ZPixmap);
    if (!im) {
        fprintf(stderr, "XGetImage failed\n");
        return 3;
    }
    dump(out, im);
    XDestroyImage(im);
    XCloseDisplay(d);
    return 0;
}
