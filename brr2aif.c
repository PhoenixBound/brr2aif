#include <assert.h>
#include <float.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/AIFF/Docs/AIFF-1.3.pdf
#define BRR2AIF_TEST_SOFT_EXTENDED false

static bool write_aiff_id(FILE *out, const char *id) {
    return fwrite(id, 1, 4, out) != 4;
}

static bool write_aiff_pstring(FILE *out, const char *str) {
    size_t len = strlen(str);
    size_t actual_len = len % 2 != 0 ? len : len + 1;
    assert(len < 256);
    return fputc((int)len, out) == -1 || fwrite(str, 1, actual_len, out) != actual_len;
}

static bool write_aiff_uchar(FILE *out, uint8_t i) {
    return fputc(i, out) == -1;
}

static bool write_aiff_char(FILE *out, int8_t i) {
    return write_aiff_uchar(out, (uint8_t)i);
}

static bool write_aiff_ushort(FILE *out, uint16_t i) {
    uint8_t data[2];
    data[0] = (uint8_t)(i >> 8);
    data[1] = (uint8_t)i;
    return fwrite(data, 1, 2, out) != 2;
}

static bool write_aiff_short(FILE *out, int16_t i) {
    return write_aiff_ushort(out, (uint16_t)i);
}

static bool write_aiff_ulong(FILE *out, uint32_t i) {
    uint8_t data[4];
    data[0] = (uint8_t)(i >> 24);
    data[1] = (uint8_t)(i >> 16);
    data[2] = (uint8_t)(i >> 8);
    data[3] = (uint8_t)(i);
    return fwrite(data, 1, 4, out) != 4;
}

static int write_aiff_long(FILE *out, int32_t i) {
    return write_aiff_ulong(out, (uint32_t)i);
}

static bool write_aiff_extended(FILE *out, uint32_t i) {
    uint8_t data[sizeof(long double) > 10 ? sizeof(long double) : 10];
    
    if (LDBL_EPSILON == 1.0842021724855044340e-19L && !BRR2AIF_TEST_SOFT_EXTENDED) {
        long double d = i;
        memcpy(data, &d, sizeof(d));
        if (data[0] == 0) {
            uint8_t temp;
            for (int j = 0, k = 9; j < k; ++j, --k) {
                temp = data[j];
                data[j] = data[k];
                data[k] = temp;
            }
        }
    } else {
        // It seems to behave the same way as the other code, from limited testing.
        uint64_t mantissa = i;
        uint16_t exponent = 16383 + 63;
        assert(i != 0);

        while (!(mantissa & (UINT64_C(1) << 63))) {
            mantissa <<= 1;
            --exponent;
        }

        data[0] = (uint8_t)(exponent >> 8) & 0x7F;
        data[1] = (uint8_t)exponent;
        data[2] = (uint8_t)(mantissa >> 56);
        data[3] = (uint8_t)(mantissa >> 48);
        data[4] = (uint8_t)(mantissa >> 40);
        data[5] = (uint8_t)(mantissa >> 32);
        data[6] = (uint8_t)(mantissa >> 24);
        data[7] = (uint8_t)(mantissa >> 16);
        data[8] = (uint8_t)(mantissa >> 8);
        data[9] = (uint8_t)mantissa;
    }
    
    return fwrite(data, 1, 10, out) != 10;
}

