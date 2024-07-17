/* -*- C++ -*-
 * File: libraw_c_api.cpp
 * Copyright 2008-2021 LibRaw LLC (info@libraw.org)
 * Created: Sat Mar  8 , 2008
 *
 * LibRaw C interface


LibRaw is free software; you can redistribute it and/or modify
it under the terms of the one of two licenses as you choose:

1. GNU LESSER GENERAL PUBLIC LICENSE version 2.1
   (See file LICENSE.LGPL provided in LibRaw distribution archive for details).

2. COMMON DEVELOPMENT AND DISTRIBUTION LICENSE (CDDL) Version 1.0
   (See file LICENSE.CDDL provided in LibRaw distribution archive for details).

 */

#include <math.h>
#include <errno.h>
#include "libraw/libraw.h"
#include <stdio.h>
#include <string.h>

#ifndef LIBRAW_WIN32_CALLS
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#endif

#ifdef LIBRAW_WIN32_CALLS
#define snprintf _snprintf
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
// Function to get the current URL (only works in a browser context)
EM_JS(char *, get_current_url, (), {
  var url = window.location.href;
  var lengthBytes = lengthBytesUTF8(url) + 1;
  var stringOnWasmHeap = _malloc(lengthBytes);
  stringToUTF8(url, stringOnWasmHeap, lengthBytes);
  return stringOnWasmHeap;
});

int my_progress_callback(void *unused_data, enum LibRaw_progress state,
                         int iter, int expected)
{
  if (iter == 0)
    printf("CB: state=%x, expected %d iterations\n", state, expected);
  return 0;
}

char *customCameras[] = {
    (char *)"43704960,4080,5356, 0, 0, 0, 0,0,148,0,0, Dalsa, FTF4052C Full,0",
    (char *)"42837504,4008,5344, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF4052C 3:4",
    (char *)"32128128,4008,4008, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF4052C 1:1",
    (char *)"24096096,4008,3006, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF4052C 4:3",
    (char *)"18068064,4008,2254, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF4052C 16:9",
    (char *)"67686894,5049,6703, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF5066C Full",
    (char *)"66573312,4992,6668, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF5066C 3:4",
    (char *)"49840128,4992,4992, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF5066C 1:1",
    (char *)"37400064,4992,3746, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF5066C 4:3",
    (char *)"28035072,4992,2808, 0, 0, 0, 0,0,148,0,0,Dalsa, FTF5066C 16:9",
    NULL};

