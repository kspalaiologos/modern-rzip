/*
   Copyright (C) 2022 Kamila Szewczyk
   Copyright (C) 1999 Phil Karn

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _REED_SOLOMON_H
#define _REED_SOLOMON_H

/* Type that stores an element of the Galois field. */
typedef int gf;

/* GF(2**m) generated from the irreducible CCSDS field generator polynomial
   p(X) = x^8 + x^7 + x^2 + x + 1. alpha=2 is the primitive element of GF(2**m) */

/* index->polynomial form conversion table */
static gf Alpha_to[256] = {
    1,   2,   4,   8,   16,  32,  64,  128, 135, 137, 149, 173, 221, 61,  122, 244, 111, 222, 59,  118, 236, 95,
    190, 251, 113, 226, 67,  134, 139, 145, 165, 205, 29,  58,  116, 232, 87,  174, 219, 49,  98,  196, 15,  30,
    60,  120, 240, 103, 206, 27,  54,  108, 216, 55,  110, 220, 63,  126, 252, 127, 254, 123, 246, 107, 214, 43,
    86,  172, 223, 57,  114, 228, 79,  158, 187, 241, 101, 202, 19,  38,  76,  152, 183, 233, 85,  170, 211, 33,
    66,  132, 143, 153, 181, 237, 93,  186, 243, 97,  194, 3,   6,   12,  24,  48,  96,  192, 7,   14,  28,  56,
    112, 224, 71,  142, 155, 177, 229, 77,  154, 179, 225, 69,  138, 147, 161, 197, 13,  26,  52,  104, 208, 39,
    78,  156, 191, 249, 117, 234, 83,  166, 203, 17,  34,  68,  136, 151, 169, 213, 45,  90,  180, 239, 89,  178,
    227, 65,  130, 131, 129, 133, 141, 157, 189, 253, 125, 250, 115, 230, 75,  150, 171, 209, 37,  74,  148, 175,
    217, 53,  106, 212, 47,  94,  188, 255, 121, 242, 99,  198, 11,  22,  44,  88,  176, 231, 73,  146, 163, 193,
    5,   10,  20,  40,  80,  160, 199, 9,   18,  36,  72,  144, 167, 201, 21,  42,  84,  168, 215, 41,  82,  164,
    207, 25,  50,  100, 200, 23,  46,  92,  184, 247, 105, 210, 35,  70,  140, 159, 185, 245, 109, 218, 51,  102,
    204, 31,  62,  124, 248, 119, 238, 91,  182, 235, 81,  162, 195, 0
};

/* Polynomial->index form conversion table */
static gf Index_of[256] = {
    255, 0,   1,   99,  2,   198, 100, 106, 3,   205, 199, 188, 101, 126, 107, 42,  4,   141, 206, 78,  200, 212,
    189, 225, 102, 221, 127, 49,  108, 32,  43,  243, 5,   87,  142, 232, 207, 172, 79,  131, 201, 217, 213, 65,
    190, 148, 226, 180, 103, 39,  222, 240, 128, 177, 50,  53,  109, 69,  33,  18,  44,  13,  244, 56,  6,   155,
    88,  26,  143, 121, 233, 112, 208, 194, 173, 168, 80,  117, 132, 72,  202, 252, 218, 138, 214, 84,  66,  36,
    191, 152, 149, 249, 227, 94,  181, 21,  104, 97,  40,  186, 223, 76,  241, 47,  129, 230, 178, 63,  51,  238,
    54,  16,  110, 24,  70,  166, 34,  136, 19,  247, 45,  184, 14,  61,  245, 164, 57,  59,  7,   158, 156, 157,
    89,  159, 27,  8,   144, 9,   122, 28,  234, 160, 113, 90,  209, 29,  195, 123, 174, 10,  169, 145, 81,  91,
    118, 114, 133, 161, 73,  235, 203, 124, 253, 196, 219, 30,  139, 210, 215, 146, 85,  170, 67,  11,  37,  175,
    192, 115, 153, 119, 150, 92,  250, 82,  228, 236, 95,  74,  182, 162, 22,  134, 105, 197, 98,  254, 41,  125,
    187, 204, 224, 211, 77,  140, 242, 31,  48,  220, 130, 171, 231, 86,  179, 147, 64,  216, 52,  176, 239, 38,
    55,  12,  17,  68,  111, 120, 25,  154, 71,  116, 167, 193, 35,  83,  137, 251, 20,  93,  248, 151, 46,  75,
    185, 96,  15,  237, 62,  229, 246, 135, 165, 23,  58,  163, 60,  183
};

