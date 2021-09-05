#include "main.h"
#include "triangle.h"
#include "draw.h"
#include "crap.h"
#include "mtl.h"
#include <stdlib.h>
#include <math.h>

static inline int dmin(int a, int b) { return a < b ? a : b; }

static inline int dmax(int a, int b) { return a > b ? a : b; }

#define OVERDRAW

static inline void triangle_span(
    int y, int x0, int x1, float z0, float z1,
    float r0, float r1, float g0, float g1, float b0, float b1,
    float u0, float u1, float v0, float v1, mtl_t* mtl
) {
    int startx = x0 < 0 ? 0 : (x0 > XRES ? XRES : x0);
    int endx = x1 <= 0 ? -1 : (x1 >= XRES ? XRES - 1 : x1);
    if (startx == XRES || endx == -1) {
        return;
    }
    const float x_diff = 1.f / (float)(x1 - x0);
    const float z_inc = (z1 - z0) * x_diff; const float r_inc = (r1 - r0) * x_diff;
    const float g_inc = (g1 - g0) * x_diff; const float b_inc = (b1 - b0) * x_diff;
    const float u_inc = (u1 - u0) * x_diff; const float v_inc = (v1 - v0) * x_diff;
    const int xoff = abs(startx - x0);
    float z = z0 + z_inc * xoff; float r = r0 + r_inc * xoff;
    float g = g0 + g_inc * xoff; float b = b0 + b_inc * xoff;
    float u = u0 + u_inc * xoff; float v = v0 + v_inc * xoff;
#ifdef OVERDRAW
    for (int x = startx; x <= endx; ++x) {
        if (check_depth(x, y, z)) {
            switch (get_pixel(x, y)) {
            case 0x00000000: pixel(x, y, 0xffff0000); break;
            case 0xffff0000: pixel(x, y, 0xff00ff00); break;
            case 0xff00ff00: pixel(x, y, 0xffffff00); break;
            case 0xffffff00: pixel(x, y, 0xff0000ff); break;
            case 0xff0000ff: pixel(x, y, 0xffff00ff); break;
            case 0xffff00ff: pixel(x, y, 0xff00ffff); break;
            case 0xff00ffff: pixel(x, y, 0xffffffff); break;
            }
            write_depth(x, y, z);
        }
        z += z_inc; r += r_inc; g += g_inc; b += b_inc;
    }
#else
    if (mtl && mtl->diffuse_texture.pixels) {
        for (int x = startx; x <= endx; ++x) {
            if (check_depth(x, y, z)) {
                const texture_t* t = &mtl->diffuse_texture;
                const int ui = dmin(dmax(0, u * t->width), t->width);
                const int vi = dmin(dmax(0, (1.f - v) * t->height), t->height);
                uint32_t c = t->pixels[vi * t->width + ui];
                pixel(x, y, color((c & 0x000000ff) * r, ((c & 0x0000ff00) >> 8) * g, ((c & 0x00ff0000) >> 16) * b, 255));
                write_depth(x, y, z);
            }
            z += z_inc; r += r_inc; g += g_inc;
            b += b_inc; u += u_inc; v += v_inc;
        }
    } else {
        for (int x = startx; x <= endx; ++x) {
            if (check_depth(x, y, z)) {
                pixel(x, y, color(r * 255.f, g * 255.f, b * 255.f, 255));
                write_depth(x, y, z);
            }
            z += z_inc; r += r_inc; g += g_inc; b += b_inc;
        }
    }
#endif
}

void line(int x0, int y0, int x1, int y1) {
    const int dx = abs(x1 - x0);
    const int xi = x0 < x1 ? 1 : -1;
    const int dy = -abs(y1 - y0);
    const int yi = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        check_pixel(x0, y0, color(255, 0, 0, 255));
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int err2 = 2 * err;
        if (err2 >= dy) {
            err += dy;
            x0 += xi;
        }
        if (err2 <= dx) {
            err += dx;
            y0 += yi;
        }
    }
}

