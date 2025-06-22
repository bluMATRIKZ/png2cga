/*
png2cga - a PNG to my 3-bit color image format converter
Copyright (C) 2025 Connor Thomson

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or 
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
    int r, g, b;
} RGB;

typedef struct {
    double l, a, b;
} Lab;

RGB palette_rgb[8] = {
    {0, 0, 0},       // Black
    {255, 0, 0},     // Red
    {0, 255, 0},     // Green
    {0, 0, 255},     // Blue
    {255, 255, 0},   // Yellow
    {255, 0, 255},   // Magenta
    {0, 255, 255},   // Cyan
    {255, 255, 255}  // White
};

Lab palette_lab[8];

Lab rgb_to_lab(RGB c) {
    double r = c.r / 255.0, g = c.g / 255.0, b = c.b / 255.0;
    double x = r * .4124 + g * .3576 + b * .1805;
    double y = r * .2126 + g * .7152 + b * .0722;
    double z = r * .0193 + g * .1192 + b * .9505;
    x /= 0.95047; y /= 1.00000; z /= 1.08883;
    x = x > .008856 ? pow(x, 1.0/3) : (7.787 * x + 16.0/116);
    y = y > .008856 ? pow(y, 1.0/3) : (7.787 * y + 16.0/116);
    z = z > .008856 ? pow(z, 1.0/3) : (7.787 * z + 16.0/116);
    return (Lab){(116 * y) - 16, 500 * (x - y), 200 * (y - z)};
}

int closest_index_lab(RGB target) {
    Lab t = rgb_to_lab(target);
    double best = 1e9;
    int best_i = 0;
    for (int i = 0; i < 8; ++i) {
        double dl = t.l - palette_lab[i].l;
        double da = t.a - palette_lab[i].a;
        double db = t.b - palette_lab[i].b;
        double dist = dl*dl + da*da + db*db;
        if (dist < best) {
            best = dist;
            best_i = i;
        }
    }
    return best_i;
}

int clamp(int x) { return x < 0 ? 0 : x > 255 ? 255 : x; }

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s input.png output.cga\n", argv[0]);
        return 1;
    }

    for (int i = 0; i < 8; ++i)
        palette_lab[i] = rgb_to_lab(palette_rgb[i]);

    int w, h, ch;
    unsigned char *img = stbi_load(argv[1], &w, &h, &ch, 3);
    if (!img) { fprintf(stderr, "Failed to load image\n"); return 1; }

    RGB *buffer = malloc(w * h * sizeof(RGB));
    int count[8] = {0};

    for (int i = 0; i < w * h; ++i)
        buffer[i] = (RGB){ img[i*3], img[i*3+1], img[i*3+2] };

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = y * w + x;
            RGB old = buffer[idx];
            int pi = closest_index_lab(old);
            RGB new = palette_rgb[pi];
            buffer[idx] = new;
            count[pi]++;
            int err_r = old.r - new.r;
            int err_g = old.g - new.g;
            int err_b = old.b - new.b;

            int offsets[4][2] = {{1,0},{-1,1},{0,1},{1,1}};
            float weights[4] = {7/16.0, 3/16.0, 5/16.0, 1/16.0};
            for (int k = 0; k < 4; ++k) {
                int nx = x + offsets[k][0];
                int ny = y + offsets[k][1];
                if (nx < 0 || nx >= w || ny >= h) continue;
                int nidx = ny * w + nx;
                buffer[nidx].r = clamp(buffer[nidx].r + err_r * weights[k]);
                buffer[nidx].g = clamp(buffer[nidx].g + err_g * weights[k]);
                buffer[nidx].b = clamp(buffer[nidx].b + err_b * weights[k]);
            }
        }
    }

    FILE *out = fopen(argv[2], "w");
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int idx = closest_index_lab(buffer[y*w + x]) + 1;
            fputc('0' + idx, out);
        }
        fputs(";\n", out);
    }
    fclose(out);
    free(buffer);
    stbi_image_free(img);

    printf("=== Conversion Complete ===\n");
    for (int i = 0; i < 8; ++i)
        printf("%d (%s): %d pixels\n", i+1,
            (const char*[]){"Black","Red","Green","Blue","Yellow","Magenta","Cyan","White"}[i],
            count[i]);
    return 0;
}
