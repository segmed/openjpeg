#include "opj_includes.h"
#include "openjpeg.h"
#include "format_defs.h"
#include "color.h"

typedef struct myfile
{
  char *mem;
  char *cur;
  size_t len;
} myfile;

OPJ_SIZE_T opj_read_from_memory(void * p_buffer, OPJ_SIZE_T p_nb_bytes, myfile* p_file)
{
  OPJ_SIZE_T l_nb_read;
  if( p_file->cur + p_nb_bytes <= p_file->mem + p_file->len )
    {
        l_nb_read = 1*p_nb_bytes;
    }
  else
    {
        l_nb_read = (OPJ_SIZE_T)(p_file->mem + p_file->len - p_file->cur);
    }
  memcpy(p_buffer,p_file->cur,l_nb_read);
  p_file->cur += l_nb_read;
  return l_nb_read ? l_nb_read : ((OPJ_SIZE_T)-1);
}

OPJ_SIZE_T opj_write_from_memory (void * p_buffer, OPJ_SIZE_T p_nb_bytes, myfile* p_file)
{
  OPJ_SIZE_T l_nb_write;
  l_nb_write = 1*p_nb_bytes;
  memcpy(p_file->cur,p_buffer,l_nb_write);
  p_file->cur += l_nb_write;
  p_file->len += l_nb_write;
  return l_nb_write;
}

OPJ_OFF_T opj_skip_from_memory (OPJ_OFF_T p_nb_bytes, myfile * p_file)
{
  if( p_file->cur + p_nb_bytes <= p_file->mem + p_file->len )
    {
    p_file->cur += p_nb_bytes;
    return p_nb_bytes;
    }

  p_file->cur = p_file->mem + p_file->len;
  return -1;
}

OPJ_BOOL opj_seek_from_memory (OPJ_OFF_T p_nb_bytes, myfile * p_file)
{
  if( (size_t)p_nb_bytes <= p_file->len )
    {
    p_file->cur = p_file->mem + p_nb_bytes;
    return OPJ_TRUE;
    }

  p_file->cur = p_file->mem + p_file->len;
  return OPJ_FALSE;
}

opj_stream_t* OPJ_CALLCONV opj_stream_create_buffer_stream(
    char *buf,
    OPJ_SIZE_T buf_size,
    OPJ_SIZE_T p_size,
    OPJ_BOOL p_is_read_stream)
{
    opj_stream_t* l_stream = 00;

    if (! buf) {
        return NULL;
    }

    l_stream = opj_stream_create(p_size, p_is_read_stream);
    if (! l_stream) {
        return NULL;
    }

    struct myfile mysrc;
    struct myfile *fsrc = &mysrc;
    fsrc->mem = fsrc->cur = (char*)buf;
    fsrc->len = buf_size; //inputlength;

    opj_stream_set_user_data(l_stream, fsrc, NULL);
    opj_stream_set_user_data_length(l_stream, buf_size);
    // opj_stream_set_current_data(l_stream, buf, buf_size);

    opj_stream_set_read_function(l_stream, (opj_stream_read_fn) opj_read_from_memory);
    opj_stream_set_write_function(l_stream, (opj_stream_write_fn) opj_write_from_memory);
    opj_stream_set_skip_function(l_stream, (opj_stream_skip_fn) opj_skip_from_memory);
    opj_stream_set_seek_function(l_stream, (opj_stream_seek_fn) opj_seek_from_memory);
    return l_stream;
}

opj_stream_t* OPJ_CALLCONV opj_stream_create_default_buffer_stream(
    char *buf, OPJ_SIZE_T buf_size, OPJ_BOOL p_is_read_stream)
{
    return opj_stream_create_buffer_stream(buf, buf_size, OPJ_J2K_STREAM_CHUNK_SIZE,
                                         p_is_read_stream);
}


typedef enum opj_prec_mode {
    OPJ_PREC_MODE_CLIP,
    OPJ_PREC_MODE_SCALE
} opj_precision_mode;

typedef struct opj_prec {
    OPJ_UINT32         prec;
    opj_precision_mode mode;
} opj_precision;

