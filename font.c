#include <stdio.h>
#include <stdlib.h>
#include <ft2build.h>
#include "lib/img/libattopng.h"
#include FT_FREETYPE_H
#include FT_GLYPH_H

// --- 1. ESTRUCTURAS DE DATOS ---

// Estructura para almacenar un solo píxel en escala de grises
typedef struct {
    int x;       // Coordenada X del píxel
    int y;       // Coordenada Y del píxel
    unsigned char v; // Valor de gris (0 a 255)
} BitmapPoint;

// Estructura para almacenar la lista completa de píxeles
typedef struct {
    BitmapPoint *points;
    size_t count; // Número total de píxeles
    int width;    // Ancho total del bitmap generado
    int height;   // Alto total del bitmap generado
} BitmapList;

// --- 2. GESTIÓN DE ERRORES ---

#define DIE_ON_ERROR(err, msg) \
    if (err) { \
        fprintf(stderr, "%s (Error FreeType: 0x%x)\n", msg, err); \
        return NULL; \
    }

// --- 3. FUNCIÓN PRINCIPAL ---

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

    // Inicialización del motor FreeType
    error = FT_Init_FreeType(&library);
    DIE_ON_ERROR(error, "Error al inicializar FreeType");

    // Carga de la fuente desde el archivo
    error = FT_New_Face(library, font_path, 0, &face);
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
                        current_point->v = gray_value;
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

// --- FUNCIÓN DE PRUEBA (main) ---

int main(int argc, char *argv[]) {
    // ESTA PRUEBA FALLARÁ si no proporcionas una fuente y un texto reales
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <ruta_a_fuente.otf> <texto_a_renderizar>\n", argv[0]);
        return 1;
    }

    const char *font_file = argv[1]; 
    const char *text = argv[2]; 
    int size = 32; // 32 píxeles de alto

    printf("Intentando renderizar texto: '%s' con fuente: %s (Tamaño %dpx)\n", 
           text, font_file, size);

    BitmapList *list = render_text_to_bitmap_list(font_file, text, size);

    libattopng_t* png = libattopng_new(list->width, list->height, PNG_GRAYSCALE);

    if (list) {
        printf("\n--- RESULTADO DEL RASTERIZADO ---\n");
        printf("Dimensiones calculadas: %d x %d\n", list->width, list->height);
        printf("Píxeles únicos encontrados: %zu\n", list->count);
        
        // Imprimir los primeros 10 píxeles para verificar
        printf("Primeros 10 puntos (X, Y, Gris):\n");
        for (size_t i = 0; i < list->count && i < list->count; i++) {
            libattopng_set_pixel(png, list->points[i].x, list->points[i].y, list->points[i].v);
            //printf("(%d, %d, %d) ", list->points[i].x, list->points[i].y, list->points[i].v);
        }
        printf("\n\n");
        
        libattopng_save(png, "test_font.png");
        libattopng_destroy(png);
        // Liberar la memoria asignada
        free(list->points);
        free(list);
    } else {
        fprintf(stderr, "Fallo al renderizar el texto.\n");
        return 1;
    }

    return 0;
}