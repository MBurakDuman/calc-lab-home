// Mehmet Burak Duman 241ADB160
// https://github.com/MBurakDuman/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: GPT: small code generation, comments and some debugs;
// Gemini (Google): code review, explanations.

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>

#define FIRST_NAME "mehmet_burak"
#define LAST_NAME "duman"
#define STUDENT_ID "241adb160"

enum { END = 256, NUMBER, POWER, INVALID };

typedef struct {
    int floating;
    double real;
    long long integer;
    size_t start;
} Value;

typedef struct {
    char *text;
    size_t length;
    size_t index;
    size_t base;
    int token;
    size_t token_pos;
    size_t error;
    Value number;
} Parser;

static void *allocate(void *old, size_t bytes)
{
    void *p = realloc(old, bytes);

    if (p == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(EXIT_FAILURE);
    }

    return p;
}

static size_t grow(size_t capacity, size_t item_size)
{
    if (capacity > (size_t)-1 / 2 / item_size) {
        fprintf(stderr, "Input is too large\n");
        exit(EXIT_FAILURE);
    }

    return capacity * 2;
}

static Value zero_value(void)
{
    Value v = {0};
    return v;
}

/* Keep the first error; later parser checks must not replace it. */
static void fail(Parser *p, size_t position)
{
    if (p->error == 0) {
        p->error = position;
    }
}

/* base counts every byte in previous lines, including comments and '\n'.
 * The current line excludes '\n', so END points at its newline or,
 * for a final line without one, at file length + 1.
 */
static void next_token(Parser *p)
{
    while (p->index < p->length &&
           (p->text[p->index] == ' ' ||
            p->text[p->index] == '\t' ||
            p->text[p->index] == '\r')) {
        p->index++;
    }

    p->token_pos = p->base + p->index + 1;

    if (p->index == p->length) {
        p->token = END;
        return;
    }

    unsigned char c = (unsigned char)p->text[p->index];

    if (isdigit(c)) {
        size_t begin = p->index;

        while (p->index < p->length &&
               isdigit((unsigned char)p->text[p->index])) {
            p->index++;
        }

        int is_float = 0;
        if (p->index + 1 < p->length && p->text[p->index] == '.' &&
            isdigit((unsigned char)p->text[p->index + 1])) {
            is_float = 1;
            p->index++;
            while (p->index < p->length &&
                   isdigit((unsigned char)p->text[p->index])) {
                p->index++;
            }
        }

        /* Temporarily terminate the token instead of copying it. */
        char saved = p->text[p->index];
        p->text[p->index] = '\0';

        p->number = zero_value();
        p->number.floating = is_float;
        if (is_float) {
            p->number.real = strtod(p->text + begin, NULL);
            p->token = isfinite(p->number.real) ? NUMBER : INVALID;
        } else {
            errno = 0;
            p->number.integer = strtoll(p->text + begin, NULL, 10);
            p->token = errno == ERANGE ? INVALID : NUMBER;
        }
        p->number.start = p->token_pos;

        p->text[p->index] = saved;
        return;
    }

    p->index++;

    if (c == '*' && p->index < p->length &&
        p->text[p->index] == '*') {
        p->index++;
        p->token = POWER;
    } else if (c == '+' || c == '-' || c == '*' ||
               c == '/' || c == '(' || c == ')') {
        p->token = c;
    } else {
        /* Leave invalid tokens for the parser to report when reached. */
        p->token = INVALID;
    }
}

/* Keep the first token of each operand.
 * Division by zero and negative exponents are reported at the start
 * of the right operand, including '(' when that operand is grouped.
 */
