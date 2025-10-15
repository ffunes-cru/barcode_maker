#include <stdio.h>
#include "lib/img/libattopng.h"
#include <stdlib.h>
#include <string.h>

#define ARRAY_LEN 107
#define HEIGHT 80 

typedef struct {
    int value;
    char ascii;
    char *pattern;
} Code128Char; 

Code128Char* code128_fromstr(char* line) {
    Code128Char *temp = malloc(sizeof(Code128Char));
    int j = 0;
    int k = 0;
    char ntemp[3];
    char* temp_pattern = malloc(sizeof(char)*14);
    for (int i = 0 ; line[i] != '\0' ; i++) {
        if (line[i] == ',') {
            j++;
            k = 0;
        }else{
            switch (j) {
                case 0:
                    //printf("0");
                    ntemp[k++] = line[i];
                    break;
                case 1:
                    //printf("1");
                    temp->ascii = (int) line[i];
                    break;
                case 2:
                    //printf("2");
                    temp_pattern[k++] = line[i];
                    break;
            }
        }
    }
    temp->value = atoi(ntemp);
    temp->pattern = temp_pattern;
    printf("value: %d, ascii: %c, pattern: %s", temp->value, temp->ascii, temp->pattern);
    return temp;
}

int code128_hashf_char(Code128Char* code128) {
    if (code128 != NULL) {
        return (int) code128->ascii;
    }
    return -1;
}

int code128_hashf_int(Code128Char* code128) {
    if (code128 != NULL) {
        return code128->value;
    }
    return -1;
}

void** csv_read(void* *f(char* line), int hash_f(void* st), char* csv_file, int max_lines) {
    FILE *fptr;

    void **general_st = malloc(sizeof(void*)*max_lines);  //ponerlo como constante

    fptr = fopen(csv_file, "r");

    if (fptr == NULL) {
        printf("Error: Could not open file %s\n", csv_file);
        exit(1);
    }

    char *line_buffer = malloc(sizeof(char)*256);
    void *temp;
    int i = 0;

    while (fscanf(fptr, "%s\n", line_buffer) != -1) {
        temp = f(line_buffer);
        general_st[hash_f(temp)] = temp;
    };

    free(line_buffer);

    fclose(fptr); // Close the file
    return general_st;
}

void code128_dest(Code128Char **code128) {

    for (int i = 0 ; i < ARRAY_LEN ; i++) {
        if (code128[i] != NULL) {
            free(code128[i]->pattern);
            free(code128[i]);
        }
    }

    free(code128);
}

char* create_code(char* input, Code128Char** dicc, Code128Char** dicc_intix) {
    int inp_len = strlen(input);
    char* output = malloc(sizeof(char)*((inp_len + 3)*11)+1);
    Code128Char* temp_char;
    Code128Char* START = dicc['#'];
    Code128Char* STOP = dicc['$'];
    int i = 1; int check_sum = 0 ; int check_value;

    if (START == NULL || STOP == NULL) {
        printf("No hay START ni STOP");
        return NULL;
    }
    //Start Symbol
    for (int j = 0 ; j < 11 ; j++) {
        output[j] = START->pattern[j];
    }
    check_sum = START->value;
    printf("%s\n", START->pattern);
    //Encoded Data
    for (; i <= inp_len ; i++) {
        temp_char = dicc[input[i-1]];

        if (temp_char == NULL) {
            printf("El caracter no existe en el diccionario\n");
            return NULL;
        }

        check_sum += temp_char->value * i;

        for (int j = 0 ; j < 11 ; j++) {
            output[i*11 + j] = temp_char->pattern[j];
        }
        printf("%s\n", temp_char->pattern);
        //printf("%c\n", temp_char->ascii);
    }
    //CheckDigit
    printf("%d\n", check_sum);
    check_value = check_sum % 103;
    temp_char = dicc_intix[check_value];
    printf("%s\n", temp_char->pattern);
    printf("%d\n", check_value);
    for (int j = 0 ; j < 11 ; j++) {
        output[i*11 + j] = temp_char->pattern[j];
    }
    i++;
    //Stop Symbol
    for (int j = 0 ; j < 11 ; j++) {
        output[i*11 + j] = STOP->pattern[j];
    }
    printf("%s\n", STOP->pattern);
    output[(inp_len + 3)*11] = '\0';
    return output;
}

int main() {
    
    #define RGBA(r, g, b, a) ((r) | ((g) << 8) | ((b) << 16) | ((a) << 24))

    Code128Char **code128_db_charix, **code128_db_valueix;
    code128_db_charix = (Code128Char**) csv_read((void*) code128_fromstr, (void*) code128_hashf_char, "code128char.txt", ARRAY_LEN);
    code128_db_valueix = (Code128Char**) csv_read((void*) code128_fromstr, (void*) code128_hashf_int, "code128int.txt", ARRAY_LEN);

    char* code = create_code("K00006530", code128_db_charix, code128_db_valueix);

    printf("%s", code);
    int code_len = strlen(code);
    code128_dest(code128_db_charix);
    code128_dest(code128_db_valueix);

    libattopng_t* png = libattopng_new(code_len+1, HEIGHT, PNG_RGBA);

    int x, y;
    for (x = 0; x < code_len; x++) {
        for (y = 0; y < HEIGHT; y++) {
            switch (code[x]) {
                case '1':
                    libattopng_set_pixel(png, x, y, RGBA( 0 , 0 , 0 , 255));
                    break;
                case '0':
                    libattopng_set_pixel(png, x, y, RGBA( 255 , 255 , 255 , 255));
                    break;
            }
        }
    }

    //barra adicional
    for (y = 0; y < HEIGHT; y++) {
        libattopng_set_pixel(png, code_len, y, RGBA( 0 , 0 , 0 , 255));
    }

    free(code);

    libattopng_save(png, "test_rgba.png");
    libattopng_destroy(png);
}
