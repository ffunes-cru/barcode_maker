#include <stdio.h>
#include "lib/img/libattopng.h"
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include <math.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#define ARRAY_LEN 107
#define HEIGHT 50
#define HEIGHT_TXT 10
#define PADDING 10
#define RES_MULT 4

typedef struct {
    int value;
    char ascii;
    char *pattern;
} Code128Char; 

typedef struct {
    Code128Char** valueix;
    Code128Char** charix;
} Code128Dict; 

// punto de la font
typedef struct {
    int x;  
    int y;
    unsigned char v;
} BitmapPoint;

// matriz vectorial de font
typedef struct {
    BitmapPoint *points;
    size_t count; 
    int width;
    int height;
} BitmapList;

typedef struct {
    FT_Library library;
    FT_Face face;
} BitmapInitData; 


//GESTIÓN DE ERRORES
#define DIE_ON_ERROR(err, msg) \
    if (err) { \
        fprintf(stderr, "%s (Error FreeType: 0x%x)\n", msg, err); \
        return NULL; \
    }


/**
 * @brief Lee una fuente OTF, rasteriza el texto dado y devuelve una lista de píxeles (x, y, valor de gris).
 * @param font_path Ruta al archivo .otf o .ttf.
 * @param text_to_render La cadena de texto a convertir a bitmap.
 * @param font_size_px Tamaño de la fuente en píxeles.
 * @return Puntero a la estructura BitmapList con los datos de píxeles, o NULL en caso de error.
 */
BitmapList* render_text_to_bitmap_list(const char *font_path, const char *text_to_render, int font_size_px, BitmapInitData bitmap_data) {
    FT_Library library = bitmap_data->library;
    FT_Face face = bitmap_data->face;
    FT_Error error;

    // Establecer el tamaño de la fuente (en píxeles)
    error = FT_Set_Pixel_Sizes(face, 0, font_size_px);
    DIE_ON_ERROR(error, "Error al establecer el tamaño de la fuente");

    // Inicializar la lista de resultados
    BitmapList *result_list = (BitmapList *)malloc(sizeof(BitmapList));
    if (!result_list) {
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return NULL;
    }
    result_list->points = NULL;
    result_list->count = 0;
    result_list->width = 0;
    result_list->height = 0; // La altura se determinará por la altura máxima del carácter

    // El pen representa la posición actual de dibujo
    FT_Int pen_x = 0;
    FT_Int pen_y = 0;
    FT_Int max_height = 0;

    // Primer paso: Calcular el ancho y alto total de la cadena
    const char *p;
    for (p = text_to_render; *p; p++) {
        // Cargar el glifo para el carácter actual
        error = FT_Load_Char(face, *p, FT_LOAD_RENDER);
        if (error) continue; // Saltar caracteres que no se puedan cargar

        // Calcular el ancho total sumando el avance (advance)
        pen_x += face->glyph->advance.x >> 6;
        
        // Determinar la altura máxima de todos los glifos
        if (face->glyph->bitmap.rows > max_height) {
            max_height = face->glyph->bitmap.rows;
        }
    }
    
    result_list->width = pen_x;
    result_list->height = max_height;
    
    // Segundo paso: Rasterizar y copiar los píxeles
    pen_x = 0; // Resetear la posición X
    
    // Este buffer temporal podría ser grande, es mejor hacerlo de forma dinámica
    // o procesar por carácter, que es lo que haremos:
    
    // Alocamos un buffer grande (tamaño heurístico) y lo ajustamos al final
    // Estimación: ancho total * alto total * factor de seguridad 1.5
    size_t initial_capacity = (size_t)result_list->width * result_list->height * 1.5;
    result_list->points = (BitmapPoint *)malloc(initial_capacity * sizeof(BitmapPoint));
    if (!result_list->points) {
        free(result_list);
        FT_Done_Face(face);
        FT_Done_FreeType(library);
        return NULL;
    }

    for (p = text_to_render; *p; p++) {
        error = FT_Load_Char(face, *p, FT_LOAD_RENDER);
        if (error) continue;

        FT_Bitmap *bitmap = &face->glyph->bitmap;
        FT_Int bitmap_left = face->glyph->bitmap_left;
        FT_Int bitmap_top = face->glyph->bitmap_top;
        
        // Recorrer el bitmap del glifo actual
        for (int y = 0; y < bitmap->rows; y++) {
            for (int x = 0; x < bitmap->width; x++) {
                
                // La altura del glifo se ajusta a la línea base
                // bitmap_top: desplazamiento desde la línea base hasta la parte superior del bitmap
                // max_height - bitmap_top: desplazamiento desde la parte superior del área de dibujo
                int final_y = max_height - bitmap_top + y; 
                int final_x = pen_x + bitmap_left + x;
                
                // El valor de gris (v) se toma del buffer del bitmap (modo FT_LOAD_RENDER es escala de grises)
                unsigned char gray_value = bitmap->buffer[y * bitmap->pitch + x];

                if (gray_value > 0) {
                    // Solo guardamos píxeles que no son blancos (valor > 0)
                    
                    if (result_list->count < initial_capacity) {
                        BitmapPoint *current_point = &result_list->points[result_list->count];
                        current_point->x = final_x;
                        current_point->y = final_y;
                        current_point->v = abs(gray_value - 255);
                        result_list->count++;
                    } else {
                        // Si la capacidad inicial no fue suficiente, habría que reallocar.
                        // Para este ejemplo, asumiremos que initial_capacity es suficiente.
                        fprintf(stderr, "Advertencia: Capacidad de buffer excedida.\n");
                    }
                }
            }
        }
        
        // Mover el lápiz a la derecha para el siguiente carácter
        pen_x += face->glyph->advance.x >> 6;
    }

    // Opcional: Reducir la memoria al tamaño exacto de píxeles encontrados
    result_list->points = (BitmapPoint *)realloc(result_list->points, result_list->count * sizeof(BitmapPoint));

    // Liberar recursos de FreeType
    FT_Done_Face(face);
    FT_Done_FreeType(library);

    return result_list;
}

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
        if (input[i-1] == ' ') {
            temp_char = dicc['b'];
        }else {
            temp_char = dicc[input[i-1]];
        }

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