int main(int ac, char *av[])
{

  const char *url = get_current_url();
  const char *allowed_domains[] = {"http://localhost",
                                   "https://sub.example.com",
                                   "https://anotherdomain.com", NULL};
  int allowed = 0;

  for (int i = 0; allowed_domains[i] != NULL; i++)
  {
    if (strstr(url, allowed_domains[i]) != NULL)
    {
      allowed = 1;
      break;
    }
  }

  if (!allowed)
  {
    printf("%s Access denied for URL: %s\n", LibRaw::version(), url);
    delete url;
    return 1; // Exit if domain is not allowed
  }
  delete url;

  int i, ret, verbose = 0, output_thumbs = 0, output_all_thumbs = 0;

  // don't use fixed size buffers in real apps!
  char outfn[1024], thumbfn[1024];

  LibRaw *RawProcessor = new LibRaw;
  RawProcessor->imgdata.rawparams.custom_camera_strings = customCameras;
  if (ac < 2)
  {
    printf("simple_dcraw - LibRaw %s sample. Emulates dcraw [-D] [-T] [-v] "
           "[-e] [-E]\n"
           " %d cameras supported\n"
           "Usage: %s [-D] [-T] [-v] [-e] raw-files....\n"
           "\t-4 - 16-bit mode\n"
           "\t-L - list supported cameras and exit\n"
           "\t-v - verbose output\n"
           "\t-T - output TIFF files instead of .pgm/ppm\n"
           "\t-e - extract thumbnails (same as dcraw -e in separate run)\n"
           "\t-E - extract all thumbnails\n",
           LibRaw::version(), LibRaw::cameraCount(), av[0]);
    delete RawProcessor;
    return 0;
  }

  putenv((char *)"TZ=UTC"); // dcraw compatibility, affects TIFF datestamp field

#define P1 RawProcessor->imgdata.idata
#define S RawProcessor->imgdata.sizes
#define C RawProcessor->imgdata.color
#define T RawProcessor->imgdata.thumbnail
#define P2 RawProcessor->imgdata.other
#define OUT RawProcessor->imgdata.params

  for (i = 1; i < ac; i++)
  {
    if (av[i][0] == '-')
    {
      if (av[i][1] == 'T' && av[i][2] == 0)
        OUT.output_tiff = 1;
      if (av[i][1] == 'v' && av[i][2] == 0)
        verbose++;
      if (av[i][1] == 'e' && av[i][2] == 0)
        output_thumbs++;
      if (av[i][1] == 'E' && av[i][2] == 0)
      {
        output_thumbs++;
        output_all_thumbs++;
      }
      if (av[i][1] == '4' && av[i][2] == 0)
        OUT.output_bps = 16;
      if (av[i][1] == 'C' && av[i][2] == 0)
        RawProcessor->set_progress_handler(my_progress_callback, NULL);
      if (av[i][1] == 'L' && av[i][2] == 0)
      {
        const char **clist = LibRaw::cameraList();
        const char **cc = clist;
        while (*cc)
        {
          printf("%s\n", *cc);
          cc++;
        }
        delete RawProcessor;
        exit(0);
      }
      continue;
    }

    if (verbose)
      printf("Processing file %s\n", av[i]);

    if ((ret = RawProcessor->open_file(av[i])) != LIBRAW_SUCCESS)
    {
      fprintf(stderr, "Cannot open_file %s: %s\n", av[i], libraw_strerror(ret));
      continue; // no recycle b/c open file will recycle itself
    }

    if (!output_thumbs) // No unpack for thumb extraction
      if ((ret = RawProcessor->unpack()) != LIBRAW_SUCCESS)
      {
        fprintf(stderr, "Cannot unpack %s: %s\n", av[i], libraw_strerror(ret));
        continue;
      }

    // thumbnail unpacking and output in the middle of main
    // image processing - for test purposes!
    if (output_all_thumbs)
    {
      if (verbose)
        printf("Extracting %d thumbnails\n",
               RawProcessor->imgdata.thumbs_list.thumbcount);
      for (int t = 0; t < RawProcessor->imgdata.thumbs_list.thumbcount; t++)
      {
        if ((ret = RawProcessor->unpack_thumb_ex(t)) != LIBRAW_SUCCESS)
          fprintf(stderr, "Cannot unpack_thumb #%d from %s: %s\n", t, av[i],
                  libraw_strerror(ret));
        if (LIBRAW_FATAL_ERROR(ret))
          break; // skip to next file
        snprintf(thumbfn, sizeof(thumbfn), "%s.thumb.%d.%s", av[i], t,
                 T.tformat == LIBRAW_THUMBNAIL_JPEG ? "jpg" : "ppm");
        if (verbose)
          printf("Writing thumbnail file %s\n", thumbfn);
        if (LIBRAW_SUCCESS != (ret = RawProcessor->dcraw_thumb_writer(thumbfn)))
        {
          fprintf(stderr, "Cannot write %s: %s\n", thumbfn,
                  libraw_strerror(ret));
          if (LIBRAW_FATAL_ERROR(ret))
            break;
        }
      }
      continue;
    }
    else if (output_thumbs)
    {
      if ((ret = RawProcessor->unpack_thumb()) != LIBRAW_SUCCESS)
      {
        fprintf(stderr, "Cannot unpack_thumb %s: %s\n", av[i],
                libraw_strerror(ret));
        if (LIBRAW_FATAL_ERROR(ret))
          continue; // skip to next file
      }
      else
      {
        snprintf(thumbfn, sizeof(thumbfn), "%s.%s", av[i],
                 T.tformat == LIBRAW_THUMBNAIL_JPEG
                     ? "thumb.jpg"
                     : (T.tcolors == 1 ? "thumb.pgm" : "thumb.ppm"));
        if (verbose)
          printf("Writing thumbnail file %s\n", thumbfn);
        if (LIBRAW_SUCCESS != (ret = RawProcessor->dcraw_thumb_writer(thumbfn)))
        {
          fprintf(stderr, "Cannot write %s: %s\n", thumbfn,
                  libraw_strerror(ret));
          if (LIBRAW_FATAL_ERROR(ret))
            continue;
        }
      }
      continue;
    }

    ret = RawProcessor->dcraw_process();

    if (LIBRAW_SUCCESS != ret)
    {
      fprintf(stderr, "Cannot do postprocessing on %s: %s\n", av[i],
              libraw_strerror(ret));
      if (LIBRAW_FATAL_ERROR(ret))
        continue;
    }
    snprintf(outfn, sizeof(outfn), "%s.%s", av[i],
             OUT.output_tiff ? "tiff" : (P1.colors > 1 ? "ppm" : "pgm"));

    if (verbose)
      printf("Writing file %s\n", outfn);

    if (LIBRAW_SUCCESS != (ret = RawProcessor->dcraw_ppm_tiff_writer(outfn)))
      fprintf(stderr, "Cannot write %s: %s\n", outfn, libraw_strerror(ret));

    RawProcessor->recycle(); // just for show this call
  }

  delete RawProcessor;
  return 0;
}

