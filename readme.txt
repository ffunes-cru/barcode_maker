COMPILING
    gcc main.c ./lib/img/libattopng.c -I /usr/include/freetype2 -lfreetype

USING
    ./a.out -c input_rep.txt -o output_files_r/ -A 590 --height 13 --res-fact 8 -C 1 --height-txt 16 --padd-txt-y 1 --padd-y 1

PRINTING
    lp -d Brother_QL-1110NWB -o fit-to-page <FILE>

    lp -d Brother_QL-1110NWB -o fit-to-page -o orientation-requested=3 -n 12 A1010.png

