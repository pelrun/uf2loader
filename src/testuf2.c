
#include <stdio.h>

int process_uf2(FILE *fp, char *filename);

int main (int argc, char *argv[])
{
    FILE *fp;

    fp = fopen(argv[1], "rb");
    if (fp)
    {
        process_uf2(fp, argv[1]);
    }
    else
    {
        printf("couldn't open %s\n", argv[1]);
    }
    return 0;
}
