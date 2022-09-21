//
// Use this code to create a byte array from a cert
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int __cdecl main(int argc, char* argv[])
{
    FILE* input;
    FILE* output;
    uint8_t byte = 0x00;

    if (argc < 2) {
        fprintf(stderr, "Usage cert2array <file>\n");
        return -1;
    }

    input = fopen(argv[1], "rb");
    if (!input) {
        fprintf(stderr, "%s not found\n", argv[1]);
        return -1;
    }

    output = fopen("cert.h", "w");
    if (!output) {
        fprintf(stderr, "Cannot create header file\n");
        fclose(input);
        return -1;
    }

    fprintf(output, "#ifndef __CERT_H__\n");
    fprintf(output, "#define __CERT_H__\n\n");
    fprintf(output, "/* %s */\n", argv[1]);
    fprintf(output, "const unsigned char cert_array[] = {");

    while (fread(&byte, 1, 1, input) == 1) {
        fprintf(output, "0x%02x,", byte);
    }

    fprintf(output, "};\n");
    fprintf(output, "#endif /* __CERT_H__ */\n");

    fclose(input);
    fclose(output);

    return 0;
}