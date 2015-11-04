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
#define NORM_FREQ ((double)FREQ / SAMPLE_RATE) //normalized frequency, cycles per sample

#define SYMBOL_LEN_MIN 1 //symbol = 1 sample (SAMPLE_RATE bits/s)
#define SYMBOL_LEN_MAX (SAMPLE_RATE / FREQ * 2) //symbol = 2 periods (1000 bits/s)
#define SYMBOL_LEN 30 //symbol = 10 samples
/* symbol_rate = SAMPLE_RATE / SYMBOL_LEN */
/* bit_rate = symbol_rate * 2 */

#define INT_BUFFER_INIT_SIZE 1024

#define SYNCH_SEQ "00110011"


struct int_buffer { //inflating ingeter bufffer structure
        int *data; //array of integers
        size_t cnt; //number of integers in the array
        size_t size; //actual array size
};


static int int_buffer_resize(struct int_buffer *ib)
{
        assert(ib != NULL);

        const size_t new_size = (ib->size == 0) ?
                INT_BUFFER_INIT_SIZE : ib->size * 2;
        int *new_data = realloc(ib->data, new_size * sizeof (int));

        if (new_data == NULL) {
                perror("realloc");
                return 1; //failure
        }

        ib->data = new_data;
        ib->size = new_size;

        return 0; //success
}

static struct int_buffer * int_buffer_init(void)
{
        struct int_buffer *ib = calloc(1, sizeof (struct int_buffer));

        if (ib == NULL) {
                perror("calloc");
        }

        return ib; //NULL or address
}

static void int_buffer_free(struct int_buffer *ib)
{
        assert(ib != NULL);

        free(ib->data);
        free(ib);
}

static int int_buffer_add(struct int_buffer *ib, int data)
{
        assert(ib != NULL);

        if (ib->cnt == ib->size) { //array full, inflate it
                if (int_buffer_resize(ib) != 0) {
                        return 1; //failure
                }
        }

        ib->data[ib->cnt++] = data;

        return 0; //success
}

int mod_symbols(char sym1, char sym2, size_t *time, struct int_buffer *buffer)
{
        double phase_shift;


        switch (2 * (sym1 - '0') + (sym2 - '0')) {
        case 0:
                phase_shift = 1.0 / 4.0 * M_PI; //00 -> 45 degrees
                break;
        case 1:
                phase_shift = 7.0 / 4.0 * M_PI; //01 -> 315 degrees
                break;
        case 2:
                phase_shift = 3.0 / 4.0 * M_PI; //10 -> 135 degrees
                break;
        case 3:
                phase_shift = 5.0 / 4.0 * M_PI; //01 -> 225 degrees
                break;
        default:
                assert(!"bad symbols");
        }

        for (size_t i = 0; i < SYMBOL_LEN; ++i) {
                const double res = AMPLITUDE * cos(2 * M_PI * NORM_FREQ *
                                *time + phase_shift);

                if (int_buffer_add(buffer, (int)res) != 0) {
                        return 1; //failure
                }

                (*time)++;
        }


        return 0; //success
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
        sf_count_t sf_count; //successfully written sampled

        struct int_buffer *buffer = int_buffer_init(); //samples buffer


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


        /* Initializations, file opening. */
        if (buffer == NULL) {
                return EXIT_FAILURE;
        }

        in_file = fopen(argv[1], "r");
        if (in_file == NULL) {
                perror(argv[1]);
                return EXIT_FAILURE;
        }


        /* Modulate synchronization sequence. */
        for (size_t i = 0; i < (sizeof (SYNCH_SEQ) - 1); i += 2) {
                if (mod_symbols(SYNCH_SEQ[i], SYNCH_SEQ[i + 1], &time, buffer)
                                != 0) {
                        return EXIT_FAILURE;
                }
        }

        /* Modulate input data file. */
        for (int sym1 = fgetc(in_file), sym2 = fgetc(in_file);
                        (sym1 == '0' || sym1 == '1') && (sym2 == '0' || sym2 == '1');
                        sym1 = fgetc(in_file), sym2 = fgetc(in_file))
        {
                if (mod_symbols(sym1, sym2, &time, buffer) != 0) {
                        return EXIT_FAILURE;
                }
        }

        /* Open output sound file, write buffer and close file. */
        argv[1][file_name_len - 3] = 'w';
        argv[1][file_name_len - 2] = 'a';
        argv[1][file_name_len - 1] = 'w';

        out_file = sf_open(argv[1], SFM_WRITE, &sf_info);
        if (out_file == NULL) {
                fprintf(stderr, "%s\n", sf_strerror(out_file));
                return EXIT_FAILURE;
        }

        sf_count = sf_write_int(out_file, buffer->data, buffer->cnt);
        assert((size_t)sf_count == buffer->cnt);

        ret = sf_close(out_file);
        if (ret != 0) {
                fprintf(stderr, "%s\n", sf_error_number(ret));
        }


        /* Close input text file and free sample buffer. */
        fclose(in_file);
        int_buffer_free(buffer);

        return EXIT_SUCCESS;
}
