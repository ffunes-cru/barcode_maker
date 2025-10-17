#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @brief Genera una cadena de texto compuesta por N líneas de largo M, separadas por '\n'.
 * @param n_lines Número de líneas (N).
 * @param m_length Longitud de cada línea (M).
 * @return char* Puntero a la cadena de texto generada. Debe ser liberado con free() por el llamador.
 */
char* generate_random_text(int n_lines, int m_length) {
    if (n_lines <= 0 || m_length <= 0) {
        fprintf(stderr, "Error: N y M deben ser mayores que cero.\n");
        return NULL;
    }

    // Calcular el tamaño total requerido:
    // (M caracteres por línea + 1 caracter de '\n') * N líneas + 1 para el '\0' final
    size_t total_length = (size_t)(m_length + 1) * n_lines + 1;

    // Asignar memoria para la cadena completa
    char *result_text = (char *)malloc(total_length);
    if (result_text == NULL) {
        perror("Fallo al asignar memoria para el texto");
        return NULL;
    }

    // Puntero para rastrear la posición actual en la cadena de resultado
    char *current_pos = result_text;

    // Inicializar el generador de números aleatorios (solo una vez)
    // Usamos time(NULL) para asegurar una semilla diferente cada vez
    srand((unsigned int)time(NULL)); 

    // Bucle para generar cada una de las N líneas
    for (int i = 0; i < n_lines; i++) {
        // Generar M caracteres aleatorios
        for (int j = 0; j < m_length; j++) {
            
            // Generar un número aleatorio (0, 1, 2) para elegir el tipo de carácter:
            // 0: Espacio
            // 1: Número (0-9)
            // 2: Letra Mayúscula (A-Z)
            int char_type = rand() % 2;
            
            char random_char;

            if (char_type == 1) {
                // Tipo 1: Número 0-9 (ASCII 48 a 57)
                random_char = (char)((rand() % (57 - 48 + 1)) + 48);
            } else { // char_type == 2
                // Tipo 2: Letra Mayúscula A-Z (ASCII 65 a 90)
                random_char = (char)((rand() % (90 - 65 + 1)) + 65);
            }

            *current_pos = random_char;
            current_pos++;
        }

        // Agregar el separador '\n', excepto después de la última línea
        if (i < n_lines - 1) {
            *current_pos = '\n';
            current_pos++;
        }
    }

    // Terminar la cadena con el terminador nulo
    *current_pos = '\0';

    return result_text;
}

/**
 * @brief Función principal que genera texto y lo guarda en un archivo.
 */
int main() {
    // --- Configuración ---
    const int N_LINES = 100;   // N: 10 líneas
    const int M_LENGTH = 20;  // M: 50 caracteres por línea
    const char *FILENAME = "random_text_output.txt";

    printf("Generando texto con %d líneas de %d caracteres (solo MAYÚSCULAS, NÚMEROS y ESPACIOS)...\n", N_LINES, M_LENGTH);

    // 1. Generar la cadena
    char *generated_text = generate_random_text(N_LINES, M_LENGTH);

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
