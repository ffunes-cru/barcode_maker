#include <stdio.h>
#include "lib/img/libattopng.h"
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include <math.h>
#include <unistd.h>
#include <getopt.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#define CODE128_ARRAY_LEN 107

#define DEFAULT_HEIGHT 20
#define DEFAULT_HEIGHT_TXT 7
#define DEFAULT_PADDING_X 5
#define DEFAULT_PADDING_Y 2
#define DEFAULT_PADDING_TEXT_Y 2
#define DEFAULT_RES_MULT 4
#define DEFAULT_COMP_FACT 2


typedef struct {
    int array_len;
    int height;
    char input[255];
    char input_file[255];
    char output_dir[255];
    int height_txt;
    int padd_x;
    int padd_y;
    int padd_txt_y;
    int res_fact;
    int comp_fact;
} Parameters;

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

/*
void obtener_ruta_completa(char *ruta_archivo_out, const char *nombre_archivo) {
    char ruta_exe[MAX_PATH];
    // Obtiene la ruta COMPLETA del ejecutable actual.
    GetModuleFileNameA(NULL, ruta_exe, MAX_PATH);

    // Encuentra la última barra (separador de directorio)
    char *ultimo_separador = strrchr(ruta_exe, '\\');
    if (ultimo_separador) {
        // Trunca la cadena para dejar solo el directorio
        *(ultimo_separador + 1) = '\0';
    }

    // Combina la ruta del directorio con el nombre del archivo relativo
    snprintf(ruta_archivo_out, MAX_PATH, "%s%s", ruta_exe, nombre_archivo);
}
*/

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
BitmapList* render_text_to_bitmap_list(const char *font_path, const char *text_to_render, int font_size_px) {
    FT_Library library;
    FT_Face face;
    FT_Error error;

    BitmapInitData* bitmap = malloc(sizeof(BitmapInitData));
    // Inicialización del motor FreeType
    error = FT_Init_FreeType(&library);
    DIE_ON_ERROR(error, "Error al inicializar FreeType");

    // Carga de la fuente desde el archivo
    error = FT_New_Face(library, "font.otf", 0, &face);
    DIE_ON_ERROR(error, "Error al cargar la fuente OTF/TTF");

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

    free(bitmap);
    return result_list;
}

