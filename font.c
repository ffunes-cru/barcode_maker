#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Definición de las constantes de tipo de carácter para mejor lectura
#define CHAR_TYPE_NUMBER 0
#define CHAR_TYPE_UPPER  1

/**
 * @brief Genera una cadena de texto compuesta por N líneas con largo aleatorio entre M_MIN y M_MAX, separadas por '\n'.
 * @param n_lines Número de líneas (N).
 * @param m_min_length Longitud mínima de cada línea.
 * @param m_max_length Longitud máxima de cada línea.
 * @return char* Puntero a la cadena de texto generada. Debe ser liberado con free() por el llamador.
 */
char* generate_random_text_range(int n_lines, int m_min_length, int m_max_length) {
    if (n_lines <= 0 || m_min_length <= 0 || m_min_length > m_max_length) {
        fprintf(stderr, "Error de parámetros: Verifique N > 0 y M_MIN <= M_MAX.\n");
        return NULL;
    }

    // --- 1. Determinar y Almacenar las Longitudes de Cada Línea ---
    // Usamos un array para almacenar las longitudes exactas para poder calcular el total de memoria.
    int *line_lengths = (int *)malloc(sizeof(int) * n_lines);
    if (line_lengths == NULL) {
        perror("Fallo al asignar memoria para las longitudes de línea");
        return NULL;
    }
    
    // Inicializar el generador de números aleatorios (solo una vez)
    srand((unsigned int)time(NULL)); 
    
    size_t total_data_length = 0;
    int range = m_max_length - m_min_length + 1;

    for (int i = 0; i < n_lines; i++) {
        // Generar longitud aleatoria entre [m_min_length, m_max_length]
        int current_length = (rand() % range) + m_min_length;
        line_lengths[i] = current_length;
        
        // Sumar la longitud de la línea + 1 para el '\n'
        total_data_length += (size_t)current_length;
    }

    // El tamaño total es: suma de longitudes + (N_LINES - 1) para '\n' + 1 para '\0' final
    size_t total_length = total_data_length + (n_lines - 1) + 1;
    
    // --- 2. Asignar Memoria y Generar la Cadena ---
    char *result_text = (char *)malloc(total_length);
    if (result_text == NULL) {
        perror("Fallo al asignar memoria para el texto final");
        free(line_lengths);
        return NULL;
    }

    char *current_pos = result_text;

    for (int i = 0; i < n_lines; i++) {
        int m_length = line_lengths[i];
        
        // Generar m_length caracteres aleatorios para la línea actual
        for (int j = 0; j < m_length; j++) {
            
            // Generar un número aleatorio (0, 1) para elegir el tipo de carácter:
            int char_type = rand() % 2; // Solo 0 (Número) o 1 (Mayúscula)
            
            char random_char;

            if (char_type == CHAR_TYPE_NUMBER) {
                // Número 0-9 (ASCII 48 a 57)
                random_char = (char)((rand() % (57 - 48 + 1)) + 48);
            } else { 
                // Letra Mayúscula A-Z (ASCII 65 a 90)
                random_char = (char)((rand() % (90 - 65 + 1)) + 65);
            }

            *current_pos = random_char;
            current_pos++;
        }

        // Agregar el separador '\n', si no es la última línea
        if (i < n_lines - 1) {
            *current_pos = '\n';
            current_pos++;
        }
    }

    // Terminar la cadena con el terminador nulo
    *current_pos = '\0';

    // Liberar la memoria auxiliar
    free(line_lengths);

    return result_text;
}

/**
 * @brief Función principal que genera texto con longitud de línea variable y lo guarda en un archivo.
 */
int main() {
    // --- Configuración ---
    const int N_LINES = 3000;     // N: Número de líneas
    const int M_MIN = 2;        // Mín: Longitud mínima por línea
    const int M_MAX = 30;        // Máx: Longitud máxima por línea
    const char *FILENAME = "random_text_variable.txt";

    printf("Generando texto con %d líneas de largo variable (entre %d y %d)...\n", 
           N_LINES, M_MIN, M_MAX);

    // 1. Generar la cadena
    char *generated_text = generate_random_text_range(N_LINES, M_MIN, M_MAX);

    if (generated_text == NULL) {
        return 1;
    }

    // 2. Abrir el archivo para escritura (w)
    FILE *file = fopen(FILENAME, "w");
    if (file == NULL) {
        perror("Error al abrir el archivo para escritura");
        free(generated_text);
        return 1;
    }

    // 3. Escribir la cadena en el archivo
    if (fputs(generated_text, file) == EOF) {
        fprintf(stderr, "Error: Fallo al escribir en el archivo.\n");
        fclose(file);
        free(generated_text);
        return 1;
    }

    // 4. Cerrar el archivo y liberar memoria
    fclose(file);
    free(generated_text);

    printf("¡Éxito! Texto guardado en %s\n", FILENAME);

    return 0;
}