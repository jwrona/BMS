#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "sndfile.h"


#define PATH "sine.waw"


int main(int argc, char **argv)
{
#if 0
        int ret;
        sf_count_t sf_count;

        SNDFILE *in_file;
        SF_INFO sf_info = { 0 };

        int *buffer;


        in_file = sf_open(PATH, SFM_READ, &sf_info);
        if (in_file == NULL) {
                fprintf(stderr, "%s\n", sf_strerror(in_file));
                return EXIT_FAILURE;
        }

        buffer = malloc(sf_info.samplerate * sizeof (int));
        if (buffer == NULL) {
                perror("malloc");
                return EXIT_FAILURE;
        }

        sf_count = sf_read_int(in_file, buffer, sf_info.samplerate);
        assert(sf_count == sf_info.samplerate);
        for (size_t i = 0; i < sf_info.samplerate; ++i) {
                printf("%d\n", buffer[i]);
        }

        ret = sf_close(in_file);
        if (ret != 0) {
                fprintf(stderr, "%s\n", sf_error_number(ret));
        }

#endif
        return EXIT_SUCCESS;
}
