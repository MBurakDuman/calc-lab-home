// Mehmet Burak Duman 241ADB160
// https://github.com/MBurakDuman/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: ChatGpt for corrections and explanations

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

enum { END = 256, NUMBER, POWER, INVALID };

typedef struct {
    const char *text;
    size_t index;
    int token;
    long long number;
    int error;
} Parser;

/* Developed with ChatGPT assistance.
 * index points to the next unread character. Each call consumes one
 * token and leaves it available for the parser to inspect.
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

    /* Recognize two adjacent stars as one token before accepting '*'. */
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

/* A parenthesized expression is evaluated as a single operand.
 * Only numbers and '(' may begin an operand, so unary signs are rejected.
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

/* Developed with ChatGPT assistance using the assignment's grammar.
 * Calling power() for the exponent groups 2**3**2 as 2**(3**2).
 * This recursive version will be replaced for very long power chains.
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

    /* Integer multiplication preserves exact integer results.
     * Squaring reduces the number of steps for large exponents.
     * Avoid the final unused square, which could otherwise overflow.
     * The assignment guarantees that integer arithmetic fits long long.
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

/* Reading powers first gives ** higher priority than * and /.
 * The loop evaluates multiplication and division left to right.
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
            /* Check before dividing to avoid undefined behaviour.
             * Integer division in C truncates toward zero as required.
             */
            if (right == 0) {
                p->error = 1;
                return 0;
            }

            result /= right;
        }
    }

    return result;
}

/* Reading a complete term for each operand gives * and / priority.
 * Updating result in a loop makes + and - left-associative.
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

int main(void)
{
    printf("Calculator started.\n");

    FILE *file = fopen("input.txt", "r");

    if (file == NULL) {
        perror("Cannot open input.txt");
        return 1;
    }

    /* Temporary development buffer.
     * Dynamic line reading will replace this before submission.
     */
    char line[1024];
    int line_number = 0;

    while (fgets(line, sizeof(line), file) != NULL) {
        line_number++;
        printf("Line %d: %s", line_number, line);

        /* Keep diagnostics on a separate line even when the final
         * input line has no newline character.
         */
        if (strchr(line, '\n') == NULL) {
            printf("\n");
        }

        Parser p = {0};
        p.text = line;

        next_token(&p);

        long long result = expression(&p);

        /* Remaining tokens mean the whole expression was not accepted,
         * for example an extra ')' or two adjacent numbers.
         */
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