static bool write_aiff(FILE *out, int32_t loop_point, uint32_t sample_length,
               int16_t const sample_data[static sample_length], uint32_t rate) {
    int32_t form_size = 0;
    form_size += 4; // AIFF ID
    form_size += 8 + 18; // Common chunk
    if (loop_point != -1) {
        form_size += 8 + 18; // Marker chunk
        form_size += 8 + 20; // Instrument chunk
    }
    form_size += 8 + 8; // Sound data chunk
    form_size += sample_length * 2; // ...and the sound data inside it

    if (write_aiff_id(out, "FORM"))
        return true;
    if (write_aiff_long(out, form_size))
        return true;
    if (write_aiff_id(out, "AIFF"))
        return true;
    
    // Let's start with the common chunk
    if (write_aiff_id(out, "COMM"))
        return true;
    if (write_aiff_long(out, 18))
        return true;
    if (write_aiff_short(out, 1)) // Mono
        return true;
    if (write_aiff_ulong(out, sample_length))
        return true;
    if (write_aiff_short(out, 16)) // 16-bit samples
        return true;
    if (write_aiff_extended(out, rate))
        return true;

    if (loop_point >= 0) {
        // Write a marker chunk
        if (write_aiff_id(out, "MARK"))
            return true;
        // We want two markers with one-character names, making it 8 bytes for each marker
        if (write_aiff_long(out, 2 + 2 * 8))
            return true;
        if (write_aiff_ushort(out, 2)) // 2 markers
            return true;
        // Marker 1: loop start
        if (write_aiff_short(out, 1))
            return true;
        if (write_aiff_ulong(out, loop_point))
            return true;
        if (write_aiff_pstring(out, "["))
            return true;
        // Marker 2: loop end
        if (write_aiff_short(out, 2))
            return true;
        if (write_aiff_ulong(out, sample_length))
            return true;
        if (write_aiff_pstring(out, "]"))
            return true;
        
        // Next, the instrument chunk, for the loop points
        if (write_aiff_id(out, "INST"))
            return true;
        if (write_aiff_long(out, 20))
            return true;
        if (write_aiff_char(out, 60)) // Base note: C4
            return true;
        if (write_aiff_char(out, 0)) // Detune
            return true;
        if (write_aiff_char(out, 24)) // Low note: C1 I guess
            return true;
        if (write_aiff_char(out, 107)) // High note: B7 I guess
            return true;
        if (write_aiff_char(out, 1)) // Low velocity
            return true;
        if (write_aiff_char(out, 127)) // High velocity
            return true;
        if (write_aiff_short(out, 0)) // Gain
            return true;
        // Write the same loop for both the sustain loop and release loop, like aif2pcm does
        for (int i = 0; i < 2; ++i) {
            if (write_aiff_short(out, 1)) // Forward looping
                return true;
            if (write_aiff_short(out, 1)) // Starting at marker 1
                return true;
            if (write_aiff_short(out, 2)) // Ending at marker 2
                return true;
        }
    }

    // Finally we have the sound data chunk
    if (write_aiff_id(out, "SSND"))
        return true;
    if (write_aiff_long(out, 8 + 2 * sample_length))
        return true;
    if (write_aiff_ulong(out, 0)) // Not aligning the sample data
        return true;
    if (write_aiff_ulong(out, 0)) // Not aligning the sample data
        return true;
    // Performance is not a goal of this code, in case you didn't realize that yet somehow
    for (uint32_t i = 0; i < sample_length; ++i) {
        if (write_aiff_short(out, sample_data[i] * (1 << 0)))
            return true;
    }
        
    // ...And that's all the chunks we need.
    return false;
}

static int32_t count_brr_blocks(uint16_t length, uint8_t const data[static length]) {
    if (length % 9 != 0 /* && length % 9 != 2     :eyes: */) {
        return -1;
    }
    int i;
    for (i = 0; i < length - 9; i += 9) {
        if (data[i] & 1)
            return -1;
    }
    if ((data[i] & 1) == 0)
        return -1;
    return length / 9;
}

static void decode_brr_block(int16_t *buffer, uint8_t const block[static 9], bool first_block) {
    int shift = block[0] >> 4;
    int filter = (block[0] >> 2) & 3;
    if (first_block && (shift != 0 || filter != 0)) {
        fprintf(stderr, "If you see this message, please tell phoenixbound that your sample with a "
                        "weird initial block exists.\n");
        if (filter == 0) {
            fprintf(stderr, "Specifically, that it uses a nonzero shift. That's the only weird "
                            "part about it.\n");
        }
        shift = filter = 0;
    }
    
    for (int i = 1; i < 9; ++i) {
        for (int j = 4; j >= 0; j -= 4) {
            int s = (block[i] & (0xF << j)) >> j;
            if (s >= 8)
                s -= 16;
            if (shift > 12) {
                s >>= 3;
                // Left shifting a negative number has UB
                s *= (1 << 12);
            } else {
                // Left shifting a negative number has UB
                s *= (1 << shift);
            }
            s >>= 1;
            int new = s;
            int old = 0;
            int older = 0;
            if (filter != 0) {
                old = buffer[-1] >> 1;
                older = buffer[-2] >> 1;
            }
            
            // fullsnes has a typo here! says old*2 for filters 2 and 3, instead of old*1
            switch (filter) {
            case 1:
                new = s + old*1+((-old*1) >> 4);
                break;
            case 2:
                new = s + old*2+((-old*3) >> 5) - older+((older*1) >> 4);
                break;
            case 3:
                new = s + old*2+((-old*13) >> 6) - older+((older*3) >> 4);
                break;
            }

            if (new > 0x7FFF)
                new = 0x7FFF;
            else if (new < -0x8000)
                new = -0x8000;
            if (new > 0x3FFF)
                new -= 0x8000;
            else if (new < -0x4000)
                new += 0x8000;
            
            *buffer++ = new * 2;
        }
    }
}

