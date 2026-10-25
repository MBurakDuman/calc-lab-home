// Mehmet Burak Duman 241ADB160
// https://github.com/MBurakDuman/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#define FIRST_NAME "mehmet_burak"
#define LAST_NAME "duman"
#define STUDENT_ID "241adb160"

enum { END = 256, NUMBER, POWER, INVALID };

typedef struct {
    const char *text;
    size_t index;
    int token;
    long long number;
    int error;
} Parser;

/* index points to the next unread character.
 * Each call leaves one token available for the parser to inspect.
 */
static void next_token(Parser *p)
{
    while (isspace((unsigned char)p->text[p->index])) {
        p->index++;
    }

    unsigned char c = (unsigned char)p->text[p->index];

    if (c == '\0') {
        p->token = END;
        return;
    }

    if (isdigit(c)) {
        char *end;

        p->number = strtoll(p->text + p->index, &end, 10);
        p->index = (size_t)(end - p->text);
        p->token = NUMBER;
        return;
    }

    p->index++;

    /* Two adjacent stars form one operator. */
    if (c == '*' && p->text[p->index] == '*') {
        p->index++;
        p->token = POWER;
    } else if (c == '+' || c == '-' || c == '*' ||
               c == '/' || c == '(' || c == ')') {
        p->token = c;
    } else {
        p->token = INVALID;
    }
}

static long long expression(Parser *p);

/* Parentheses make a complete expression act as one operand.
 * Rejecting other starting tokens also rejects unary signs.
 */
static long long primary(Parser *p)
{
    if (p->token == NUMBER) {
        long long value = p->number;
        next_token(p);
        return value;
    }

    if (p->token == '(') {
        next_token(p);

        long long value = expression(p);

        if (p->error) {
            return 0;
        }

        if (p->token != ')') {
            p->error = 1;
            return 0;
        }

        next_token(p);
        return value;
    }

    p->error = 1;
    return 0;
}

/* Recursion on the right groups 2**3**2 as 2**(3**2).
 * Very long power chains will need an iterative implementation.
 */
static long long power(Parser *p)
{
    long long base = primary(p);

    if (p->error || p->token != POWER) {
        return base;
    }

    next_token(p);

    long long exponent = power(p);

    if (p->error) {
        return 0;
    }

    if (exponent < 0) {
        p->error = 1;
        return 0;
    }

    long long result = 1;

    /* Squaring needs only logarithmically many steps.
     * Skip the unused final square to avoid unnecessary overflow.
     * The assignment guarantees that integer arithmetic fits.
     */
    while (exponent > 0) {
        if (exponent % 2 != 0) {
            result *= base;
        }

        exponent /= 2;

        if (exponent > 0) {
            base *= base;
        }
    }

    return result;
}

/* Parsing powers first gives ** higher precedence.
 * The loop applies multiplication and division left to right.
 */
static long long term(Parser *p)
{
    long long result = power(p);

    while (!p->error && (p->token == '*' || p->token == '/')) {
        int op = p->token;
        next_token(p);

        long long right = power(p);

        if (p->error) {
            return 0;
        }

        if (op == '*') {
            result *= right;
        } else {
            if (right == 0) {
                p->error = 1;
                return 0;
            }

            result /= right;
        }
    }

    return result;
}

/* Each operand is a complete term, so * and / finish before + and -.
 * The loop makes subtraction left-associative.
 */
static long long expression(Parser *p)
{
    long long result = term(p);

    while (!p->error && (p->token == '+' || p->token == '-')) {
        int op = p->token;
        next_token(p);

        long long right = term(p);

        if (p->error) {
            return 0;
        }

        if (op == '+') {
            result += right;
        } else {
            result -= right;
        }
    }

    return result;
}

