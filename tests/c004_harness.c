/* Real ISPY host code, with an in-memory link in place of the device driver. */
#define main ispy_main
#include "../check.c"
#undef main

static unsigned char incoming[2048], outgoing[4096];
static unsigned int in_count, in_pos, out_count;
static int writes, fail_write, read_chunk, write_chunk;

LINK OpenLink(char *name) { (void)name; return 1; }
int CloseLink(LINK link) { (void)link; return 1; }
int ResetLink(LINK link) { (void)link; return 1; }
int AnalyseLink(LINK link) { (void)link; return 1; }
int TestError(LINK link) { (void)link; return 0; }
int TestRead(LINK link) { (void)link; return in_pos < in_count; }
int TestWrite(LINK link) { (void)link; return 1; }

int ReadLink(LINK link, unsigned char *buffer, unsigned int count, int timeout)
{
    (void)link; (void)timeout;
    if (count > in_count - in_pos) count = in_count - in_pos;
    if (read_chunk && count > (unsigned int)read_chunk) count = read_chunk;
    memcpy(buffer, incoming + in_pos, count);
    in_pos += count;
    return count;
}

int WriteLink(LINK link, unsigned char *buffer, unsigned int count, int timeout)
{
    (void)link; (void)timeout;
    if (getenv("TRACE_WRITES")) fputs("mock WriteLink\n", stderr);
    if (++writes == fail_write) return -1;
    if (write_chunk && count > (unsigned int)write_chunk) count = write_chunk;
    if (out_count + count > sizeof(outgoing)) abort();
    memcpy(outgoing + out_count, buffer, count);
    out_count += count;
    return count;
}

static void packet(unsigned char *data, unsigned int length)
{
    incoming[in_count++] = length & 255;
    incoming[in_count++] = length >> 8;
    memcpy(incoming + in_count, data, length);
    in_count += length;
}

int main(int argc, char **argv)
{
    struct tpstats parent = {0};
    struct c4stats chip = {0};
    unsigned char raw[33] = {0}, revision[2] = {0};
    int i;
    if (argc < 2) return 2;
    TheLink = 1;
    root = &parent;
    parent.tptype = T_425;
    chip.parent = &parent;
    chip.tptype = C4 + 1;
    chip.tpid = 1;
    chip.route = 1;
    chip.routelen = 1;
    if (getenv("FAIL_WRITE")) fail_write = atoi(getenv("FAIL_WRITE"));
    if (getenv("READ_CHUNK")) read_chunk = atoi(getenv("READ_CHUNK"));
    if (getenv("WRITE_CHUNK")) write_chunk = atoi(getenv("WRITE_CHUNK"));

    if (!strcmp(argv[1], "verify")) {
        struct c4stats wanted = {0};
        if (argc != 5) return 2;
        chip.tptype = C4 + atoi(argv[2]);
        for (i = 0; i < 32; i++) chip.config[i] = wanted.config[i] = C004_EMPTY;
        wanted.config[7] = atoi(argv[3]);
        chip.config[7] = atoi(argv[4]);
        return verifyC004(&chip, &wanted) ? 0 : 1;
    } else if (!strcmp(argv[1], "set")) {
        if (argc != 34) return 2;
        for (i = 0; i < 32; i++) chip.config[i] = atoi(argv[i + 2]);
        setC004(&chip);
    } else if (!strcmp(argv[1], "read")) {
        if (argc != 37) return 2;
        chip.tptype = C4 + atoi(argv[2]);
        revision[0] = atoi(argv[3]);
        for (i = 0; i < 32; i++) raw[i] = atoi(argv[i + 4]);
        packet(revision, getenv("REVISION_LENGTH") ? atoi(getenv("REVISION_LENGTH")) : 1);
        packet(raw, atoi(argv[36]));
        readC004(&chip);
        printf("TYPE %d\nCONFIG", chip.tptype);
        for (i = 0; i < 32; i++) printf(" %d", chip.config[i]);
        puts("");
    } else if (!strcmp(argv[1], "parse")) {
        struct tpstats *tree, *node;
        char pipe[LINESIZE] = "", link[LINESIZE], banner[LINESIZE] = "";
        lex(stdin, &tree, pipe, link, banner);
        for (node = tree; node; node = node->next) {
            if (class(node->tptype) == C4) {
                struct c4stats *q = (struct c4stats *)node;
                printf("CONFIG");
                for (i = 0; i < 32; i++) printf(" %d", q->config[i]);
                puts("");
                writeC4(q); puts("");
                writeCl(q); puts("");
            }
        }
        while (tree) { node = tree->next; free(tree); tree = node; }
    } else if (!strcmp(argv[1], "packet")) {
        unsigned char data[32];
        for (i = 0; i < 32; i++) data[i] = i;
        if (!sendiserver(TheLink, 32, data)) return 1;
    } else if (!strcmp(argv[1], "reset-history")) {
        struct tpstats nodes[140] = {{0}};
        struct c004reset *seen = NULL, *next;
        for (i = 0; i < 140; i++) {
            if (i) nodes[i].parent = &nodes[i - 1];
            nodes[i].route = 0;
            /* These leading-zero routes all collided in the old checksum. */
            if (!firstc004reset(&seen, &nodes[i], 0)) return 3;
            if (firstc004reset(&seen, &nodes[i], 0)) return 4;
            if (!firstc004reset(&seen, &nodes[i], 1)) return 5;
        }
        while (seen) { next = seen->next; free(seen); seen = next; }
    } else return 2;
    printf("WIRE ");
    for (i = 0; i < (int)out_count; i++) printf("%02x", outgoing[i]);
    puts("");
    return 0;
}
