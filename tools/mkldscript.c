#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spec.h"
#include "util.h"

struct Segment *g_segments;
int g_segmentsCount;

static void write_ld_script(FILE *fout)
{
    int i;
    int j;

    fputs("SECTIONS {\n"
          "    _RomSize = 0;\n"
          "    _RomStart = _RomSize;\n\n",
          fout);

    for (i = 0; i < g_segmentsCount; i++)
    {
        const struct Segment *seg = &g_segments[i];

        // align start of ROM segment
        if (seg->fields & (1 << STMT_romalign))
            fprintf(fout, "    _RomSize = (_RomSize + %i) & ~ %i;\n", seg->romalign - 1, seg->romalign - 1);

        // initialized data (.text, .data, .rodata, .sdata)

        // Increment the start of the section
        //if (seg->fields & (1 << STMT_increment))
            //fprintf(fout, "    . += 0x%08X;\n", seg->increment);

        fprintf(fout, "    _%sSegmentRomStartTemp = _RomSize;\n"
                  "    _%sSegmentRomStart = _%sSegmentRomStartTemp;\n"
                  "    ..%s ", seg->name, seg->name, seg->name, seg->name);

        if (seg->fields & (1 << STMT_after))
            fprintf(fout, "_%sSegmentEnd ", seg->after);
        else if (seg->fields & (1 << STMT_number))
            fprintf(fout, "0x%02X000000 ", seg->number);
        else if (seg->fields & (1 << STMT_address))
            fprintf(fout, "0x%08X ", seg->address);

        // (AT(_RomSize) isn't necessary, but adds useful "load address" lines to the map file)
        fprintf(fout, ": AT(_RomSize)\n    {\n"
                  "        _%sSegmentStart = .;\n"
                  "        . = ALIGN(0x10);\n"
                  "        _%sSegmentTextStart = .;\n",
                  seg->name, seg->name);

        if (seg->fields & (1 << STMT_align))
            fprintf(fout, "        . = ALIGN(0x%X);\n", seg->align);

        for (j = 0; j < seg->includesCount; j++)
        {
            fprintf(fout, "            %s (.text)\n", seg->includes[j].fpath);
            if (seg->includes[j].linkerPadding != 0)
                fprintf(fout, "            . += 0x%X;\n", seg->includes[j].linkerPadding);
        }

        fprintf(fout, "        _%sSegmentTextEnd = .;\n", seg->name);

        fprintf(fout, "    _%sSegmentTextSize = ABSOLUTE( _%sSegmentTextEnd - _%sSegmentTextStart );\n", seg->name, seg->name, seg->name);

        fprintf(fout, "        _%sSegmentDataStart = .;\n", seg->name);

        for (j = 0; j < seg->includesCount; j++)
        {
            if (!seg->includes[j].dataWithRodata)
                fprintf(fout, "            %s (.data)\n", seg->includes[j].fpath);
        }

        /*
         for (j = 0; j < seg->includesCount; j++)
            fprintf(fout, "            %s (.rodata)\n", seg->includes[j].fpath);

          for (j = 0; j < seg->includesCount; j++)
            fprintf(fout, "            %s (.sdata)\n", seg->includes[j].fpath);
        */

        //fprintf(fout, "        . = ALIGN(0x10);\n");
        fprintf(fout, "        _%sSegmentDataEnd = .;\n", seg->name);

        fprintf(fout, "    _%sSegmentDataSize = ABSOLUTE( _%sSegmentDataEnd - _%sSegmentDataStart );\n", seg->name, seg->name, seg->name);

        fprintf(fout, "        _%sSegmentRoDataStart = .;\n", seg->name);

        for (j = 0; j < seg->includesCount; j++)
        {
            if (seg->includes[j].dataWithRodata)
                fprintf(fout, "            %s (.data)\n", seg->includes[j].fpath);
            fprintf(fout, "            %s (.rodata)\n", seg->includes[j].fpath);
            // Compilers other than IDO, such as GCC, produce different sections such as
            // the ones named directly below. These sections do not contain values that
            // need relocating, but we need to ensure that the base .rodata section
            // always comes first. The reason this is important is due to relocs assuming
            // the base of .rodata being the offset for the relocs and thus needs to remain
            // the beginning of the entire rodata area in order to remain consistent.
            // Inconsistencies will lead to various .rodata reloc crashes as a result of
            // either missing relocs or wrong relocs.
            fprintf(fout, "            %s (.rodata.str1.4)\n", seg->includes[j].fpath);
            fprintf(fout, "            %s (.rodata.cst4)\n", seg->includes[j].fpath);
            fprintf(fout, "            %s (.rodata.cst8)\n", seg->includes[j].fpath);
        }

         //fprintf(fout, "        . = ALIGN(0x10);\n");

        fprintf(fout, "        _%sSegmentRoDataEnd = .;\n", seg->name);

        fprintf(fout, "    _%sSegmentRoDataSize = ABSOLUTE( _%sSegmentRoDataEnd - _%sSegmentRoDataStart );\n", seg->name, seg->name, seg->name);

        fprintf(fout, "        _%sSegmentSDataStart = .;\n", seg->name);

        for (j = 0; j < seg->includesCount; j++)
            fprintf(fout, "            %s (.sdata)\n", seg->includes[j].fpath);

         fprintf(fout, "        . = ALIGN(0x10);\n");

        fprintf(fout, "        _%sSegmentSDataEnd = .;\n", seg->name);

        fprintf(fout, "        _%sSegmentOvlStart = .;\n", seg->name);

        for (j = 0; j < seg->includesCount; j++)
            fprintf(fout, "            %s (.ovl)\n", seg->includes[j].fpath);

        fprintf(fout, "        . = ALIGN(0x10);\n");

        fprintf(fout, "        _%sSegmentOvlEnd = .;\n", seg->name);

        if (seg->fields & (1 << STMT_increment))
            fprintf(fout, "    . += 0x%08X;\n", seg->increment);


        fputs("    }\n", fout);
        //fprintf(fout, "    _RomSize += ( _%sSegmentDataEnd - _%sSegmentTextStart );\n", seg->name, seg->name);
        fprintf(fout, "    _RomSize += ( _%sSegmentOvlEnd - _%sSegmentTextStart );\n", seg->name, seg->name);

        fprintf(fout, "    _%sSegmentRomEndTemp = _RomSize;\n"
                  "_%sSegmentRomEnd = _%sSegmentRomEndTemp;\n\n",
                  seg->name, seg->name, seg->name);

        // algn end of ROM segment
        if (seg->fields & (1 << STMT_romalign))
            fprintf(fout, "    _RomSize = (_RomSize + %i) & ~ %i;\n", seg->romalign - 1, seg->romalign - 1);

        // uninitialized data (.sbss, .scommon, .bss, COMMON)
        fprintf(fout, "    ..%s.bss ADDR(..%s) + SIZEOF(..%s) (NOLOAD) :\n"
                      /*"    ..%s.bss :\n"*/
                      "    {\n"
                      "        . = ALIGN(0x10);\n"
                      "        _%sSegmentBssStart = .;\n",
                      seg->name, seg->name, seg->name, seg->name);
        if (seg->fields & (1 << STMT_align))
            fprintf(fout, "        . = ALIGN(0x%X);\n", seg->align);
        for (j = 0; j < seg->includesCount; j++)
            fprintf(fout, "            %s (.sbss)\n", seg->includes[j].fpath);
        for (j = 0; j < seg->includesCount; j++)
            fprintf(fout, "            %s (.scommon)\n", seg->includes[j].fpath);
        for (j = 0; j < seg->includesCount; j++)
            fprintf(fout, "            %s (.bss)\n", seg->includes[j].fpath);
        for (j = 0; j < seg->includesCount; j++)
            fprintf(fout, "            %s (COMMON)\n", seg->includes[j].fpath);
        fprintf(fout, "        . = ALIGN(0x10);\n"
                      "        _%sSegmentBssEnd = .;\n"
                      "        _%sSegmentEnd = .;\n"
                      "    }\n"
                      "    _%sSegmentBssSize = ABSOLUTE( _%sSegmentBssEnd - _%sSegmentBssStart );\n\n",
                      seg->name, seg->name, seg->name, seg->name, seg->name);

        // Increment the end of the segment
        //if (seg->fields & (1 << STMT_increment))
            //fprintf(fout, "    . += 0x%08X;\n", seg->increment);

        //fprintf(fout, "    ..%s.ovl ADDR(..%s) + SIZEOF(..%s) :\n"
        //    /*"    ..%s.bss :\n"*/
        //    "    {\n",
        //    seg->name, seg->name, seg->name);
        //fprintf(fout, "        _%sSegmentOvlStart = .;\n", seg->name);

        //for (j = 0; j < seg->includesCount; j++)
        //    fprintf(fout, "            %s (.ovl)\n", seg->includes[j].fpath);

        ////fprintf(fout, "        . = ALIGN(0x10);\n");

        //fprintf(fout, "        _%sSegmentOvlEnd = .;\n", seg->name);

        //fprintf(fout, "\n    }\n");
    }


    fputs("    _RomEnd = _RomSize;\n}\n", fout);
}

static void usage(const char *execname)
{
    fprintf(stderr, "Nintendo 64 linker script generation tool v0.02\n"
                    "usage: %s SPEC_FILE LD_SCRIPT\n"
                    "SPEC_FILE  file describing the organization of object files into segments\n"
                    "LD_SCRIPT  filename of output linker script\n",
                    execname);
}

int main(int argc, char **argv)
{
    FILE *ldout;
    void *spec;
    size_t size;

    if (argc != 3)
    {
        usage(argv[0]);
        return 1;
    }

    spec = util_read_whole_file(argv[1], &size);
    parse_rom_spec(spec, &g_segments, &g_segmentsCount);

    ldout = fopen(argv[2], "w");
    if (ldout == NULL)
        util_fatal_error("failed to open file '%s' for writing", argv[2]);
    write_ld_script(ldout);
    fclose(ldout);

    free_rom_spec(g_segments, g_segmentsCount);
    free(spec);

    return 0;
}
