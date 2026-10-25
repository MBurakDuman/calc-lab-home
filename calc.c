// Mehmet Burak Duman 241ADB160

#include <stdio.h>

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