void double_line(int x00, int x10, int y0, int x01, int x11, int y1) {
    const int dx0 = abs(x01 - x00);
    const int xi0 = x00 < x01 ? 1 : -1;
    const int dx1 = abs(x11 - x10);
    const int xi1 = x10 < x11 ? 1 : -1;
    const int dy = -abs(y1 - y0);
    const int yi = y0 < y1 ? 1 : -1;
    int err0 = dx0 + dy;
    int err1 = dx1 + dy;

    check_pixel(x00, y0, color(255, 0, 0, 255));
    check_pixel(x10, y0, color(255, 0, 0, 255));

    while (1) {
        if (y0 == y1 && x00 == x01 && x10 == x11) {
            break;
        }
        const int err02 = 2 * err0;
        const int err12 = 2 * err1;
        if (err02 >= dy) {
            err0 += dy;
            if (x00 != x01) {
                x00 += xi0;
                check_pixel(x00, y0, color(255, 0, 0, 255));
            }
        }
        if (err12 >= dy) {
            err1 += dy;
            if (x10 != x11) {
                x10 += xi1;
                check_pixel(x10, y0, color(255, 0, 0, 255));
            }
        }
        if (err02 <= dx0 && err12 <= dx1) {
            err0 += dx0;
            err1 += dx1;
            y0 += yi;
            check_pixel(x00, y0, color(255, 0, 0, 255));
            check_pixel(x10, y0, color(255, 0, 0, 255));
        }
    }
}