typedef struct opj_decompress_params {
    /** core library parameters */
    opj_dparameters_t core;

    /** input file name */
    char infile[OPJ_PATH_LEN];
    /** output file name */
    char outfile[OPJ_PATH_LEN];
    /** input file format 0: J2K, 1: JP2, 2: JPT */
    int decod_format;
    /** output file format 0: PGX, 1: PxM, 2: BMP */
    int cod_format;
    /** index file name */
    char indexfilename[OPJ_PATH_LEN];

    /** Decoding area left boundary */
    OPJ_UINT32 DA_x0;
    /** Decoding area right boundary */
    OPJ_UINT32 DA_x1;
    /** Decoding area up boundary */
    OPJ_UINT32 DA_y0;
    /** Decoding area bottom boundary */
    OPJ_UINT32 DA_y1;
    /** Verbose mode */
    OPJ_BOOL m_verbose;

    /** tile number of the decoded tile */
    OPJ_UINT32 tile_index;
    /** Nb of tile to decode */
    OPJ_UINT32 nb_tile_to_decode;

    opj_precision* precision;
    OPJ_UINT32     nb_precision;

    /* force output colorspace to RGB */
    int force_rgb;
    /* upsample components according to their dx/dy values */
    int upsample;
    /* split output components to different files */
    int split_pnm;
    /** number of threads */
    int num_threads;
    /* Quiet */
    int quiet;
    /* Allow partial decode */
    int allow_partial;
    /** number of components to decode */
    OPJ_UINT32 numcomps;
    /** indices of components to decode */
    OPJ_UINT32* comps_indices;
} opj_decompress_parameters;

static void set_default_parameters(opj_decompress_parameters* parameters)
{
    if (parameters) {
        memset(parameters, 0, sizeof(opj_decompress_parameters));

        /* default decoding parameters (command line specific) */
        parameters->decod_format = -1;
        parameters->cod_format = -1;

        /* default decoding parameters (core) */
        opj_set_default_decoder_parameters(&(parameters->core));
    }
}

opj_image_t* opj_decompress(
    char *buf,
    size_t buf_size,
    int decod_format,
    int cod_format
    )
{
    opj_image_t* image = NULL;
    opj_stream_t *l_stream = NULL;              /* Stream */
    opj_codec_t* l_codec = NULL;                /* Handle to a decompressor */
    opj_decompress_parameters parameters;           /* decompression parameters */

    /* set decoding parameters to default values */
    set_default_parameters(&parameters);

    l_stream = opj_stream_create_default_buffer_stream(buf, buf_size, 1);
    if (!l_stream) {
        fprintf(stderr, "ERROR -> failed to create the stream from buffer\n");
        return 0;
    }
    /* get a decoder handle */
    switch(decod_format)
    {
    case J2K_CFMT:
        l_codec = opj_create_decompress(OPJ_CODEC_J2K);
        break;
    case JP2_CFMT:
        l_codec = opj_create_decompress(OPJ_CODEC_JP2);
        break;
    default:
        fprintf(stderr, "Impossible happen" );
        return 0;
    }
    /* decode the JPEG2000 stream */
    /* ---------------------- */
    if (!opj_setup_decoder(l_codec, &(parameters.core))) {
        fprintf(stderr, "ERROR -> opj_decompress: failed to setup the decoder\n");
        opj_stream_destroy(l_stream);
        opj_destroy_codec(l_codec);
        return 0;
    }

    /* Read the main header of the codestream and if necessary the JP2 boxes*/
    if (! opj_read_header(l_stream, l_codec, &image)) {
        fprintf(stderr, "ERROR -> j2k_to_image: failed to read the header\n");
        opj_stream_destroy(l_stream);
        opj_destroy_codec(l_codec);
        opj_image_destroy(image);
        return 0;
    }
    /* Optional if you want decode the entire image */
     if (!opj_set_decode_area(l_codec, image, (OPJ_INT32)0,
                                  (OPJ_INT32)0, (OPJ_INT32)0,
                                  (OPJ_INT32)0)) {
        fprintf(stderr, "ERROR -> opj_decompress: failed to set the decoded area\n");
        opj_stream_destroy(l_stream);
        opj_destroy_codec(l_codec);
        opj_image_destroy(image);
        return 0;
    }

    /* Get the decoded image */
    if (!(opj_decode(l_codec, l_stream, image) &&
            opj_end_decompress(l_codec,   l_stream))) {
        fprintf(stderr, "ERROR -> opj_decompress: failed to decode image!\n");
        opj_destroy_codec(l_codec);
        opj_stream_destroy(l_stream);
        opj_image_destroy(image);
        return 0;
    }

    if (image->color_space != OPJ_CLRSPC_SYCC
            && image->numcomps == 3 && image->comps[0].dx == image->comps[0].dy
            && image->comps[1].dx != 1) {
        image->color_space = OPJ_CLRSPC_SYCC;
    } else if (image->numcomps <= 2) {
        image->color_space = OPJ_CLRSPC_GRAY;
    }

    if (image->color_space == OPJ_CLRSPC_SYCC) {
        color_sycc_to_rgb(image);
    } else if ((image->color_space == OPJ_CLRSPC_CMYK) &&
               (parameters.cod_format != TIF_DFMT)) {
        color_cmyk_to_rgb(image);
    } else if (image->color_space == OPJ_CLRSPC_EYCC) {
        color_esycc_to_rgb(image);
    }

    /* free remaining structures */
    if (l_codec) {
        opj_destroy_codec(l_codec);
        opj_stream_destroy(l_stream);
    }
    return image;
}