void save_code128(char* texto, Code128Dict dict) {

    char* code = create_code(texto, dict.charix, dict.valueix);

    printf("%s", code);
    int code_len = strlen(code);

    int image_width = (code_len+1 + 2)*RES_MULT;
    int image_height = (HEIGHT + HEIGHT_TXT + PADDING)*RES_MULT;

    libattopng_t* png = libattopng_new(image_width, image_height, PNG_GRAYSCALE);

    int x, y;
    for (x = 0; x < code_len*RES_MULT; x++) {
        for (y = PADDING*RES_MULT; y < HEIGHT*RES_MULT; y++) {
            switch (code[(int)floor(x / RES_MULT)]) {
                case '1':
                    libattopng_set_pixel(png, x, y, 255);
                    break;
                case '0':
                    libattopng_set_pixel(png, x, y, 0);
                    break;
            }
        }
    }

    BitmapList *list = render_text_to_bitmap_list("test.otf", texto, HEIGHT_TXT*RES_MULT);

    for (size_t i = 0; i < list->count && i < list->count; i++) {
        libattopng_set_pixel(png, list->points[i].x + (PADDING)*RES_MULT, list->points[i].y + (HEIGHT + PADDING)*RES_MULT, list->points[i].v);
        //printf("(%d, %d, %d) ", list->points[i].x, list->points[i].y, list->points[i].v);
    }
    
    //barra adicional
    for (y = PADDING*RES_MULT; y < HEIGHT*RES_MULT; y++) {
        for (x = 0 ; x < RES_MULT ; x++) {
            libattopng_set_pixel(png, code_len*RES_MULT+x, y, 0);
        }
    }

    free(code);

    free(list->points);
    free(list);

    libattopng_save(png, "test_rgba.png");
    libattopng_destroy(png);
}