static int get_full_loop_len(int16_t const *sa_data, int32_t const sa_length, int32_t const sa_loop_len,
                      int16_t const *next_block, int first_loop_start) {
    int loop_start = sa_length - sa_loop_len;
    bool no_match_found = true;
    while (loop_start >= first_loop_start && no_match_found) {
        if (sa_data[loop_start] == next_block[0] &&
            sa_data[loop_start + 1] == next_block[1]) {
            no_match_found = false;
        } else {
            loop_start -= sa_loop_len;
        }
    }
    
    if (loop_start >= first_loop_start) {
        return sa_length - loop_start;
    } else {
        return -1;
    }
}

static int16_t *decode_sample(uint16_t brr_len, uint8_t const brr[static brr_len], int32_t *out_len,
                       int32_t *out_loop_len, int32_t loop_start) {
    int32_t block_count = count_brr_blocks(brr_len, brr);
    if (block_count * 9 != brr_len) {
        fprintf(stderr, "Couldn't verify BRR integrity.\n");
        return NULL;
    }
    
    *out_len = block_count * 16;
    if (!(brr[brr_len - 9] & 2) && loop_start != -1) {
        fprintf(stderr, "Non-looping sample detected -- ignoring loop point\n");
        loop_start = -1;
    }
    
    size_t base_loop_len = loop_start == -1 ? 0 : (*out_len - (loop_start / 9 * 16));
    size_t allocation_size = *out_len * sizeof(int16_t);
    
    int16_t *p = malloc(allocation_size);
    if (!p) {
        fprintf(stderr, "Out of memory\n");
        return NULL;
    }
    int16_t *ret = p;
    
    bool needs_another_loop;
    bool first_block = true;
    int decoding_start = 0;
    int times = 0;
    
    do {
        needs_another_loop = false;
        
        for (int pos = decoding_start; pos < block_count * 9; pos += 9) {
            decode_brr_block(p, &brr[pos], first_block);
            p += 16;
            first_block = false;
        }
        
        if (base_loop_len) {
            decoding_start = loop_start;
            
            int16_t after_loop[18];
            after_loop[0] = p[-2];
            after_loop[1] = p[-1];
            
            decode_brr_block(&after_loop[2], &brr[loop_start], false);
            int full_loop_len = get_full_loop_len(ret, *out_len, base_loop_len, &after_loop[2],
                                                  (brr_len - loop_start) / 9 * 16);
            
            if (full_loop_len == -1) {
                needs_another_loop = true;
                ptrdiff_t diff = p - ret;
                int16_t *new_stuff = realloc(ret, (*out_len + base_loop_len) * sizeof(int16_t));
                if (new_stuff == NULL) {
                    fprintf(stderr, "Out of memory 2: electric boogaloo\n");
                    free(ret);
                    return NULL;
                }
                p = new_stuff + diff;
                *out_len += base_loop_len;
                ret = new_stuff;
            } else {
                *out_loop_len = full_loop_len;
            }
        }
        
        ++times;
    } while (needs_another_loop && times < 64);
    
    if (needs_another_loop) {
        fprintf(stderr, "Sample didn't loop properly after 64 iterations. "
                        "That is one freak of a sample.\n");
        free(ret);
        return NULL;
    }
    return ret;
}