/* Generator polynomial g(x) in index form */
static gf Gg[33] = { 0, 249, 59, 66, 4,   43, 126, 251, 97,  30,  3,  213, 50, 66, 170, 5, 24,
                     5, 170, 66, 50, 213, 3,  30,  97,  251, 126, 43, 4,   66, 59, 249, 0 };

#define min(a, b) ((a) < (b) ? (a) : (b))

#define CLEAR(a, n)                                      \
    {                                                    \
        for (int ci = (n)-1; ci >= 0; ci--) (a)[ci] = 0; \
    }

#define COPY(a, b, n)                                          \
    {                                                          \
        for (int ci = (n)-1; ci >= 0; ci--) (a)[ci] = (b)[ci]; \
    }

#define COPYDOWN(a, b, n)                                      \
    {                                                          \
        for (int ci = (n)-1; ci >= 0; ci--) (a)[ci] = (b)[ci]; \
    }

/* Conversion lookup tables from conventional alpha to Berlekamp's
   dual-basis representation.

   taltab[] -- convert conventional to dual basis
   tal1tab[] -- convert dual basis to conventional  */

static const unsigned char
    taltab[256] = { 0,   123, 175, 212, 153, 226, 54,  77,  250, 129, 85,  46,  99,  24,  204, 183, 134, 253, 41,  82,
                    31,  100, 176, 203, 124, 7,   211, 168, 229, 158, 74,  49,  236, 151, 67,  56,  117, 14,  218, 161,
                    22,  109, 185, 194, 143, 244, 32,  91,  106, 17,  197, 190, 243, 136, 92,  39,  144, 235, 63,  68,
                    9,   114, 166, 221, 239, 148, 64,  59,  118, 13,  217, 162, 21,  110, 186, 193, 140, 247, 35,  88,
                    105, 18,  198, 189, 240, 139, 95,  36,  147, 232, 60,  71,  10,  113, 165, 222, 3,   120, 172, 215,
                    154, 225, 53,  78,  249, 130, 86,  45,  96,  27,  207, 180, 133, 254, 42,  81,  28,  103, 179, 200,
                    127, 4,   208, 171, 230, 157, 73,  50,  141, 246, 34,  89,  20,  111, 187, 192, 119, 12,  216, 163,
                    238, 149, 65,  58,  11,  112, 164, 223, 146, 233, 61,  70,  241, 138, 94,  37,  104, 19,  199, 188,
                    97,  26,  206, 181, 248, 131, 87,  44,  155, 224, 52,  79,  2,   121, 173, 214, 231, 156, 72,  51,
                    126, 5,   209, 170, 29,  102, 178, 201, 132, 255, 43,  80,  98,  25,  205, 182, 251, 128, 84,  47,
                    152, 227, 55,  76,  1,   122, 174, 213, 228, 159, 75,  48,  125, 6,   210, 169, 30,  101, 177, 202,
                    135, 252, 40,  83,  142, 245, 33,  90,  23,  108, 184, 195, 116, 15,  219, 160, 237, 150, 66,  57,
                    8,   115, 167, 220, 145, 234, 62,  69,  242, 137, 93,  38,  107, 16,  196, 191 },
    tal1tab[256] = {
        0,   204, 172, 96,  121, 181, 213, 25,  240, 60,  92,  144, 137, 69,  37,  233, 253, 49,  81,  157, 132, 72,
        40,  228, 13,  193, 161, 109, 116, 184, 216, 20,  46,  226, 130, 78,  87,  155, 251, 55,  222, 18,  114, 190,
        167, 107, 11,  199, 211, 31,  127, 179, 170, 102, 6,   202, 35,  239, 143, 67,  90,  150, 246, 58,  66,  142,
        238, 34,  59,  247, 151, 91,  178, 126, 30,  210, 203, 7,   103, 171, 191, 115, 19,  223, 198, 10,  106, 166,
        79,  131, 227, 47,  54,  250, 154, 86,  108, 160, 192, 12,  21,  217, 185, 117, 156, 80,  48,  252, 229, 41,
        73,  133, 145, 93,  61,  241, 232, 36,  68,  136, 97,  173, 205, 1,   24,  212, 180, 120, 197, 9,   105, 165,
        188, 112, 16,  220, 53,  249, 153, 85,  76,  128, 224, 44,  56,  244, 148, 88,  65,  141, 237, 33,  200, 4,
        100, 168, 177, 125, 29,  209, 235, 39,  71,  139, 146, 94,  62,  242, 27,  215, 183, 123, 98,  174, 206, 2,
        22,  218, 186, 118, 111, 163, 195, 15,  230, 42,  74,  134, 159, 83,  51,  255, 135, 75,  43,  231, 254, 50,
        82,  158, 119, 187, 219, 23,  14,  194, 162, 110, 122, 182, 214, 26,  3,   207, 175, 99,  138, 70,  38,  234,
        243, 63,  95,  147, 169, 101, 5,   201, 208, 28,  124, 176, 89,  149, 245, 57,  32,  236, 140, 64,  84,  152,
        248, 52,  45,  225, 129, 77,  164, 104, 8,   196, 221, 17,  113, 189,
    };

