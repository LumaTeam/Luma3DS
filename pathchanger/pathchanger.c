#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;

static u8 *memsearch(u8 *startPos, const void *pattern, int size, int patternSize)
{
    const u8 *patternc = (const u8 *)pattern;

    //Preprocessing
    int table[256];

    int i;
    for(i = 0; i < 256; ++i)
        table[i] = patternSize + 1;
    for(i = 0; i < patternSize; ++i)
        table[patternc[i]] = patternSize - i;

    //Searching
    int j = 0;

    while(j <= size - patternSize)
    {
        if(memcmp(patternc, startPos + j, patternSize) == 0)
            return startPos + j;
        j += table[startPos[j + patternSize]];
    }

    return NULL;
}

static int fsize(FILE *fp)
{
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    rewind(fp);

    return size;
}

static void error(FILE *payload, const char *message)
{
    fclose(payload);
    printf("%s, are you sure you're using an AuReiNand payload?\n", message);
    exit(0);
}

int main(int argc, char **argv)
{
    if(argc == 1)
    {
        printf("Usage: %s <AuReiNand payload path>\n", argv[0]);
        exit(0);
    }

    FILE *payload;
    size_t size;

    payload = fopen(argv[1], "rb+");
    size = fsize(payload);
    if(size > 0x20000)
        error(payload, "The input file is too large");

    u8 *buffer = (u8 *)malloc(size);
    fread(buffer, 1, size, payload);

    u8 pattern[] = {'s', 0, 'd', 0, 'm', 0, 'c', 0, ':', 0, '/', 0};

    u8 *found = memsearch(buffer, pattern, size, sizeof(pattern));

    if(found == NULL)
    {
        free(buffer);
        error(payload, "Pattern not found");
    }

    u8 input[38] = {0};
    u8 payloadname[2 * (sizeof(input) - 1)] = {0};

    printf("Enter the payload's path (37 characters max): ");
    scanf("%37s", input);

    unsigned int i;
    for (i = 0; i < sizeof(input) - 1; i++)
        payloadname[2 * i] = input[i];

    memcpy(found + 12, payloadname, sizeof(payloadname));

    rewind(payload);
    fwrite(buffer, 1, size, payload);

    free(buffer);
    fclose(payload);

    exit(0);
}