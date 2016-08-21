/**
 * \file bms1B.c
 * \brief QPSK demodulator
 * \author Jan Wrona, <xwrona00@stud.fit.vutbr.cz>
 * \date 2015
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "sndfile.h"


#define AMPLITUDE 0x7F000000u
#define FREQ 1000 //frequency [Hz]
#define THRESHOLD 0.1 //god knows why this number

#define BUFFER_SIZE 1024


typedef enum { //syncing FSM states
        SYNC_STATE_INIT,
        SYNC_STATE_FIRST_00,
        SYNC_STATE_FIRST_11,
        SYNC_STATE_SECOND_00,
        SYNC_STATE_SECOND_11,
} sync_state_t;


static const double phase_shift[4] = {
        1.0 / 4.0 * M_PI, //00 -> 45 degrees
        7.0 / 4.0 * M_PI, //01 -> 315 degrees
        3.0 / 4.0 * M_PI, //10 -> 135 degrees
        5.0 / 4.0 * M_PI, //11 -> 225 degrees
};

static const char *res_sym[4] = {
        "00", //00 -> 45 degrees
        "01", //01 -> 315 degrees
        "10", //10 -> 135 degrees
        "11", //11 -> 225 degrees
};


static int sync(double key, size_t *symbol_len, size_t time, double norm_freq)
{
        static sync_state_t sync_state = SYNC_STATE_INIT;
        static size_t rem_items;

        const double res = key / AMPLITUDE; //received cosinus value
        double ref; //reference cosinus value


        switch (sync_state) {
        /* First sample always has to conform to "00" phase shift. */
        case SYNC_STATE_INIT:
                ref = cos(2.0 * M_PI * norm_freq * time + phase_shift[0]);
                if (fabs(ref - res) < THRESHOLD) { //found first "00"
                        (*symbol_len)++;
                        sync_state = SYNC_STATE_FIRST_00;
                } else { //found nothing or  some other symbol
                        fprintf(stderr, "error: bad initialization sequence\n");
                        return -1;
                }
                break;

        /* We can read another "00" sample or first "01" sample. */
        case SYNC_STATE_FIRST_00:
                ref = cos(2.0 * M_PI * norm_freq * time + phase_shift[0]);
                if (fabs(ref - res) < THRESHOLD) { //found another "00"
                        (*symbol_len)++;
                        break; //don't change state
                }

                ref = cos(2.0 * M_PI * norm_freq * time + phase_shift[3]);
                if (fabs(ref - res) < THRESHOLD) { //found "11"
                        sync_state = SYNC_STATE_FIRST_11;
                        rem_items = *symbol_len - 1;
                } else { //found nothing or "01" or "10"
                        fprintf(stderr, "error: bad initialization sequence\n");
                        return -1;
                }
                break;

        /* Now we know, how long (in samples) one symbol should be. */
        /* We can read another "01" sample or first "00" sample. */
        case SYNC_STATE_FIRST_11:
                if (rem_items == 0) { //expecting first "00" sample
                        sync_state = SYNC_STATE_SECOND_00; //switch state
                        rem_items = *symbol_len;
                        //pass through to the next state
                } else { //expecting another "01" sample
                        ref = cos(2.0 * M_PI * norm_freq * time + phase_shift[3]);
                        if (fabs(ref - res) < THRESHOLD) { //found "11"
                                rem_items--;
                                break;
                        } else { //found nothing or some other symbol
                                fprintf(stderr, "error: bad initialization sequence\n");
                                return -1;
                        }
                }

        /* We can read "00" sample or first "01" sample. */
        case SYNC_STATE_SECOND_00:
                if (rem_items == 0) { //expecting first "01" sample
                        sync_state = SYNC_STATE_SECOND_11; //switch state
                        rem_items = *symbol_len;
                        //pass through to the next state
                } else { //expecting another "00" sample
                        ref = cos(2.0 * M_PI * norm_freq * time + phase_shift[0]);
                        if (fabs(ref - res) < THRESHOLD) { //found "00"
                                rem_items--;
                                break;
                        } else { //found nothing or some other symbol
                                fprintf(stderr, "error: bad initialization sequence\n");
                                return -1;
                        }
                }

        /* Last sync symbol, we have to read all the "01" samples. */
        case SYNC_STATE_SECOND_11:
                if (rem_items > 1) { //not all "01" samples read yet
                        ref = cos(2.0 * M_PI * norm_freq * time + phase_shift[3]);
                        if (fabs(ref - res) < THRESHOLD) { //found "11"
                                rem_items--;
                                break;
                        } else { //found nothing or some other symbol
                                fprintf(stderr, "error: bad initialization sequence\n");
                                return -1;
                        }
                } else if (rem_items == 1) { //last "01" sample expected
                        ref = cos(2.0 * M_PI * norm_freq * time + phase_shift[3]);
                        if (fabs(ref - res) < THRESHOLD) { //found last "11"
                                rem_items--;
                                return 0; //whole synchronization sequence read
                        } else { //found nothing or some other symbol
                                fprintf(stderr, "error: bad initialization sequence\n");
                                return -1;
                        }
                } else { //rem_items == 0
                        assert(!"NOOOOO, this is not synchronization sequence");
                }
                break;

        default:
                assert(!"unknown sync FSM state");
        }


        return 1; //synchronization sequence not completely read
}