// This function is copied from the GBA decomp preprocessor tool.
static uint8_t *read_whole_file(FILE *restrict file, size_t *out_length) {
    const int CHUNK_SIZE = 4096;
    *out_length = 0;
    
    uint8_t *buffer = malloc(CHUNK_SIZE);
    if (!buffer) {
        fprintf(stderr, "Out of memory while reading file, somehow\n");
        return NULL;
    }
    
    size_t num_allocated_bytes = CHUNK_SIZE;
    size_t buffer_offset = 0;
    size_t count;
    
    while ((count = fread(&buffer[buffer_offset], 1, CHUNK_SIZE, file)) != 0) {
        if (ferror(file)) {
            fprintf(stderr, "Error while reading file\n");
            free(buffer);
            return NULL;
        }
        *out_length += count;
        if (feof(file)) {
            break;
        }
        num_allocated_bytes += CHUNK_SIZE;
        buffer_offset += CHUNK_SIZE;
        uint8_t *new_buffer = realloc(buffer, num_allocated_bytes);
        if (!new_buffer) {
            fprintf(stderr, "Out of memory while reading file, somehow\n");
            free(buffer);
            return NULL;
        }
        buffer = new_buffer;
    }
    
    return buffer;
}

static int u16_hex_string_to_number(char const *str) {
    size_t len = strlen(str);
    if (strcmp(str, "-1") == 0) {
        return -1;
    }
    if (len > 2 && str[0] == '0' && str[1] == 'x') {
        str += 2;
        len -= 2;
    }
    if (len <= 0 || len >= 4) {
        return -2;
    }
    
    uint16_t n = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = str[i];
        n <<= 4;
        if ('0' <= c && c <= '9') {
            n |= c - '0';
        } else if ('A' <= c && c <= 'F') {
            n |= c - 'A' + 10;
        } else if ('a' <= c && c <= 'f') {
            n |= c - 'a' + 10;
        } else {
            return -2;
        }
    }
    return n;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage:\n%s <infile.brr> <outfile.aif> <hex-brr-freq> <hex-brr-loop-offset-or-negative-one>\n",
                argv[0] ? argv[0] : "brr2aif");
        goto fail1;
    }

    int brr_freq = u16_hex_string_to_number(argv[3]);
    if (brr_freq < 0) {
        fprintf(stderr, "Invalid BRR frequency '%s'\n", argv[3]);
        goto fail1;
    }
    
    int brr_loop_offset = u16_hex_string_to_number(argv[4]);
    if (brr_loop_offset == -2) {
        fprintf(stderr, "Invalid BRR loop offset '%s'\n", argv[4]);
        goto fail1;
    }
    
    FILE *infile = fopen(argv[1], "rb");
    if (!infile) {
        fprintf(stderr, "Couldn't open %s for reading\n", argv[1]);
        goto fail1;
    }
    size_t brr_size;
    uint8_t *infile_data = read_whole_file(infile, &brr_size);
    if (!infile_data) {
        fprintf(stderr, "The above error occurred while reading %s\n", argv[1]);
        goto fail2;
    }
    fclose(infile);
    infile = NULL;
    
    FILE *outfile = fopen(argv[2], "wb");
    if (!outfile) {
        fprintf(stderr, "Couldn't open %s for writing\n", argv[2]);
        goto fail3;
    }
    
    int32_t samples_length;
    int32_t samples_loop_length = 0;
    int16_t *samples = decode_sample(brr_size, infile_data, &samples_length,
                                           &samples_loop_length, brr_loop_offset);
    if (!samples) {
        fprintf(stderr, "The above error occurred while converting %s\n", argv[1]);
        goto fail4;
    }
    
    // I really need to confirm those magic numbers... I think I got them by assuming tuning was 0x80?
    // But tuning really ought to be 0...
    if (write_aiff(outfile, samples_length - samples_loop_length, samples_length, samples, brr_freq * 65 / 4)) {
        fprintf(stderr, "Error writing to %s\n", argv[2]);
        goto fail5;
    }
    if (fclose(outfile)) {
        fprintf(stderr, "Error closing %s\n", argv[2]);
        goto fail5;
    }
    
    free(samples);
    free(infile_data);
    return !!fclose(stderr);
    
fail5:
    free(samples);
fail4:
    fclose(outfile);
fail3:
    free(infile_data);
fail2:
    if (infile)
        fclose(infile);
fail1:
    fclose(stderr);
    return 1;
}