static Value calculate(Parser *p, Value a, int op, Value b,
                       size_t operator_pos)
{
    Value result = zero_value();
    result.start = a.start;

    int is_b_zero = b.floating ? b.real == 0.0 : b.integer == 0;
    if (op == '/' && is_b_zero) {
        fail(p, b.start);
        return result;
    }

    if (a.floating || b.floating) {
        result.floating = 1;
        double da = a.floating ? a.real : (double)a.integer;
        double db = b.floating ? b.real : (double)b.integer;
        switch (op) {
            case '+': result.real = da + db; break;
            case '-': result.real = da - db; break;
            case '*': result.real = da * db; break;
            case '/': result.real = da / db; break;
            case POWER: result.real = pow(da, db); break;
        }
        if (!isfinite(result.real)) {
            fail(p, operator_pos);
        }
        return result;
    }

    if (op == POWER && b.integer < 0) {
        fail(p, b.start);
        return result;
    }

    switch (op) {
        case '+':
            result.integer = a.integer + b.integer;
            break;
        case '-':
            result.integer = a.integer - b.integer;
            break;
        case '*':
            result.integer = a.integer * b.integer;
            break;
        case '/':
            result.integer = a.integer / b.integer;
            break;
        default: {
            long long base = a.integer;
            long long exponent = b.integer;
            result.integer = 1;

            /* Squaring takes logarithmic time. Skip the unused final
             * square. The assignment guarantees integer results fit.
             */
            while (exponent > 0) {
                if (exponent % 2 != 0) {
                    result.integer *= base;
                }

                exponent /= 2;

                if (exponent > 0) {
                    base *= base;
                }
            }

            break;
        }
    }

    return result;
}

static Value expression(Parser *p);

static Value primary(Parser *p)
{
    Value v = zero_value();
    size_t start = p->token_pos;

    if (p->token == NUMBER) {
        v = p->number;
        next_token(p);
    } else if (p->token == '(') {
        next_token(p);
        v = expression(p);

        if (!p->error) {
            if (p->token != ')') {
                fail(p, p->token_pos);
            } else {
                next_token(p);
            }
        }

        v.start = start;
    } else {
        /* Missing operands, unary signs and empty parentheses fail
         * at the token currently being inspected.
         */
        fail(p, start);
    }

    return v;
}

/* Store the power chain and evaluate backwards.
 * This gives right associativity without one recursive call per '**'.
 * Recursion is still used for nested parentheses.
 */
typedef struct {
    Value val;
    size_t op_pos;
} PowerPart;

static Value power(Parser *p)
{
    Value v = primary(p);

    if (p->error || p->token != POWER) {
        return v;
    }

    size_t count = 0;
    size_t capacity = 8;
    PowerPart *parts = allocate(NULL, capacity * sizeof(*parts));

    while (!p->error && p->token == POWER) {
        if (count == capacity) {
            capacity = grow(capacity, sizeof(*parts));
            parts = allocate(parts, capacity * sizeof(*parts));
        }

        parts[count].val = v;
        parts[count].op_pos = p->token_pos;
        count++;
        next_token(p);
        v = primary(p);
    }

    while (!p->error && count > 0) {
        count--;
        v = calculate(p, parts[count].val, POWER, v, parts[count].op_pos);
    }

    free(parts);
    return v;
}

/* Powers are completed before multiplication and division.
 * Folding the result in a loop makes these operators left-associative.
 */
static Value term(Parser *p)
{
    Value v = power(p);

    while (!p->error && (p->token == '*' || p->token == '/')) {
        int op = p->token;
        size_t operator_pos = p->token_pos;
        next_token(p);

        Value right = power(p);

        if (!p->error) {
            v = calculate(p, v, op, right, operator_pos);
        }
    }

    return v;
}

/* Each operand is a complete term, giving * and / priority over + and -.
 */
static Value expression(Parser *p)
{
    Value v = term(p);

    while (!p->error && (p->token == '+' || p->token == '-')) {
        int op = p->token;
        size_t operator_pos = p->token_pos;
        next_token(p);

        Value right = term(p);

        if (!p->error) {
            v = calculate(p, v, op, right, operator_pos);
        }
    }

    return v;
}

/* Reuse a buffer that doubles when full.
 * Memory depends on the longest line, and reading is linear in file size.
 * consumed includes '\n'; length excludes it.
 */
