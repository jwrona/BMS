/**
 * \file bms1A.c
 * \brief QPSK modulator
 * \author Jan Wrona, <xwrona00@stud.fit.vutbr.cz>
 * \date 2015
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "sndfile.h"


#define SAMPLE_RATE 18000
#define CHANNELS 1
#define FORMAT (SF_FORMAT_WAV | SF_FORMAT_PCM_32) //major and minor
#define AMPLITUDE 0x7F000000u

#define FREQ 1000 //frequency [Hz]
#define NORM_FREQ ((double)FREQ / SAMPLE_RATE) //normalized f, cycles per sample

#define SYMBOL_LEN_MIN 1 //symbol = 1 sample (SAMPLE_RATE bps)
#define SYMBOL_LEN_MAX (SAMPLE_RATE / FREQ * 2) //symbol = 2 periods (1000 bps)
#define SYMBOL_LEN 30 //symbol = SYMBOL_LEN samples
/* symbol_rate = SAMPLE_RATE / SYMBOL_LEN */
/* bit_rate = symbol_rate * 2 */

#define SYNCH_SEQ "00110011"


static const double phase_shift[4] = {
        1.0 / 4.0 * M_PI, //00 -> 45 degrees
        7.0 / 4.0 * M_PI, //01 -> 315 degrees
        3.0 / 4.0 * M_PI, //10 -> 135 degrees
        5.0 / 4.0 * M_PI, //11 -> 225 degrees
};


static void mod_symbols(char sym1, char sym2, size_t *time, int *buffer)
{
        size_t phase_shift_idx = 2 * (sym1 - '0') + (sym2 - '0');


        assert(phase_shift_idx < 4);

        for (size_t i = 0; i < SYMBOL_LEN; ++i) {
                buffer[i] = AMPLITUDE * cos(2.0 * M_PI * NORM_FREQ * *time +
                                phase_shift[phase_shift_idx]);

                (*time)++;
        }
}

int main(int argc, char **argv)
{
        FILE *in_file; //input text file with zeroes '0' and ones '1'
        size_t file_name_len;
        size_t time = 0; //discrete time
        int ret;

        SNDFILE *out_file; //output WAW file
        SF_INFO sf_info = { //output WAW file parameters
                .samplerate = SAMPLE_RATE,
                .channels = CHANNELS,
                .format = FORMAT,
        };
        sf_count_t items_written; //successfully written items

        int buffer[SYMBOL_LEN]; //samples buffer


        if (argc != 2) {
                fprintf(stderr, "error: bad argument count\n");
                return EXIT_FAILURE;
        }

        file_name_len = strlen(argv[1]);
        if (file_name_len < 3 ||
                        strcmp(argv[1] + (file_name_len - 3), "txt") != 0) {
                fprintf(stderr, "error: bad input file name\n");
                return EXIT_FAILURE;
        }


        /* Input and output file opening. */
        in_file = fopen(argv[1], "r");
        if (in_file == NULL) {
                perror(argv[1]);
                return EXIT_FAILURE;
        }

        argv[1][file_name_len - 3] = 'w';
        argv[1][file_name_len - 2] = 'a';
        argv[1][file_name_len - 1] = 'v';
        out_file = sf_open(argv[1], SFM_WRITE, &sf_info);
        if (out_file == NULL) {
                fprintf(stderr, "%s\n", sf_strerror(out_file));
                return EXIT_FAILURE;
        }


        /* Modulate and write synchronization sequence. */
        for (size_t i = 0; i < (sizeof (SYNCH_SEQ) - 1); i += 2) {
                mod_symbols(SYNCH_SEQ[i], SYNCH_SEQ[i + 1], &time, buffer);

                items_written = sf_write_int(out_file, buffer, SYMBOL_LEN);
                assert(items_written == SYMBOL_LEN);
        }

        /* Modulate and write input data file. */
        for (int sym1 = fgetc(in_file), sym2 = fgetc(in_file);
                        (sym1 == '0' || sym1 == '1') &&
                        (sym2 == '0' || sym2 == '1');
                        sym1 = fgetc(in_file), sym2 = fgetc(in_file))
        {
                mod_symbols(sym1, sym2, &time, buffer);

                items_written = sf_write_int(out_file, buffer, SYMBOL_LEN);
                assert(items_written == SYMBOL_LEN);
        }


        /* Close input and output filess. */
        ret = sf_close(out_file);
        if (ret != 0) {
                fprintf(stderr, "%s\n", sf_error_number(ret));
        }
        fclose(in_file);


        return EXIT_SUCCESS;
}
