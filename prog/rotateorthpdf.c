/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -
 -  Redistribution and use in source and binary forms, with or without
 -  modification, are permitted provided that the following conditions
 -  are met:
 -  1. Redistributions of source code must retain the above copyright
 -     notice, this list of conditions and the following disclaimer.
 -  2. Redistributions in binary form must reproduce the above
 -     copyright notice, this list of conditions and the following
 -     disclaimer in the documentation and/or other materials
 -     provided with the distribution.
 -
 -  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 -  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 -  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ANY
 -  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 -  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 -  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 -  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 -  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 -  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 -  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *====================================================================*/

/*
 * rotateorthpdf.c
 *
 *    This program rotates individual page images in a pdf of images.
 *    It takes a rotation string that has three modes:
 *    (1) a list of integers in [0,1,2,3] for 90 degree cw rotations; only
 *        need to list up to last non-zero rotation
 *    (2) a 4, followed by one integer, to rotate all images by the same amount
 *    (3) a 5, followed by parenthesized comma-separated pairs of numbers:
 *    (page, rotation).
 *
 *    Syntax:
 *       rotateorthpdf filein imres rotstring scalefactor quality
 *                     title fileout
 *
 *    The %imres is the desired resolution of the rasterization from the
 *    pdf page to a page image.  Two choices are allowed: 150 and 300 ppi.
 *    Use 0 for default (150 ppi).  The actual resolution used by the
 *    renderer depends on the page image size and is computed internally.
 *    We limit the maximum resolution to 300 ppi because these images are
 *    RGB uncompressed and are large: 6.3 MB for 150 ppi and 25 MB for 300 ppi.
 *
 *    The %rotstring flag determines which, if any, images are rotated cw by
 *    multiples of 90 degrees.  There are 3 modes, and here are examples:
 *    Mode 1: "00201".  The third image is rotated by 180 degrees and
 *            the fifth by 90 degrees.  All others are not rotated.
 *    Mode 2: "41".  All images are rotated 90 degrees cw.
 *    Mode 3: "5(12,3)(19,1)".  Image 12 is rotated cw by 270 degrees and image
 *            19 is rotated by 90 degrees.  All others are not rotated.
 *    Note that images are numbered from 0, not from 1.
 *
 *    The %scalefactor is the scaling to be applied to each image.  You
 *    can use any positive value not exceeding 2.0.
 *
 *    The %quality is the jpeg output quality factor for images stored
 *    with DCT encoding in the pdf.  Use 0 for the default value (50),
 *    which is satisfactory for many purposes.  Use 75 for standard
 *    jpeq quality; 85-95 is very high quality.  Allowed values are
 *    between 25 and 95.
 *
 *    The %title is the title given to the pdf.  Use %title == "none"
 *    to omit the title.
 *
 *    The pdf output is written to %fileout.  It is advisable (but not
 *    required) to have a '.pdf' extension.
 *
 *    As the first step in processing, images are saved in the directory
 *    /tmp/lept/renderpdf/, as RGB in ppm format, and at the resolution
 *    specified by %imres, either 150 or 300 ppi.  Page-sized images will
 *    rendered at 150 ppi will be about 6MB; at 300 ppi they will be 25 MB.
 *
 *    We use pdftoppm to render the images at (typically) 150 pixels/inch.
 *    The renderer uses the mediaboxes to decide how big to make the
 *    images.  If those boxes have values that are too large, the
 *    intermediate ppm images can be very large.  To prevent that,
 *    we compute the resolution to input to pdftoppm that results in
 *    page images at the requested resolution.
 *
 *    N.B.  This requires running pdftoppm from the Poppler package
 *          of pdf utilities  For non-unix systems, this requires
 *          installation of the cygwin Poppler package:
 *       https://cygwin.com/cgi-bin2/package-cat.cgi?file=x86/poppler/
 *              poppler-0.26.5-1
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

l_int32 main(int    argc,
             char **argv)
{
char       buf[256];
char      *rotstring, *filein, *title, *fileout;
l_int32    imres, render_res, quality;
l_float32  scalefactor;
SARRAY    *safiles;

    if (argc != 8)
        return ERROR_INT(
            "Syntax: rotateorthpdf filein imres rotstring"
                    " scalefactor quality title fileout", __func__, 1);
    filein = argv[1];
    imres = atoi(argv[2]);
    rotstring = argv[3];
    scalefactor = atof(argv[4]);
    quality = atoi(argv[5]);  /* jpeg quality */
    title = argv[6];
    fileout = argv[7];
    if (imres <= 0) imres = 150;  /* default value */
    if (imres != 150 && imres != 300) {
        L_WARNING("imres = %d must be 150 or 300; setting to 150\n",
                  __func__, imres);
        imres = 150;
    }
    if (scalefactor <= 0.0) scalefactor = 1.0;
    if (scalefactor > 2.0) {
        L_WARNING("scalefactor %f too big; setting to 2.0\n", __func__, 
                  scalefactor);
        scalefactor = 2.0;
    }
    if (quality <= 0) quality = 50;  /* default value */
    if (quality < 25) {
        L_WARNING("quality = %d is too low; setting to 25\n",
                  __func__, quality);
        quality = 25;
    }
    if (quality > 95) {
        L_WARNING("quality = %d is too high; setting to 95\n",
                  __func__, quality);
        quality = 95;
    }
    setLeptDebugOK(1);

        /* Render all images from the pdf file */
    if (l_pdfRenderFile(filein, imres, &safiles))
        return ERROR_INT_1("rendering failed from filein", filein,
                           __func__, 1);

        /* Optionally rotate, then scale and collect all images in memory.
         * If n > 100, use pixacomp instead of pixa to store everything
         * before generating the pdf. */
    lept_stderr("rotating ...\n");
    rotateorthFilesToPdf(safiles, rotstring, scalefactor, quality,
                         title, fileout);
    sarrayDestroy(&safiles);
    return 0;
}