Code128Char* code128_fromstr(char* line) {
    Code128Char *temp = malloc(sizeof(Code128Char));
    int j = 0;
    int k = 0;
    int k_val = 0;
    char ntemp[3] = {0};
    char* temp_pattern = malloc(sizeof(char)*14);
    for (int i = 0 ; line[i] != '\0' ; i++) {
        if (line[i] == ',') {
            j++;
            if (j == 1) ntemp[k_val] = '\0';
            k = 0;
        }else{
            switch (j) {
                case 0:
                    //printf("0");
                    ntemp[k++] = line[i];
                    k_val = k;
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
    ntemp[3] = '\0';
    temp_pattern[k] = '\0';
    temp->value = atoi(ntemp);
    temp->pattern = temp_pattern;
    //printf("value: %d, ascii: %c, pattern: %s", temp->value, temp->ascii, temp->pattern);
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
    printf( "ERROR");
    return -1;
}


void** csv_read(void* *f(char* line), int hash_f(void* st), char* csv_file, int max_lines, char isHash) {
    FILE *fptr;

    void **general_st = calloc(max_lines, sizeof(void*)); 

    fptr = fopen(csv_file, "r");

    if (fptr == NULL) {
        printf("Error: Could not open file %s\n", csv_file);
        exit(1);
    }

    char *line_buffer = malloc(sizeof(char)*256);
    void *temp;
    int i = 0;
    
    while (fscanf(fptr, "%[^\n]", line_buffer) != -1) {
        //printf("%s", line_buffer);
        temp = f(line_buffer);
        if (isHash) {
            general_st[hash_f(temp)] = temp;
        }else {
            general_st[i++] = temp;
        }
        getc(fptr);
    };
    //printf("%d\n", i);

    free(line_buffer);

    fclose(fptr); // Close the file
    return general_st;
}

void code128_dest(Code128Char **code128) {

    for (int i = 0 ; i < CODE128_ARRAY_LEN ; i++) {
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
    //printf("%s\n", START->pattern);
    //Encoded Data
    for (; i <= inp_len ; i++) {
        if (input[i-1] == ' ') {
            temp_char = dicc['b'];
        }else {
            //printf("\n%c\n\n", input[i-1]);
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
        //printf("%s\n", temp_char->pattern);
        //printf("%c\n", temp_char->ascii);
    }
    //CheckDigit
    check_value = check_sum % 103;
    temp_char = dicc_intix[check_value];
    for (int j = 0 ; j < 11 ; j++) {
        output[i*11 + j] = temp_char->pattern[j];
    }
    //printf("\n", temp_char->pattern);
    i++;
    //Stop Symbol
    for (int j = 0 ; j < 11 ; j++) {
        output[i*11 + j] = STOP->pattern[j];
    }
    //printf("%s\n", STOP->pattern);
    output[(inp_len + 3)*11] = '\0';
    return output;
}

void save_code128(char* texto, Code128Dict dict, BitmapInitData* bitmap, char* filename, Parameters param) {


    if (texto == NULL) {
        fprintf(stderr, "Advertencia: texto de entrada es NULL para %s. Saltando.\n", filename);
        printf("%s\n", stderr);
        return; 
    }

    char* code = create_code(texto, dict.charix, dict.valueix);

    if (code == NULL) {
        fprintf(stderr, "Error: No se pudo crear el código para '%s'.\n", texto);
        printf("%s\n", stderr);
        return; // O hacer la limpieza parcial y retornar
    }

    int code_len = strlen(code);


    int code_res_fac = (int) floor(( param.res_fact / param.comp_fact ));
    int image_width = (code_len+1 + (param.padd_x*2))*code_res_fac;
    int image_height = (param.height + (param.height_txt - (param.padd_txt_y*2)) + (param.padd_y*2))*param.res_fact;
    //int code_res_fac = RES_MULT;
    //printf("%d", code_res_fac);

    libattopng_t* png = libattopng_new(image_width, image_height, PNG_GRAYSCALE);

    int x, y=1;
    for (x = param.padd_x*code_res_fac; x < (code_len+param.padd_x)*code_res_fac; x++) {
        for (y = param.padd_y*param.res_fact; y < param.height*param.res_fact; y++) {
            switch (code[(int)floor((x - (param.padd_x*code_res_fac)) / code_res_fac)]) {
                case '1':
                    //printf(" x  : x: %d , ix: %d", x, (int)floor((x - (PADDING*RES_MULT)) / RES_MULT));
                    libattopng_set_pixel(png, x, y, 0);
                    break;
                case '0':
                    //printf(" 0  : x: %d , ix: %d", x, (int)floor((x - (PADDING*RES_MULT)) / RES_MULT));
                    libattopng_set_pixel(png, x, y, 255);
                    break;
            }
        }
        //printf("\n");
    }

    BitmapList *list = render_text_to_bitmap_list("test.otf", texto, (param.height_txt - param.padd_txt_y)*param.res_fact);

    int center_pad = (int) (image_width / 2) - (list->width / 2);

    for (size_t i = 0; i < list->count; i++) {
        libattopng_set_pixel(png, list->points[i].x + center_pad, list->points[i].y + (param.height + param.padd_txt_y)*param.res_fact, list->points[i].v);
        //printf("(%d, %d, %d) ", list->points[i].x, list->points[i].y, list->points[i].v);
    }
    
    //barra adicional
    for (y = param.padd_y*param.res_fact; y < param.height*param.res_fact; y++) {
        for (x = 0 ; x < code_res_fac ; x++) {
            libattopng_set_pixel(png, (param.padd_x+code_len)*code_res_fac+x, y, 0);
            libattopng_set_pixel(png, (param.padd_x+code_len)*code_res_fac+x+1, y, 0);
        }
    }

    for (x = 0 ; x < image_width ; x++) {
        //libattopng_set_pixel(png, x, image_height, 0);
        libattopng_set_pixel(png, x, 0, 0);
    }


    free(code);

    free(list->points);
    free(list);

    libattopng_save(png, filename);
    libattopng_destroy(png);
}

BitmapInitData *init_bitmap() {

    BitmapInitData* bitmap = malloc(sizeof(BitmapInitData));
    
    return bitmap;
}

Code128Dict init_code128() {
    Code128Dict dict;
    Code128Char **code128_db_charix, **code128_db_valueix;

    code128_db_charix = (Code128Char**) csv_read((void*) code128_fromstr, (void*) code128_hashf_char, "code128char.txt", CODE128_ARRAY_LEN, 1);
    code128_db_valueix = (Code128Char**) csv_read((void*) code128_fromstr, (void*) code128_hashf_int, "code128int.txt", CODE128_ARRAY_LEN, 1);
    dict.valueix = code128_db_valueix;
    dict.charix = code128_db_charix;
    return dict;
}

void free_inits(Code128Dict dict, BitmapInitData* bitmap) {
    code128_dest(dict.valueix);
    code128_dest(dict.charix);
    free(bitmap);
}


char* process_txtline(char* line) {
    int temp_len = 10;
    char* string = malloc(sizeof(char)*temp_len);
    int i;
    for (i = 0 ; line[i] != '\0'; i++) {
        if (i >= temp_len) {
            temp_len+= 10;
            string = realloc(string, sizeof(char)*(temp_len+1));
        }
        string[i] = line[i];
    }
    string = realloc(string, sizeof(char)*(i + 1));
    string[i] = '\0';
    return string;
}

void print_help_and_exit(const char *prog_name, int rec_value) {
    fprintf(stdout, "\n");
    fprintf(stdout, "Usage: %s [GENERAL OPTIONS] [CONFIGURATION OPTIONS]\n", prog_name);
    fprintf(stdout, "Description: Creates Code128 images with certain configuration parameters.\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "General Options:\n");
    fprintf(stdout, "  -s, --input <string>    Uses console input for creating single a Code128 image.\n");
    fprintf(stdout, "  -c, --input-file <path>    Route to input, each string in the file must be separated by \\n.\n");
    fprintf(stdout, "  -o, --output-dir <path>    Code128 image/s output directory.\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Configuration Options:\n");
    fprintf(stdout, "  -A, --array-len <val>      Define input array max length (MANDATORY).\n");
    fprintf(stdout, "  -H, --height <val>         Define height (Default: %d).\n", DEFAULT_HEIGHT);
    fprintf(stdout, "  -T, --height-txt <val>     Define text height (Default: %d).\n", DEFAULT_HEIGHT_TXT);
    fprintf(stdout, "  -X, --padd-x <val>         Define x padding (Default: %d).\n", DEFAULT_PADDING_X);
    fprintf(stdout, "                             WARNING: Be aware that lower values may eliminate the quiet zone, rendering the code unreadable.\n");
    fprintf(stdout, "  -Y, --padd-y <val>         Define y padding (Default: %d).\n", DEFAULT_PADDING_Y);
    fprintf(stdout, "  -y, --padd-txt-y <val>     Define y padding for text (Default: %d).\n", DEFAULT_PADDING_TEXT_Y);
    fprintf(stdout, "  -R, --res-fact <val>       Define integer scaling factor (Default: %d).\n", DEFAULT_RES_MULT);
    fprintf(stdout, "  -C, --comp-fact <val>      Define code128 compression factor (Value must be <= --res-fact) (Default: %d).\n", DEFAULT_COMP_FACT);
    // Añade el resto de tus opciones largas aquí...
    fprintf(stdout, "\n");
    fprintf(stdout, "  -h, --help                 Show this message and exit.\n");
    fprintf(stdout, "\n");
    exit(rec_value);
}



int SttoNumber(char* optarg, char flag, char* filename) {
    long val;
    char *endptr;
    char *err_msg = NULL; 
    if (optarg) {

        val = strtol(optarg, &endptr, 10);
        
        if (endptr == optarg) {
            sprintf(err_msg, "No numeric value provided for %c", flag);
        } 

        else if (*endptr != '\0') {
            sprintf(err_msg, "Invalid characters foudn for %c", flag);
        } 

        else if (val > 2147483647 || val < -2147483648) { // Max/Min de un int de 32 bits
                sprintf(err_msg, "Value out of range for %c", flag);
        }
        
        if (err_msg) {
            fprintf(stderr, "Argument error: %s\n", err_msg);
            print_help_and_exit(filename, 1);
        }

        // Si pasa las comprobaciones, asigna el valor.
        return (int) val; 
            
    } 
}


int main(int argc, char *argv[]) {
    Code128Dict dict = init_code128();
    BitmapInitData* bitmap = init_bitmap();
    if (bitmap == NULL) return 1;
    
    Parameters param = {
        .array_len = 0,
        .height = DEFAULT_HEIGHT,
        .input = "",
        .input_file = "",
        .output_dir = "",
        .height_txt = DEFAULT_HEIGHT_TXT,
        .padd_x = DEFAULT_PADDING_X,
        .padd_y = DEFAULT_PADDING_Y,
        .padd_txt_y = DEFAULT_PADDING_TEXT_Y,
        .res_fact = DEFAULT_RES_MULT,
        .comp_fact = DEFAULT_COMP_FACT
    };

    static struct option long_options[] = {
        // { "nombre_largo", tiene_argumento, puntero_flag, valor_corto }

        {"input",           optional_argument, 0, 's'},
        {"input-file",      required_argument, 0, 'c'},
        {"output-dir",      required_argument, 0, 'o'},
        
        {"arr-len",        required_argument, 0, 'A'},
        {"height",       required_argument, 0, 'H'},
        {"height-txt",          required_argument, 0, 'T'},
        {"padd-x",          required_argument, 0, 'X'},
        {"padd-y",          required_argument, 0, 'Y'},
        {"padd-txt-y",          required_argument, 0, 'y'},
        {"res-fact",          required_argument, 0, 'R'},
        {"comp-fact",          required_argument, 0, 'C'},
        
        {"help",            no_argument,       0, 'h'},
        {0, 0, 0, 0} // Marcador de fin de la lista
    };

    int opt_index = 0;
    int c;

    

    while ((c = getopt_long(argc, argv, "c:o:s:A:H:T:X:Y:y:R:C:h", long_options, &opt_index)) != -1) {
        switch (c) {
        case 'c':
            strncpy(param.input_file, optarg, 254);
            break;
        case 'o':
            strncpy(param.output_dir, optarg, 254);
            break;
        case 's':
            strncpy(param.input, optarg, 254);
            break;
        case 'A':
            param.array_len = SttoNumber(optarg, 'A', argv[0]);
            break;
        case 'H':
            param.height = SttoNumber(optarg, 'H', argv[0]);
            break;
        case 'T':
            param.height_txt = SttoNumber(optarg, 'T', argv[0]);
            break;
        case 'X': 
            param.padd_x = SttoNumber(optarg, 'X', argv[0]);
            break;
        case 'Y': 
            param.padd_y = SttoNumber(optarg, 'Y', argv[0]);
            break;
        case 'y': 
            param.padd_txt_y = SttoNumber(optarg, 'y', argv[0]);
            break;
        case 'R': 
            param.res_fact = SttoNumber(optarg, 'R', argv[0]);
            break;
        case 'C': 
            param.comp_fact = SttoNumber(optarg, 'C', argv[0]);
            break;
        case 'h':
            print_help_and_exit(argv[0], 1);
            break;
        case '?': // getopt_long imprime el error por defecto para '?'
            return 1;
        default:
            return 1;
        }
    }

    if (strcmp(param.input_file, "") == 0 && strcmp(param.input, "") == 0) {
        fprintf(stderr, "Error: Please specify the input option (--input-file -c or -s --input).\n");
        print_help_and_exit(argv[0], 1);
        return 1;
    }
    
    if (param.array_len == 0 && strcmp(param.input, "") == 0) {
        fprintf(stderr, "Error: Please provide the number of elements to generate.\n");
        print_help_and_exit(argv[0], 1);
        return 1;
    }
    printf("input: %s , console: %s", param.input_file, param.input);
    if (strcmp(param.input_file, "") != 0 && strcmp(param.input, "") != 0) {
        fprintf(stderr, "Error: Either console input or file input, not both.\n");
        print_help_and_exit(argv[0], 1);
        return 1;
    }
    char strr[255];

    if (strcmp(param.input, "") != 0) {
        if (strcmp(param.output_dir, "") != 0) {
            sprintf(strr, "%s/%s.png", param.output_dir, param.input);
        } else {
            sprintf(strr, "./%s.png", param.input);
        }
        save_code128(param.input,dict ,bitmap, strr, param);
    } else {

        char** strings = (char**) csv_read((void*) process_txtline, NULL, param.input_file, param.array_len, 0);
        for (int i = 0 ; i < param.array_len ; i++) {
            if (strcmp(param.output_dir, "") != 0) {
                sprintf(strr, "%s/%s.png", param.output_dir, strings[i]);
            } else {
                sprintf(strr, "./%s.png", strings[i]);
            }
            printf("%s\n", strr);
            printf("%s\n", strings[i]);
            save_code128(strings[i],dict ,bitmap, strr, param);
            free(strings[i]);
        } 
        free(strings);
    }

    free_inits(dict, bitmap);
}