static inline void triangle_half(
    const point_t p[3], int mode,
    const int b[4], int c, mtl_t* mtl
    ) {
    const int x00 = (int)p[b[1]].x; const int x01 = (int)p[b[0]].x;
    const int x10 = (int)p[b[3]].x; const int x11 = (int)p[b[2]].x;
    const int y0 = (int)p[0].y < 0 ? 0 : ((int)p[0].y > YRES ? YRES : (int)p[0].y);
    const int y1 = (int)p[2].y <= 0 ? -1 : ((int)p[2].y >= YRES ? YRES - 1 : (int)p[2].y);
    if (y0 == YRES || y1 == -1) {
        return;
    }
    const float z_min = fmin(p[0].z, fmin(p[1].z, p[2].z)); const float z_max = fmax(p[0].z, fmax(p[1].z, p[2].z));
    const float r_min = fmin(p[0].r, fmin(p[1].r, p[2].r)); const float r_max = fmax(p[0].r, fmax(p[1].r, p[2].r));
    const float g_min = fmin(p[0].g, fmin(p[1].g, p[2].g)); const float g_max = fmax(p[0].g, fmax(p[1].g, p[2].g));
    const float b_min = fmin(p[0].b, fmin(p[1].b, p[2].b)); const float b_max = fmax(p[0].b, fmax(p[1].b, p[2].b));
    const float u_min = fmin(p[0].u, fmin(p[1].u, p[2].u)); const float u_max = fmax(p[0].u, fmax(p[1].u, p[2].u));
    const float v_min = fmin(p[0].v, fmin(p[1].v, p[2].v)); const float v_max = fmax(p[0].v, fmax(p[1].v, p[2].v));
    const float y_diff0 = 1.f / (p[b[0]].y - p[b[1]].y) * mode; const float y_diff1 = 1.f / (p[b[2]].y - p[b[3]].y) * mode;
    const float z_inc0 = (p[b[0]].z - p[b[1]].z) * y_diff0; const float z_inc1 = (p[b[2]].z - p[b[3]].z) * y_diff1;
    const float r_inc0 = (p[b[0]].r - p[b[1]].r) * y_diff0; const float r_inc1 = (p[b[2]].r - p[b[3]].r) * y_diff1;
    const float g_inc0 = (p[b[0]].g - p[b[1]].g) * y_diff0; const float g_inc1 = (p[b[2]].g - p[b[3]].g) * y_diff1;
    const float b_inc0 = (p[b[0]].b - p[b[1]].b) * y_diff0; const float b_inc1 = (p[b[2]].b - p[b[3]].b) * y_diff1;
    const float u_inc0 = (p[b[0]].u - p[b[1]].u) * y_diff0; const float u_inc1 = (p[b[2]].u - p[b[3]].u) * y_diff1;
    const float v_inc0 = (p[b[0]].v - p[b[1]].v) * y_diff0; const float v_inc1 = (p[b[2]].v - p[b[3]].v) * y_diff1;
    int yoff = mode > 0 ? abs(y0 - (int)p[0].y) : abs(y1 - (int)p[2].y);
    float z0 = p[c].z + z_inc0 * yoff; float z1 = p[c].z + z_inc1 * yoff;
    float r0 = p[c].r + r_inc0 * yoff; float r1 = p[c].r + r_inc1 * yoff;
    float g0 = p[c].g + g_inc0 * yoff; float g1 = p[c].g + g_inc1 * yoff;
    float b0 = p[c].b + b_inc0 * yoff; float b1 = p[c].b + b_inc1 * yoff;
    float u0 = p[c].u + u_inc0 * yoff; float u1 = p[c].u + u_inc1 * yoff;
    float v0 = p[c].v + v_inc0 * yoff; float v1 = p[c].v + v_inc1 * yoff;

    const int beginy = p[0].y; const int endy = p[2].y;
    const int dx0 = abs(x01 - x00); const int x0_inc = x00 < x01 ? 1 : -1;
    const int dx1 = abs(x11 - x10); const int x1_inc = x10 < x11 ? 1 : -1;
    const int dy = -abs(endy - beginy); const int y_inc = (int)beginy < (int)endy ? 1 : -1;
    int err0 = dx0 + dy; int err1 = dx1 + dy;
    int x0 = x00; int x1 = x10; int y = beginy;

    //line(x00, p[0].y, x01, p[a].y);
    //line(x10, p[0].y, x11, p[a].y);
    //double_line(x00, x10, p[0].y, x01, x11, p[a].y);

#define YITERATE() \
    const int err02 = 2 * err0; const int err12 = 2 * err1; \
    {\
        if (y == endy && x0 == x01 && x1 == x11) {\
            break;\
        }\
        if (err02 >= dy) {\
            err0 += dy;\
            if (x0 != x01) {\
                x0 += x0_inc;\
            }\
        }\
        if (err12 >= dy) {\
            err1 += dy;\
            if (x1 != x11) {\
                x1 += x1_inc;\
            }\
        }\
    }

    //check_pixel(x0, y, color(255, 0, 0, 255));
    //check_pixel(x1, y, color(255, 0, 0, 255));
    if (x01 < x11) {
        while (1) {
            YITERATE()
            if (err02 <= dx0 && err12 <= dx1) {
                err0 += dx0; err1 += dx1; y += y_inc;
                if (y >= 0 && y < YRES) {
                    triangle_span(y, x0, x1, z0, z1,
                        r0, r1, g0, g1, b0, b1,
                        u0, u1, v0, v1, mtl);
                    z0 += z_inc0; z1 += z_inc1; r0 += r_inc0; r1 += r_inc1;
                    g0 += g_inc0; g1 += g_inc1; b0 += b_inc0; b1 += b_inc1;
                    u0 += u_inc0; u1 += u_inc1; v0 += v_inc0; v1 += v_inc1;
                    /*const float bz0 = fmax(z1, z_min); const float bz1 = fmin(z0, z_max);
                    const float br0 = fmax(r1, r_min); const float br1 = fmin(r0, r_max);
                    const float bg0 = fmax(g1, g_min); const float bg1 = fmin(g0, g_max);
                    const float bb0 = fmax(b1, b_min); const float bb1 = fmin(b0, b_max);
                    const float bu0 = fmax(u1, u_min); const float bu1 = fmin(u0, u_max);
                    const float bv0 = fmax(v1, v_min); const float bv1 = fmin(v0, v_max);
                    triangle_span(y, x0, x1, bz0, bz1,
                        br0, br1, bg0, bg1, bb0, bb1,
                        bu0, bu1, bv0, bv1, mtl);
                    z0 += z_inc0; z1 += z_inc1; r0 += r_inc0; r1 += r_inc1;
                    g0 += g_inc0; g1 += g_inc1; b0 += b_inc0; b1 += b_inc1;
                    u0 += u_inc0; u1 += u_inc1; v0 += v_inc0; v1 += v_inc1;*/
                }
            }
        }
    } else {
        while (1) {
            YITERATE()
            if (err02 <= dx0 && err12 <= dx1) {
                err0 += dx0; err1 += dx1; y += y_inc;
                if (y >= 0 && y < YRES) {
                    triangle_span(y, x1, x0, z1, z0,
                        r1, r0, g1, g0, b1, b0,
                        u1, u0, v1, v0, mtl);
                    z0 += z_inc0; z1 += z_inc1; r0 += r_inc0; r1 += r_inc1;
                    g0 += g_inc0; g1 += g_inc1; b0 += b_inc0; b1 += b_inc1;
                    u0 += u_inc0; u1 += u_inc1; v0 += v_inc0; v1 += v_inc1;
                    /*const float bz0 = fmax(z1, z_min); const float bz1 = fmin(z0, z_max);
                    const float br0 = fmax(r1, r_min); const float br1 = fmin(r0, r_max);
                    const float bg0 = fmax(g1, g_min); const float bg1 = fmin(g0, g_max);
                    const float bb0 = fmax(b1, b_min); const float bb1 = fmin(b0, b_max);
                    const float bu0 = fmax(u1, u_min); const float bu1 = fmin(u0, u_max);
                    const float bv0 = fmax(v1, v_min); const float bv1 = fmin(v0, v_max);
                    triangle_span(y, x1, x0, bz0, bz1,
                        br0, br1, bg0, bg1, bb0, bb1,
                        bu0, bu1, bv0, bv1, mtl);
                    z0 += z_inc0; z1 += z_inc1; r0 += r_inc0; r1 += r_inc1;
                    g0 += g_inc0; g1 += g_inc1; b0 += b_inc0; b1 += b_inc1;
                    u0 += u_inc0; u1 += u_inc1; v0 += v_inc0; v1 += v_inc1;*/
                }
            }
        }
    }

    /*int beginy = mode > 0 ? y0 : y1;
    int endy = mode > 0 ? y1 : y0;
    if (x_inc0 < x_inc1) {
        for (int y = beginy; mode > 0 ? (y < endy) : (y >= endy); y += mode) {
            float bz0 = fmax(z0, z_min); float bz1 = fmin(z1, z_max);
            float br0 = fmax(r0, r_min); float br1 = fmin(r1, r_max);
            float bg0 = fmax(g0, g_min); float bg1 = fmin(g1, g_max);
            float bb0 = fmax(b0, b_min); float bb1 = fmin(b1, b_max);
            float bu0 = fmax(u0, u_min); float bu1 = fmin(u1, u_max);
            float bv0 = fmax(v0, v_min); float bv1 = fmin(v1, v_max);
            triangle_span(y, x0, x1, bz0, bz1,
                br0, br1, bg0, bg1, bb0, bb1,
                bu0, bu1, bv0, bv1, mtl);
            z0 += z_inc0; z1 += z_inc1; r0 += r_inc0; r1 += r_inc1;
            g0 += g_inc0; g1 += g_inc1; b0 += b_inc0; b1 += b_inc1;
            u0 += u_inc0; u1 += u_inc1; v0 += v_inc0; v1 += v_inc1;
        }
    } else {
        for (int y = beginy; mode > 0 ? (y < endy) : (y >= endy); y += mode) {
            float bz0 = fmax(z1, z_min); float bz1 = fmin(z0, z_max);
            float br0 = fmax(r1, r_min); float br1 = fmin(r0, r_max);
            float bg0 = fmax(g1, g_min); float bg1 = fmin(g0, g_max);
            float bb0 = fmax(b1, b_min); float bb1 = fmin(b0, b_max);
            float bu0 = fmax(u1, u_min); float bu1 = fmin(u0, u_max);
            float bv0 = fmax(v1, v_min); float bv1 = fmin(v0, v_max);
            triangle_span(y, x0, x1, bz0, bz1,
                br0, br1, bg0, bg1, bb0, bb1,
                bu0, bu1, bv0, bv1, mtl);
            z0 += z_inc0; z1 += z_inc1; r0 += r_inc0; r1 += r_inc1;
            g0 += g_inc0; g1 += g_inc1; b0 += b_inc0; b1 += b_inc1;
            u0 += u_inc0; u1 += u_inc1; v0 += v_inc0; v1 += v_inc1;
        }
    }*/
}

