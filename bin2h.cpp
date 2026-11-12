#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("usage: bin2h <input.sys> <output.h>\n");
        return 1;
    }
    FILE* in = fopen(argv[1], "rb");
    if (!in) { printf("cannot open %s\n", argv[1]); return 1; }
    fseek(in, 0, SEEK_END);
    long sz = ftell(in);
    fseek(in, 0, SEEK_SET);

    FILE* out = fopen(argv[2], "w");
    if (!out) { fclose(in); printf("cannot write %s\n", argv[2]); return 1; }

    fprintf(out, "#pragma once\n");
    fprintf(out, "unsigned char WinRing0_sys[] = {\n");
    unsigned char buf[1024];
    long total = 0;
    while (total < sz) {
        size_t n = fread(buf, 1, sizeof(buf), in);
        for (size_t i = 0; i < n; i++) {
            fprintf(out, "0x%02X,", buf[i]);
            if ((total + i) % 16 == 15) fprintf(out, "\n");
        }
        total += n;
    }
    fprintf(out, "\n};\n");
    fprintf(out, "unsigned int WinRing0_sys_len = %ld;\n", sz);

    fclose(out);
    fclose(in);
    printf("written %s (%ld bytes)\n", argv[2], sz);
    return 0;
}