BitmapInitData *init_bitmap() {

    FT_Library library;
    FT_Face face;

    BitmapInitData* bitmap = malloc(sizeof(BitmapInitData));
    // Inicialización del motor FreeType
    error = FT_Init_FreeType(&library);
    DIE_ON_ERROR(error, "Error al inicializar FreeType");

    // Carga de la fuente desde el archivo
    error = FT_New_Face(library, font_path, 0, &face);
    DIE_ON_ERROR(error, "Error al cargar la fuente OTF/TTF");
    
    bitmap->library = library;
    bitmap->face = face;
    
    return bitmap
}

Code128Dict init_code128() {
    Code128Dict dict;
    Code128Char **code128_db_charix, **code128_db_valueix;
    code128_db_charix = (Code128Char**) csv_read((void*) code128_fromstr, (void*) code128_hashf_char, "code128char.txt", ARRAY_LEN);
    code128_db_valueix = (Code128Char**) csv_read((void*) code128_fromstr, (void*) code128_hashf_int, "code128int.txt", ARRAY_LEN);
    dict.valueix = code128_db_valueix;
    dict.charix = code128_db_charix;
    return dict;
}

void free_inits(Code128Dict dict, BitmapInitData* bitmap) {
    free(dict.valueix);
    free(dict.charix);
    free(bitmap);
}

int main() {
    
    #define RGB(r, g, b) ((r) | ((g) << 8) | ((b) << 16))

    Code128Char **code128_db_charix, **code128_db_valueix;
    code128_db_charix = (Code128Char**) csv_read((void*) code128_fromstr, (void*) code128_hashf_char, "code128char.txt", ARRAY_LEN);
    code128_db_valueix = (Code128Char**) csv_read((void*) code128_fromstr, (void*) code128_hashf_int, "code128int.txt", ARRAY_LEN);
    char texto[] = "A1010";


    char* code = create_code(texto, code128_db_charix, code128_db_valueix);

    printf("%s", code);
    int code_len = strlen(code);
    code128_dest(code128_db_charix);
    code128_dest(code128_db_valueix);

    int image_width = (code_len+1 + 2)*RES_MULT;
    int image_height = (HEIGHT + HEIGHT_TXT + PADDING)*RES_MULT;

    libattopng_t* png = libattopng_new(image_width, image_height, PNG_GRAYSCALE);

    int x, y;
    for (x = 0; x < code_len*RES_MULT; x++) {
        for (y = PADDING*RES_MULT; y < HEIGHT*RES_MULT; y++) {
            switch (code[(int)floor(x / RES_MULT)]) {
                case '1':
                    libattopng_set_pixel(png, x, y, 255);
                    break;
                case '0':
                    libattopng_set_pixel(png, x, y, 0);
                    break;
            }
        }
    }

    BitmapList *list = render_text_to_bitmap_list("test.otf", texto, HEIGHT_TXT*RES_MULT, bitmap_data);

    for (size_t i = 0; i < list->count && i < list->count; i++) {
        libattopng_set_pixel(png, list->points[i].x + (PADDING)*RES_MULT, list->points[i].y + (HEIGHT + PADDING)*RES_MULT, list->points[i].v);
        //printf("(%d, %d, %d) ", list->points[i].x, list->points[i].y, list->points[i].v);
    }
    
    //barra adicional
    for (y = PADDING*RES_MULT; y < HEIGHT*RES_MULT; y++) {
        for (x = 0 ; x < RES_MULT ; x++) {
            libattopng_set_pixel(png, code_len*RES_MULT+x, y, 0);
        }
    }

    free(code);

    free(list->points);
    free(list);

    libattopng_save(png, "test_rgba.png");
    libattopng_destroy(png);
}