static inline void swap(point_t* p0, point_t* p1) {
    point_t t = *p0;
    *p0 = *p1;
    *p1 = t;
}

#define TRIANGLE_TOP(P) triangle_half(P, 1, (int[4]){1, 0, 2, 0}, 0, mtl)
#define TRIANGLE_BOT(P) triangle_half(P, -1, (int[4]){2, 0, 2, 1}, 2, mtl)

void draw_triangle(point_t p0, point_t p1, point_t p2, mtl_t* mtl) {
    point_t p[3] = {p0, p1, p2};
    if (p[0].y > p[1].y) { swap(&p[0], &p[1]); }
    if (p[1].y > p[2].y) { swap(&p[1], &p[2]); }
    if (p[0].y > p[1].y) { swap(&p[0], &p[1]); }
    if (p[1].y == p[2].y) {
        TRIANGLE_TOP(p);
    } else if (p[0].y == p[1].y) {
        TRIANGLE_BOT(p);
    } else {
        point_t p3;
        const float factor = (p[1].y - p[0].y) / (p[2].y - p[0].y);
        p3.x = p[0].x + factor * (p[2].x - p[0].x);
        p3.y = p[1].y;
        p3.z = p[0].z + factor * (p[2].z - p[0].z);
        p3.r = p[0].r + factor * (p[2].r - p[0].r);
        p3.g = p[0].g + factor * (p[2].g - p[0].g);
        p3.b = p[0].b + factor * (p[2].b - p[0].b);
        p3.u = p[0].u + factor * (p[2].u - p[0].u);
        p3.v = p[0].v + factor * (p[2].v - p[0].v);
        point_t t0[3] = {p[0], p[1], p3};
        point_t t1[3] = {p[1], p3, p[2]};
        TRIANGLE_TOP(t0);
        TRIANGLE_BOT(t1);
    }
}