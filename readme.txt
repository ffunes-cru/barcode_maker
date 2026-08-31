========================================================================
CODE 128 STUDIO (Windows 11 Fluent GUI + CLI + Auto Updater)
========================================================================

DESCRIPCIÓN:
  Code 128 Studio es una suite integral y ultra liviana (< 2 MB) para la
  generación e impresión de códigos de barras Code 128 (Subconjunto B/C)
  con renderizado 100% acelerado por GPU (OpenGL DrawList), soporte para
  tiras continuas de impresoras térmicas Brother (QL series) y auto-actualizador.

COMPILACIÓN (CMake):
    mkdir -p build && cd build
    cmake ..
    make -j$(nproc)

    Binarios generados:
      - code128_studio  : Binario ÚNICO que funciona tanto en modo GUI como CLI
      - code128_updater : Ejecutable independiente de auto-actualización hot

EJECUCIÓN:
    1. Modo Gráfico (Windows 11 Fluent GUI):
       ./build/code128_studio
       ./build/code128_studio --gui

    2. Modo Consola (CLI Directo):
       ./build/code128_studio -s "A0101" -o output/ --height 13 --res-fact 8 -C 1
       ./build/code128_studio -c input_rep.txt -o output/ -A 500

    3. Actualizador:
       ./build/code128_updater

CI/CD GITHUB ACTIONS:
    El archivo .github/workflows/build_and_release.yml compila automáticamente
    los paquetes para Linux (.tar.gz) y Windows (.zip / installer) en cada commit
    y crea releases públicos cuando se crea un tag (ej: v1.1.0).
