#include <stdio.h>

int main() {
    int n;
    FILE *file;

    // Take user input
    printf("Enter an integer: ");
    scanf("%d", &n);

    // Open file in write mode
    file = fopen("sample.txt", "w");
    if (file == NULL) {
        printf("Error opening file!\n");
        return 1;
    }

    // Write numbers to file
    for (int i = 1; i <= n; i++) {
        fprintf(file, "%d", i);
        if (i < n) {
            fprintf(file, " ");  // Add space except after the last number
        }
    }

    // Close the file
    fclose(file);
    printf("First %d numbers written to sample.txt\n", n);

    return 0;
}