int main(int argc, char **argv)
{
        int ret; //return code

        SNDFILE *in_file; //input WAW file
        SF_INFO sf_info = { 0 }; //input WAW file parameters
        size_t file_name_len;
        int buffer; //sample buffer

        double norm_freq;
        size_t time = 0; //discrete time
        size_t symbol_len = 0; //in samples

        FILE *out_file;


        if (argc != 2) {
                fprintf(stderr, "error: bad argument count\n");
                return EXIT_FAILURE;
        }

        file_name_len = strlen(argv[1]);
        if (file_name_len < 3 ||
                        strcmp(argv[1] + (file_name_len - 3), "wav") != 0) {
                fprintf(stderr, "error: bad input file name\n");
                return EXIT_FAILURE;
        }


        /* Initializations, file opening. */
        in_file = sf_open(argv[1], SFM_READ, &sf_info);
        if (in_file == NULL) {
                fprintf(stderr, "%s\n", sf_strerror(in_file));
                return EXIT_FAILURE;
        }


        /* Calculate normalized frequency. */
        norm_freq = (double)FREQ / sf_info.samplerate;

        /* Read synchronization sequence and determine symbol length. */
        while (sf_read_int(in_file, &buffer, 1) != 0) {
                ret = sync((double)buffer, &symbol_len, time++, norm_freq);
                if (ret == -1) { //some error during synchronization
                        return EXIT_FAILURE;
                } else if (ret == 0) { //all sync sequence successfully read
                        break;
                }
        }

        //printf("bit rate = %zu\n", sf_info.samplerate / symbol_len * 2);

        /* Open output text file. */
        argv[1][file_name_len - 3] = 't';
        argv[1][file_name_len - 2] = 'x';
        argv[1][file_name_len - 1] = 't';
        out_file = fopen(argv[1], "w");
        if (out_file == NULL) {
                perror(argv[1]);
                return EXIT_FAILURE;
        }

        while (1) {
                size_t res_histogram[4] = { 0 }; //result histogram
                size_t max_val = 0; //maximum value in histogram (one of them)
                size_t max_idx = 0; //index of maximum value in histogram

                /* Read all samples for one symbol. */
                for (size_t i = 0; i < symbol_len; ++i) {
                        double res;

                        if (sf_read_int(in_file, &buffer, 1) == 0) {
                                goto all_read_lab;
                        }
                        res = (double)buffer / AMPLITUDE;

                        /* Compare with all four possible phase shifts. */
                        /* It is stupid, but working. */
                        for (size_t j = 0; j < 4; ++j) {
                                double ref = cos(2.0 * M_PI * norm_freq * time +
                                                phase_shift[j]);

                                res_histogram[j] += fabs(ref - res) < THRESHOLD;
                        }
                        time++;
                }

                /* Find the most popular phase shift for this symbol. */
                for (size_t i = 0; i < 4; ++i) {
                        if (max_val < res_histogram[i]) {
                                max_val = res_histogram[i];
                                max_idx = i;
                        }
                }

                /* Write string coresponding to the symbol to the file. */
                fputs(res_sym[max_idx], out_file);
        }


all_read_lab:
        fputc('\n', out_file); //write EOL to the output file

        /* Close files. */
        fclose(out_file);
        ret = sf_close(in_file);
        if (ret != 0) {
                fprintf(stderr, "%s\n", sf_error_number(ret));
        }

        return EXIT_SUCCESS;
}