static int make_output_directory(const char *path)
{
    struct stat st;

    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }

        fprintf(stderr, "Output path is not a directory: %s\n", path);
        return 1;
    }

    /* Only create a missing directory; other stat errors must be reported. */
    if (errno != ENOENT) {
        perror(path);
        return 1;
    }

    if (mkdir(path, 0777) != 0) {
        perror(path);
        return 1;
    }

    return 0;
}

static char *make_output_path(const char *input, const char *outdir)
{
    /* Strip parent directories so labs/task1.txt becomes task1. */
    const char *base = strrchr(input, '/');

    if (base != NULL) {
        base++;
    } else {
        base = input;
    }

    size_t base_length = strlen(base);

    if (base_length >= 4 &&
        strcmp(base + base_length - 4, ".txt") == 0) {
        base_length -= 4;
    }

    const char suffix[] =
        "_" FIRST_NAME "_" LAST_NAME "_" STUDENT_ID ".txt";

    size_t directory_length = strlen(outdir);
    size_t size = directory_length + 1 + base_length + sizeof(suffix);

    char *path = malloc(size);

    if (path == NULL) {
        fprintf(stderr, "Out of memory\n");
        return NULL;
    }

    memcpy(path, outdir, directory_length);
    path[directory_length] = '/';

    memcpy(path + directory_length + 1, base, base_length);

    /* sizeof(suffix) includes the terminating null character. */
    memcpy(path + directory_length + 1 + base_length,
           suffix, sizeof(suffix));

    return path;
}

static int process_file(const char *input, const char *outdir)
{
    struct stat st;

    if (stat(input, &st) != 0) {
        perror(input);
        return 1;
    }

    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "Input is not a regular file: %s\n", input);
        return 1;
    }

    FILE *in = fopen(input, "r");

    if (in == NULL) {
        perror(input);
        return 1;
    }

    if (make_output_directory(outdir) != 0) {
        fclose(in);
        return 1;
    }

    char *output = make_output_path(input, outdir);

    if (output == NULL) {
        fclose(in);
        return 1;
    }

    FILE *out = fopen(output, "w");

    if (out == NULL) {
        perror(output);
        free(output);
        fclose(in);
        return 1;
    }

    int status = 0;

    /* Temporary buffer; dynamic line reading is the next stage. */
    char line[1024];

    while (fgets(line, sizeof(line), in) != NULL) {
        Parser p = {0};
        p.text = line;

        next_token(&p);

        /* Whitespace-only lines contain no expression. */
        if (p.token == END) {
            continue;
        }

        long long result = expression(&p);
        int written;

        /* Temporary error format. Exact file positions and discarding
         * partial results will be implemented in the next stage.
         */
        if (p.error || p.token != END) {
            written = fprintf(out, "ERROR\n");
        } else {
            written = fprintf(out, "%lld\n", result);
        }

        if (written < 0) {
            fprintf(stderr, "Cannot write output: %s\n", output);
            status = 1;
            break;
        }
    }

    if (ferror(in)) {
        fprintf(stderr, "Cannot read input: %s\n", input);
        status = 1;
    }

    if (fclose(in) != 0) {
        perror(input);
        status = 1;
    }

    /* Buffered writes can fail when the file is closed. */
    if (fclose(out) != 0) {
        perror(output);
        status = 1;
    }

    free(output);
    return status;
}

int main(int argc, char *argv[])
{
    const char *input = NULL;
    const char *outdir = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (outdir != NULL ||
                i + 1 >= argc ||
                argv[i + 1][0] == '-') {
                goto usage;
            }

            outdir = argv[++i];
        } else {
            if (argv[i][0] == '-' || input != NULL) {
                goto usage;
            }

            input = argv[i];
        }
    }

    /* This stage requires -o. Default folders and directory input
     * will be added when the full command line is implemented.
     */
    if (input == NULL || outdir == NULL) {
        goto usage;
    }

    return process_file(input, outdir);

usage:
    fprintf(stderr, "Usage: %s -o OUTDIR input.txt\n", argv[0]);
    return 1;
}