opj_image_t* decode_j2k(
    char *buf,
    size_t buf_size
    )
{
    int decod_format;
    unsigned char *src = (unsigned char*)buf;
    // 32bits truncation should be ok since DICOM cannot have larger than 2Gb image
    uint32_t file_length = (uint32_t)buf_size;
    // WARNING: OpenJPEG is very picky when there is a trailing 00 at the end of the JPC
    // so we need to make sure to remove it.
    // Marker 0xffd9 EOI End of Image (JPEG 2000 EOC End of codestream)
    // gdcmData/D_CLUNIE_CT1_J2KR.dcm contains a trailing 0xFF which apparently is ok...
    // Ref: https://github.com/malaterre/GDCM/blob/master/Source/MediaStorageAndFileFormat/gdcmJPEG2000Codec.cxx#L637
    while( file_length > 0 && src[file_length-1] != 0xd9 )
    {
        file_length--;
    }
    // what if 0xd9 is never found ?
    if( !( file_length > 0 && src[file_length-1] == 0xd9 ) )
    {
        return 0;
    }
    // https://github.com/malaterre/GDCM/blob/master/Source/MediaStorageAndFileFormat/gdcmJPEG2000Codec.cxx#L656
    const char jp2magic[] = "\x00\x00\x00\x0C\x6A\x50\x20\x20\x0D\x0A\x87\x0A";
    if( memcmp( src, jp2magic, sizeof(jp2magic) ) == 0 )
    {
        /* JPEG-2000 compressed image data ... sigh */
        fprintf(stderr, "J2K start like JPEG-2000 compressed image data instead of codestream" );
        decod_format = JP2_CFMT;
    }
    else
    {
        /* JPEG-2000 codestream */
        decod_format = J2K_CFMT;
    }
    return opj_decompress(src, file_length, decod_format, PXM_DFMT);
}

// Encode RGB bytes array to an OPJ_IMAGE
opj_image_t* bytestoimage(char *buf, int w, int h, int prec, int numcomps)
{
    /* decode the source image */
    /* ----------------------- */
    opj_cparameters_t parameters;   /* compression parameters */

    /* set encoding parameters to default values */
    opj_set_default_encoder_parameters(&parameters);

    int subsampling_dx = parameters.subsampling_dx;
    int subsampling_dy = parameters.subsampling_dy;

    int i, compno;
    OPJ_COLOR_SPACE color_space;
    opj_image_cmptparm_t cmptparm[4]; /* RGBA: max. 4 components */
    opj_image_t * image = NULL;

    if (numcomps < 3) {
        color_space = OPJ_CLRSPC_GRAY;    /* GRAY, GRAYA */
    } else {
        color_space = OPJ_CLRSPC_SRGB;    /* RGB, RGBA */
    }

    if (prec < 8) {
        prec = 8;
    }

    subsampling_dx = parameters.subsampling_dx;
    subsampling_dy = parameters.subsampling_dy;

    memset(&cmptparm[0], 0, (size_t)numcomps * sizeof(opj_image_cmptparm_t));

    for (i = 0; i < numcomps; i++) {
        cmptparm[i].prec = (OPJ_UINT32)prec;
        cmptparm[i].sgnd = 0;
        cmptparm[i].dx = (OPJ_UINT32)subsampling_dx;
        cmptparm[i].dy = (OPJ_UINT32)subsampling_dy;
        cmptparm[i].w = (OPJ_UINT32)w;
        cmptparm[i].h = (OPJ_UINT32)h;
    }
    image = opj_image_create((OPJ_UINT32)numcomps, &cmptparm[0], color_space);

    if (!image) {
        fprintf(stderr, "failed to create image: opj_image_create\n");
        return NULL;
    }

    /* set image offset and reference grid */
    image->x0 = (OPJ_UINT32)parameters.image_offset_x0;
    image->y0 = (OPJ_UINT32)parameters.image_offset_y0;
    image->x1 = (OPJ_UINT32)(parameters.image_offset_x0 + (w - 1) * subsampling_dx
                             + 1);
    image->y1 = (OPJ_UINT32)(parameters.image_offset_y0 + (h - 1) * subsampling_dy
                             + 1);

    for (i = 0; i < w * h; i++) {
        for (compno = 0; compno < numcomps; compno++) {
            image->comps[compno].data[i] = buf[i];
        }
    }
    return image;
}