static int read_line(FILE *file, char **line, size_t *capacity,
                     size_t *length, size_t *consumed)
{
    *length = 0;
    *consumed = 0;

    int c;

    while ((c = fgetc(file)) != EOF) {
        (*consumed)++;

        if (c == '\n') {
            break;
        }

        if (*length + 1 >= *capacity) {
            *capacity = grow(*capacity, 1);
            *line = allocate(*line, *capacity);
        }

        (*line)[(*length)++] = (char)c;
    }

    (*line)[*length] = '\0';

    if (ferror(file)) {
        return -1;
    }

    return *consumed != 0;
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

    char *path = allocate(NULL, size);

    memcpy(path, outdir, directory_length);
    path[directory_length] = '/';
    memcpy(path + directory_length + 1, base, base_length);
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

    FILE *in = fopen(input, "rb");

    if (in == NULL) {
        perror(input);
        return 1;
    }

    if (make_output_directory(outdir) != 0) {
        fclose(in);
        return 1;
    }

    /* Stage results on disk. An expression error discards every
     * earlier result without storing the whole output in memory.
     */
    FILE *results = tmpfile();

    if (results == NULL) {
        perror("Temporary file");
        fclose(in);
        return 1;
    }

    size_t capacity = 256;
    char *line = allocate(NULL, capacity);
    size_t length, consumed;
    size_t base = 0;
    size_t error = 0;
    int status = 0;
    int read_status;

    while ((read_status = read_line(in, &line, &capacity,
                                   &length, &consumed)) > 0) {
        Parser p = {0};
        p.text = line;
        p.length = length;
        p.base = base;

        next_token(&p);

        size_t first = p.token_pos - base - 1;

        /* Only a leading '#' marks a comment. A later '#' is invalid.
         * Comments and blank lines still count toward file positions.
         */
        if (p.token != END && line[first] != '#') {
            Value v = expression(&p);

            if (!p.error && p.token != END) {
                fail(&p, p.token_pos);
            }

            if (p.error) {
                error = p.error;
                break;
            }

            int written = v.floating
                ? fprintf(results, "%.15g\n", v.real)
                : fprintf(results, "%lld\n", v.integer);
            if (written < 0) {
                perror("Temporary output");
                status = 1;
                break;
            }
        }

        base += consumed;
    }

    if (read_status < 0) {
        fprintf(stderr, "Cannot read input: %s\n", input);
        status = 1;
    }

    free(line);

    if (fclose(in) != 0) {
        perror(input);
        status = 1;
    }

    if (!status && !error && fseek(results, 0, SEEK_SET) != 0) {
        perror("Temporary output");
        status = 1;
    }

    char *output = make_output_path(input, outdir);

    /* Open the destination only after reading and closing the input. */
    if (!status) {
        FILE *out = fopen(output, "wb");

        if (out == NULL) {
            perror(output);
            status = 1;
        } else {
            if (error) {
                if (fprintf(out, "ERROR:%zu\n", error) < 0) {
                    status = 1;
                }
            } else {
                char buffer[8192];
                size_t n;

                while ((n = fread(buffer, 1, sizeof(buffer), results)) > 0) {
                    if (fwrite(buffer, 1, n, out) != n) {
                        status = 1;
                        break;
                    }
                }

                if (ferror(results)) {
                    status = 1;
                }
            }

            if (fclose(out) != 0) {
                status = 1;
            }

            if (status) {
                fprintf(stderr, "Cannot write output: %s\n", output);
            }
        }
    }

    if (fclose(results) != 0) {
        perror("Temporary output");
        status = 1;
    }

    free(output);

    /* Expression errors are normal results, so they leave status at 0. */
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

    if (input == NULL || outdir == NULL) {
        goto usage;
    }

    return process_file(input, outdir);

usage:
    fprintf(stderr, "Usage: %s -o OUTDIR input.txt\n", argv[0]);
    return 1;
}
