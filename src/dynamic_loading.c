#ifdef DARWIN

#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include "dynamic_loading.h"
#include "segments.h"
#include "utils.h"

/* Currently, mach-o will allow up to the max of 2^15 alignment */
/* in an object file.                                           */
#ifndef L2_MAX_OFILE_ALIGNMENT
# define L2_MAX_OFILE_ALIGNMENT 15
#endif

/* Writable sections generally available on Darwin.     */
static const struct dyld_sections_s {
    const char *seg;
    const char *sect;
} GC_dyld_sections[] = {
    { SEG_DATA, SECT_DATA },
    /* Used by FSF GCC, but not by OS X system tools, so far.   */
    { SEG_DATA, "__static_data" },
    { SEG_DATA, SECT_BSS },
    { SEG_DATA, SECT_COMMON },
    /* FSF GCC - zero-sized object sections for targets         */
    /*supporting section anchors.                               */
    { SEG_DATA, "__zobj_data" },
    { SEG_DATA, "__zobj_bss" }
};

/* Additional writable sections:                                */
/* GCC on Darwin constructs aligned sections "on demand", where */
/* the alignment size is embedded in the section name.          */
/* Furthermore, there are distinctions between sections         */
/* containing private vs. public symbols.  It also constructs   */
/* sections specifically for zero-sized objects, when the       */
/* target supports section anchors.                             */
static const char * const GC_dyld_add_sect_fmts[] = {
  "__bss%u",
  "__pu_bss%u",
  "__zo_bss%u",
  "__zo_pu_bss%u"
};

void GC_dyld_add_image(const struct mach_header64* hdr, intptr_t vmaddr_slide) {
  unsigned long start, end;
  unsigned i, j;
  const struct section_64 *sec;
  const char *name;

  for (i = 0; i < sizeof(GC_dyld_sections)/sizeof(GC_dyld_sections[0]); i++) {
    sec = getsectbynamefromheader_64(hdr, GC_dyld_sections[i].seg,
                           GC_dyld_sections[i].sect);
    if (sec == NULL || sec->size < sizeof(void*))
      continue;
    start = vmaddr_slide + sec->addr;
    end = start + sec->size;
    Segments_add_segment((void*)start, (void*)end, GC_dyld_sections[i].sect);
  }

  /* Sections constructed on demand.    */
  for (j = 0; j < sizeof(GC_dyld_add_sect_fmts) / sizeof(char *); j++) {
    const char *fmt = GC_dyld_add_sect_fmts[j];

    /* Add our manufactured aligned BSS sections.       */
    for (i = 0; i <= L2_MAX_OFILE_ALIGNMENT; i++) {
      char secnam[16];

      (void)snprintf(secnam, sizeof(secnam), fmt, (unsigned)i);
      secnam[sizeof(secnam) - 1] = '\0';
      sec = getsectbynamefromheader_64(hdr, SEG_DATA, secnam);
      if (sec == NULL || sec->size == 0)
        continue;
      start = vmaddr_slide + sec->addr;
      end = start + sec->size;
      //TODO: probably need to change fmt by secnam (but secnam is local)
      Segments_add_segment(start, end, fmt);
    }
  }
}

void GC_init_dyld(void)
{
  DEBUG("GC: Registering segments\n");
  /* Apple's Documentation:
     When you call _dyld_register_func_for_add_image, the dynamic linker
     runtime calls the specified callback (func) once for each of the images
     that is currently loaded into the program. When a new image is added to
     the program, your callback is called again with the mach_header for the
     new image, and the virtual memory slide amount of the new image.

     This WILL properly register already linked libraries and libraries
     linked in the future.
  */
  _dyld_register_func_for_add_image(
        (void (*)(const struct mach_header*, intptr_t))GC_dyld_add_image);
  // _dyld_register_func_for_remove_image(
  //       (void (*)(const struct mach_header*, intptr_t))GC_dyld_image_remove);
                        /* Structure mach_header64 has the same fields  */
                        /* as mach_header except for the reserved one   */
                        /* at the end, so these casts are OK.           */
}

#endif // DARWIN