static int rse32(uint8_t data[223], uint8_t bb[32]) {
    CLEAR(bb, 32);

    /* Convert to conventional basis */
    for (int i = 0; i < 223; i++) data[i] = tal1tab[data[i]];

    for (int i = 222; i >= 0; i--) {
        gf feedback = Index_of[data[i] ^ bb[31]];
        if (feedback != 255) { /* feedback term is non-zero */
            for (int j = 31; j > 0; j--)
                if (Gg[j] != 255)
                    bb[j] = bb[j - 1] ^ Alpha_to[(Gg[j] + feedback) % 255];
                else
                    bb[j] = bb[j - 1];
            bb[0] = Alpha_to[(Gg[0] + feedback) % 255];
        } else { /* feedback term is zero. encoder becomes a
                    single-byte shifter */
            for (int j = 31; j > 0; j--) bb[j] = bb[j - 1];
            bb[0] = 0;
        }
    }

    /* Convert to l-basis */
    for (int i = 0; i < 255; i++) data[i] = taltab[data[i]];

    return 0;
}

int rsd32(uint8_t data[255], int eras_pos[32], int no_eras) {
    int deg_lambda, el, deg_omega;
    int i, j, r, k;
    gf u, q, tmp, num1, num2, den, discr_r;
    gf lambda[33], s[33]; /* Err+Eras Locator poly & and syndrome poly */
    gf b[33], t[33], omega[33];
    gf root[32], reg[33], loc[32];
    int syn_error, count;

    /* Convert to conventional basis */
    for (i = 0; i < 255; i++) data[i] = tal1tab[data[i]];

    /* form the syndromes; i.e., evaluate data(x) at roots of g(x)
       namely @**(112+i)*11, i = 0, ... ,(255-223-1) */
    for (i = 1; i <= 32; i++) {
        s[i] = data[0];
    }
    for (j = 1; j < 255; j++) {
        if (data[j] == 0) continue;
        tmp = Index_of[data[j]];

        for (i = 1; i <= 32; i++) s[i] ^= Alpha_to[(tmp + (111 + i) * 11 * j) % 255];
    }

    /* Convert syndromes to index form, checking for nonzero condition */
    syn_error = 0;
    for (i = 1; i <= 32; i++) {
        syn_error |= s[i];
        s[i] = Index_of[s[i]];
    }

    if (!syn_error) {
        /* if syndrome is zero, data[] is a codeword and there are no
           errors to correct. So return data[] unmodified */
        count = 0;
        goto finish;
    }
    CLEAR(&lambda[1], 32);
    lambda[0] = 1;

    if (no_eras > 0) {
        /* Init lambda to be the erasure locator polynomial */
        lambda[1] = Alpha_to[(11 * eras_pos[0]) % 255];
        for (i = 1; i < no_eras; i++) {
            u = (11 * eras_pos[i]) % 255;
            for (j = i + 1; j > 0; j--) {
                tmp = Index_of[lambda[j - 1]];
                if (tmp != 255) lambda[j] ^= Alpha_to[(u + tmp) % 255];
            }
        }
    }
    for (i = 0; i < 33; i++) b[i] = Index_of[lambda[i]];

    /* Begin Berlekamp-Massey algorithm to determine error+erasure
       locator polynomial */
    r = no_eras;
    el = no_eras;
    while (++r <= 32) { /* r is the step number */
        /* Compute discrepancy at the r-th step in poly-form */
        discr_r = 0;
        for (i = 0; i < r; i++) {
            if ((lambda[i] != 0) && (s[r - i] != 255)) {
                discr_r ^= Alpha_to[(Index_of[lambda[i]] + s[r - i]) % 255];
            }
        }
        discr_r = Index_of[discr_r]; /* Index form */
        if (discr_r == 255) {
            COPYDOWN(&b[1], b, 32);
            b[0] = 255;
        } else {
            t[0] = lambda[0];
            for (i = 0; i < 32; i++) {
                if (b[i] != 255)
                    t[i + 1] = lambda[i + 1] ^ Alpha_to[(discr_r + b[i]) % 255];
                else
                    t[i + 1] = lambda[i + 1];
            }
            if (2 * el <= r + no_eras - 1) {
                el = r + no_eras - el;
                for (i = 0; i <= 32; i++) b[i] = (lambda[i] == 0) ? 255 : (Index_of[lambda[i]] - discr_r + 255) % 255;
            } else {
                COPYDOWN(&b[1], b, 32);
                b[0] = 255;
            }
            COPY(lambda, t, 33);
        }
    }

    /* Convert lambda to index form and compute deg(lambda(x)) */
    deg_lambda = 0;
    for (i = 0; i < 33; i++) {
        lambda[i] = Index_of[lambda[i]];
        if (lambda[i] != 255) deg_lambda = i;
    }

    /* Find roots of the error+erasure locator polynomial by Chien
       Search */
    COPY(&reg[1], &lambda[1], 32);
    count = 0; /* Number of roots of lambda(x) */
    for (i = 1, k = 139; i <= 255; i++, k = (k + 139) % 255) {
        q = 1;
        for (j = deg_lambda; j > 0; j--) {
            if (reg[j] != 255) {
                reg[j] = (reg[j] + j) % 255;
                q ^= Alpha_to[reg[j]];
            }
        }
        if (q != 0) continue;
        /* store root (index-form) and error location number */
        root[count] = i;
        loc[count] = k;
        if (++count == deg_lambda) break;
    }
    if (deg_lambda != count) {
        /* deg(lambda) unequal to number of roots => uncorrectable
           error detected */
        count = -1;
        goto finish;
    }
    /* Compute err+eras evaluator poly omega(x) = s(x)*lambda(x) (modulo
       x**(255-223)). in index form. Also find deg(omega). */
    deg_omega = 0;
    for (i = 0; i < 32; i++) {
        tmp = 0;
        j = (deg_lambda < i) ? deg_lambda : i;
        for (; j >= 0; j--) {
            if ((s[i + 1 - j] != 255) && (lambda[j] != 255)) tmp ^= Alpha_to[(s[i + 1 - j] + lambda[j]) % 255];
        }
        if (tmp != 0) deg_omega = i;
        omega[i] = Index_of[tmp];
    }
    omega[32] = 255;

    /* Compute error values in poly-form. num1 = omega(inv(X(l))), num2 =
       inv(X(l))**(112-1) and den = lambda_pr(inv(X(l))) all in poly-form */
    for (j = count - 1; j >= 0; j--) {
        num1 = 0;
        for (i = deg_omega; i >= 0; i--) {
            if (omega[i] != 255) num1 ^= Alpha_to[(omega[i] + i * root[j]) % 255];
        }
        num2 = Alpha_to[(root[j] * 111) % 255];
        den = 0;

        /* lambda[i+1] for i even is the formal derivative lambda_pr of lambda[i] */
        for (i = min(deg_lambda, 32 - 1) & ~1; i >= 0; i -= 2) {
            if (lambda[i + 1] != 255) den ^= Alpha_to[(lambda[i + 1] + i * root[j]) % 255];
        }
        if (den == 0) {
            /* Convert to dual-basis */
            count = -1;
            goto finish;
        }
        /* Apply error to data */
        if (num1 != 0) {
            data[loc[j]] ^= Alpha_to[(Index_of[num1] + Index_of[num2] + 255 - Index_of[den]) % 255];
        }
    }
finish:
    /* Convert to dual- basis */
    for (i = 0; i < 255; i++) data[i] = taltab[data[i]];

    if (eras_pos != NULL) {
        for (i = 0; i < count; i++) {
            if (eras_pos != NULL) eras_pos[i] = loc[i];
        }
    }
    return count;
}

// Scatter data over the array.
void distribute(uint8_t * src, uint8_t * dst, int rows, int cols, int reverse) {
    int totalBytes = rows * cols;
    int boundary = totalBytes;
    int offset = 0;

    if (!reverse) {
        while (totalBytes--) {
            dst[offset] = *src++;
            offset += BLK_LEN;
            if (offset >= boundary) offset -= (boundary - 1);
        }
    } else {
        while (totalBytes--) {
            *dst++ = src[offset];
            offset += BLK_LEN;
            if (offset >= boundary) offset -= (boundary - 1);
        }
    }
}

#endif