#ifdef __cplusplus
#include <new>
extern "C"
{
#endif

  libraw_data_t *libraw_init(unsigned int flags)
  {
    LibRaw *ret;
    try
    {
      ret = new LibRaw(flags);
    }
    catch (const std::bad_alloc &)
    {
      return NULL;
    }
    return &(ret->imgdata);
  }

  unsigned libraw_capabilities() { return LibRaw::capabilities(); }
  const char *libraw_version() { return LibRaw::version(); }
  const char *libraw_strprogress(enum LibRaw_progress p)
  {
    return LibRaw::strprogress(p);
  }
  int libraw_versionNumber() { return LibRaw::versionNumber(); }
  const char **libraw_cameraList() { return LibRaw::cameraList(); }
  int libraw_cameraCount() { return LibRaw::cameraCount(); }
  const char *libraw_unpack_function_name(libraw_data_t *lr)
  {
    if (!lr)
      return "NULL parameter passed";
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->unpack_function_name();
  }

  void libraw_subtract_black(libraw_data_t *lr)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->subtract_black();
  }

  int libraw_open_file(libraw_data_t *lr, const char *file)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->open_file(file);
  }

  libraw_iparams_t *libraw_get_iparams(libraw_data_t *lr)
  {
    if (!lr)
      return NULL;
    return &(lr->idata);
  }

  libraw_lensinfo_t *libraw_get_lensinfo(libraw_data_t *lr)
  {
    if (!lr)
      return NULL;
    return &(lr->lens);
  }

  libraw_imgother_t *libraw_get_imgother(libraw_data_t *lr)
  {
    if (!lr)
      return NULL;
    return &(lr->other);
  }

#ifndef LIBRAW_NO_IOSTREAMS_DATASTREAM
  int libraw_open_file_ex(libraw_data_t *lr, const char *file, INT64 sz)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->open_file(file, sz);
  }
#endif

#ifdef LIBRAW_WIN32_UNICODEPATHS
  int libraw_open_wfile(libraw_data_t *lr, const wchar_t *file)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->open_file(file);
  }

#ifndef LIBRAW_NO_IOSTREAMS_DATASTREAM
  int libraw_open_wfile_ex(libraw_data_t *lr, const wchar_t *file, INT64 sz)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->open_file(file, sz);
  }
