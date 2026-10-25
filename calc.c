// Mehmet Burak Duman 241ADB160
// https://github.com/MBurakDuman/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

enum { END = 256, NUMBER, INVALID };

typedef struct {
    const char *text;
    size_t index;
    int token;
    long long number;
    int error;
} Parser;

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

    if (c == '+' || c == '-' || c == '*' ||
        c == '/' || c == '(' || c == ')') {
        p->token = c;
    } else {
        p->token = INVALID;
    }
}

static long long primary(Parser *p)
{
    if (p->token != NUMBER) {
        p->error = 1;
        return 0;
    }

    long long value = p->number;
    next_token(p);

    return value;
}

static long long expression(Parser *p)
{
    long long result = primary(p);

    while (!p->error && (p->token == '+' || p->token == '-')) {
        int op = p->token;
        next_token(p);

        long long right = primary(p);

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

int main(void)
{
    printf("Calculator started.\n");

    FILE *file = fopen("input.txt", "r");

    if (file == NULL) {
        perror("Cannot open input.txt");
        return 1;
    }

    char line[1024];
    int line_number = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        line_number++;
        printf("Line %d: %s", line_number, line);

        Parser p = {0};
        p.text = line;

        next_token(&p);

        long long result = expression(&p);

        if (p.error || p.token != END) {
            printf("  ERROR\n");
        } else {
            printf("  RESULT: %lld\n", result);
        }
    }

    if (ferror(file)) {
        perror("Cannot read input.txt");
        fclose(file);
        return 1;
    }

    if (fclose(file) != 0) {
        perror("Cannot close input.txt");
        return 1;
    }

    return 0;
}