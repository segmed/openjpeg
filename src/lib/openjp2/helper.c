#include "opj_includes.h"
#include "format_defs.h"
#include "color.h"

opj_stream_t* OPJ_CALLCONV opj_stream_create_buffer_stream(
    char *buf,
    OPJ_SIZE_T buf_size,
    OPJ_SIZE_T p_size,
    OPJ_BOOL p_is_read_stream)
{
    opj_stream_t* l_stream = 00;
    const char *mode;

    if (! buf) {
        return NULL;
    }

    if (p_is_read_stream) {
        mode = "rb";
    } else {
        mode = "wb";
    }

    l_stream = opj_stream_create(p_size, p_is_read_stream);
    if (! l_stream) {
        return NULL;
    }

    opj_stream_set_user_data(l_stream, NULL, NULL);
    opj_stream_set_user_data_length(l_stream, buf_size);
    opj_stream_set_current_data(l_stream, buf, buf_size);
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

    /* decode the JPEG2000 stream */
    /* ---------------------- */

    l_codec = opj_create_decompress(OPJ_CODEC_J2K);
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
    }
    return image;
}

opj_image_t* decode_j2k(
    char *buf,
    size_t buf_size
    )
{
    return opj_decompress(buf, buf_size, J2K_CFMT, PXM_DFMT);
}