#endif
#endif
  int libraw_open_buffer(libraw_data_t *lr, const void *buffer, size_t size)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->open_buffer(buffer, size);
  }
  int libraw_open_bayer(libraw_data_t *lr, unsigned char *data,
                        unsigned datalen, ushort _raw_width, ushort _raw_height,
                        ushort _left_margin, ushort _top_margin,
                        ushort _right_margin, ushort _bottom_margin,
                        unsigned char procflags, unsigned char bayer_pattern,
                        unsigned unused_bits, unsigned otherflags,
                        unsigned black_level)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->open_bayer(data, datalen, _raw_width, _raw_height, _left_margin,
                          _top_margin, _right_margin, _bottom_margin, procflags,
                          bayer_pattern, unused_bits, otherflags, black_level);
  }
  int libraw_unpack(libraw_data_t *lr)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->unpack();
  }
  int libraw_unpack_thumb(libraw_data_t *lr)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->unpack_thumb();
  }
  int libraw_unpack_thumb_ex(libraw_data_t *lr, int i)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->unpack_thumb_ex(i);
  }
  void libraw_recycle_datastream(libraw_data_t *lr)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->recycle_datastream();
  }
  void libraw_recycle(libraw_data_t *lr)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->recycle();
  }
  void libraw_close(libraw_data_t *lr)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    delete ip;
  }

  void libraw_set_exifparser_handler(libraw_data_t *lr, exif_parser_callback cb,
                                     void *data)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->set_exifparser_handler(cb, data);
  }

  void libraw_set_dataerror_handler(libraw_data_t *lr, data_callback func,
                                    void *data)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->set_dataerror_handler(func, data);
  }
  void libraw_set_progress_handler(libraw_data_t *lr, progress_callback cb,
                                   void *data)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->set_progress_handler(cb, data);
  }

  // DCRAW
  int libraw_adjust_sizes_info_only(libraw_data_t *lr)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->adjust_sizes_info_only();
  }
  int libraw_dcraw_ppm_tiff_writer(libraw_data_t *lr, const char *filename)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->dcraw_ppm_tiff_writer(filename);
  }
  int libraw_dcraw_thumb_writer(libraw_data_t *lr, const char *fname)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->dcraw_thumb_writer(fname);
  }
  int libraw_dcraw_process(libraw_data_t *lr)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->dcraw_process();
  }
  libraw_processed_image_t *libraw_dcraw_make_mem_image(libraw_data_t *lr,
                                                        int *errc)
  {
    if (!lr)
    {
      if (errc)
        *errc = EINVAL;
      return NULL;
    }
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->dcraw_make_mem_image(errc);
  }
  libraw_processed_image_t *libraw_dcraw_make_mem_thumb(libraw_data_t *lr,
                                                        int *errc)
  {
    if (!lr)
    {
      if (errc)
        *errc = EINVAL;
      return NULL;
    }
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->dcraw_make_mem_thumb(errc);
  }

  void libraw_dcraw_clear_mem(libraw_processed_image_t *p)
  {
    LibRaw::dcraw_clear_mem(p);
  }

  int libraw_raw2image(libraw_data_t *lr)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->raw2image();
  }
  void libraw_free_image(libraw_data_t *lr)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->free_image();
  }
  int libraw_get_decoder_info(libraw_data_t *lr, libraw_decoder_info_t *d)
  {
    if (!lr || !d)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->get_decoder_info(d);
  }
  int libraw_COLOR(libraw_data_t *lr, int row, int col)
  {
    if (!lr)
      return EINVAL;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    return ip->COLOR(row, col);
  }

  /* getters/setters used by 3DLut Creator */
  DllDef void libraw_set_demosaic(libraw_data_t *lr, int value)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->imgdata.params.user_qual = value;
  }

  DllDef void libraw_set_output_color(libraw_data_t *lr, int value)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->imgdata.params.output_color = value;
  }

  DllDef void libraw_set_adjust_maximum_thr(libraw_data_t *lr, float value)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->imgdata.params.adjust_maximum_thr = value;
  }

  DllDef void libraw_set_output_bps(libraw_data_t *lr, int value)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->imgdata.params.output_bps = value;
  }

  DllDef void libraw_set_output_tif(libraw_data_t *lr, int value)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->imgdata.params.output_tiff = value;
  }

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define LIM(x, min, max) MAX(min, MIN(x, max))

  DllDef void libraw_set_user_mul(libraw_data_t *lr, int index, float val)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->imgdata.params.user_mul[LIM(index, 0, 3)] = val;
  }

  DllDef void libraw_set_gamma(libraw_data_t *lr, int index, float value)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->imgdata.params.gamm[LIM(index, 0, 5)] = value;
  }

  DllDef void libraw_set_no_auto_bright(libraw_data_t *lr, int value)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->imgdata.params.no_auto_bright = value;
  }

  DllDef void libraw_set_bright(libraw_data_t *lr, float value)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->imgdata.params.bright = value;
  }

  DllDef void libraw_set_highlight(libraw_data_t *lr, int value)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->imgdata.params.highlight = value;
  }

  DllDef void libraw_set_fbdd_noiserd(libraw_data_t *lr, int value)
  {
    if (!lr)
      return;
    LibRaw *ip = (LibRaw *)lr->parent_class;
    ip->imgdata.params.fbdd_noiserd = value;
  }

  DllDef int libraw_get_raw_height(libraw_data_t *lr)
  {
    if (!lr)
      return EINVAL;
    return lr->sizes.raw_height;
  }

  DllDef int libraw_get_raw_width(libraw_data_t *lr)
  {
    if (!lr)
      return EINVAL;
    return lr->sizes.raw_width;
  }

  DllDef int libraw_get_iheight(libraw_data_t *lr)
  {
    if (!lr)
      return EINVAL;
    return lr->sizes.iheight;
  }

  DllDef int libraw_get_iwidth(libraw_data_t *lr)
  {
    if (!lr)
      return EINVAL;
    return lr->sizes.iwidth;
  }

  DllDef float libraw_get_cam_mul(libraw_data_t *lr, int index)
  {
    if (!lr)
      return EINVAL;
    return lr->color.cam_mul[LIM(index, 0, 3)];
  }

  DllDef float libraw_get_pre_mul(libraw_data_t *lr, int index)
  {
    if (!lr)
      return EINVAL;
    return lr->color.pre_mul[LIM(index, 0, 3)];
  }

  DllDef float libraw_get_rgb_cam(libraw_data_t *lr, int index1, int index2)
  {
    if (!lr)
      return EINVAL;
    return lr->color.rgb_cam[LIM(index1, 0, 2)][LIM(index2, 0, 3)];
  }

  DllDef int libraw_get_color_maximum(libraw_data_t *lr)
  {
    if (!lr)
      return EINVAL;
    return lr->color.maximum;
  }

#ifdef __cplusplus
}
#endif
