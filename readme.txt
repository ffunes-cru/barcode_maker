========================================================================
CODE 128 MAKER & STUDIO (Windows 11 GUI + CLI)
========================================================================

COMPILING WITH CMAKE:
    mkdir -p build && cd build
    cmake ..
    make -j$(nproc)

    Binaries generated in build/:
      - code128_gui   : Windows 11 Fluent GUI Studio (Hot Live Preview, Brother Tape mode, Direct Print)
      - code128_maker : Original CLI tool

RUNNING GUI:
    ./build/code128_gui

USING CLI:
    ./build/code128_maker -s "A0101" -o output_files_r/ -A 1 --height 13 --res-fact 8 -C 1 --height-txt 16 --padd-txt-y 1 --padd-y 1
    ./build/code128_maker -c input_rep.txt -o output_files_r/ -A 590 --height 13 --res-fact 8 -C 1 --height-txt 16 --padd-txt-y 1 --padd-y 1

PRINTING (Brother QL Series):
    - Direct via GUI: Click "🖨️ Imprimir Directo" in the top bar.
    - Via CLI:
        lp -d Brother_QL-1110NWB -o fit-to-page <FILE>
        lp -d Brother_QL-1110NWB -o fit-to-page -o orientation-requested=3 -n 12 A1010.png
