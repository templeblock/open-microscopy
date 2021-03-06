/*
 * Copyright 2007-2010 Glenn Pierce, Paul Barber,
 * Oxford University (Gray Institute for Radiation Oncology and Biology) 
 *
 * This file is part of FreeImageAlgorithms.
 *
 * FreeImageAlgorithms is free software: you can redistribute it and/or modify
 * it under the terms of the Lesser GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FreeImageAlgorithms is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * Lesser GNU General Public License for more details.
 *
 * You should have received a copy of the Lesser GNU General Public License
 * along with FreeImageAlgorithms.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "FreeImageAlgorithms.h"
#include "FreeImageAlgorithms_Utils.h"
#include "FreeImageAlgorithms_Drawing.h"
#include "FreeImageAlgorithms_Palettes.h"
#include "FreeImageAlgorithms_Utilities.h"

#include <iostream>
#include <math.h>

#include "agg_basics.h"
#include "agg_rendering_buffer.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_rasterizer_outline.h"
#include "agg_renderer_primitives.h"
#include "agg_alpha_mask_u8.h"
#include "agg_scanline_p.h"
#include "agg_scanline_u.h"
#include "agg_renderer_scanline.h"
#include "agg_pixfmt_rgb.h"
#include "agg_pixfmt_rgba.h"
#include "agg_pixfmt_gray.h"
#include "agg_path_storage.h"
#include "agg_conv_stroke.h"
#include "agg_ellipse.h"
#include "agg_glyph_raster_bin.h"
#include "agg_renderer_raster_text.h"
#include "agg_embedded_raster_fonts.h"
#include "agg_trans_affine.h"
#include "agg_span_interpolator_linear.h"
#include "agg_span_allocator.h"
#include "agg_conv_transform.h"
#include "agg_span_image_filter_rgba.h"
#include "agg_scanline_bin.h"
#include "agg_conv_curve.h"
#include "agg_conv_contour.h"
#include "agg_gamma_lut.h"

#ifdef WIN32
#include "agg_font_win32_tt.h"
#else
//#include "agg_font_freetype.h"
#endif

typedef struct _FIA_Matrix 
{
  agg::trans_affine trans_affine;

} FIA_Matrix;

FIA_Matrix * DLL_CALLCONV
FIA_MatrixNew()
{
    FIA_Matrix *matrix = (FIA_Matrix*) malloc(sizeof(FIA_Matrix));

    matrix->trans_affine.reset();
    
    return matrix;
}

FIA_Matrix * DLL_CALLCONV
FIA_MatrixNewWithValues(double v0, double v1, double v2, 
                                        double v3, double v4, double v5)
{
    FIA_Matrix *matrix = (FIA_Matrix*) malloc(sizeof(FIA_Matrix));

    matrix->trans_affine.reset();
 
    FIA_MatrixSetValues(matrix, v0, v1, v2, v3, v4, v5);
    
    return matrix;
}

int DLL_CALLCONV
FIA_MatrixDestroy(FIA_Matrix *matrix)
{
    free(matrix);
      
    return FIA_SUCCESS;
}

int DLL_CALLCONV
FIA_MatrixSetValues(FIA_Matrix *matrix, double v0, double v1, double v2, 
                                        double v3, double v4, double v5)
{
    double values[6];
    
    values[0] = v0;
    values[1] = v1;
    values[2] = v2;
    values[3] = v3;
    values[4] = v4;
    values[5] = v5;   
    
    matrix->trans_affine.load_from(values);
    
    return FIA_SUCCESS;
}

int DLL_CALLCONV
FIA_MatrixScale(FIA_Matrix *matrix, double x, double y, FIA_MatrixOrder order)
{
  if(order == FIA_MatrixOrderAppend) {
  
    matrix->trans_affine.scale(x, y);
  }
  else {
  
      agg::trans_affine t;
      t.scale(x, y);
      
      matrix->trans_affine.premultiply(t);
  }

  return FIA_SUCCESS;
}

int DLL_CALLCONV
FIA_MatrixRotate(FIA_Matrix *matrix, double a, FIA_MatrixOrder order)
{
    if(order == FIA_MatrixOrderAppend) {
  
      matrix->trans_affine.rotate(a * agg::pi / 180.0);
    }
    else {
  
      agg::trans_affine t;
      t.rotate(a * agg::pi / 180.0);
      
      matrix->trans_affine.premultiply(t);
    }
  
    return FIA_SUCCESS;
}

int DLL_CALLCONV
FIA_MatrixTranslate(FIA_Matrix *matrix, double x, double y, FIA_MatrixOrder order)
{
   if(order == FIA_MatrixOrderAppend) {
   
      matrix->trans_affine.translate(x, y);
    }
    else {
    
      agg::trans_affine t;
      t.translate(x, y);
      
      matrix->trans_affine.premultiply(t);
    }
    
    return FIA_SUCCESS;
}

int DLL_CALLCONV
FIA_MatrixInvert(FIA_Matrix *matrix)
{
    matrix->trans_affine.invert();

    return FIA_SUCCESS;
}

static int
DrawTransformedImage (FIBITMAP *src, FIBITMAP *dst, agg::trans_affine image_mtx, RGBQUAD colour, int retain_background)
{
    typedef agg::pixfmt_bgra32                       src_pixfmt_type;
    typedef agg::pixfmt_bgra32                       dst_pixfmt_type;
    typedef agg::renderer_base < dst_pixfmt_type >   renbase_type;
        
    int src_width = FreeImage_GetWidth (src);
    int src_height = FreeImage_GetHeight (src);
    
    int dst_width = FreeImage_GetWidth (dst);
    int dst_height = FreeImage_GetHeight (dst);

    //Due to the nature of the algorithm you 
    //have to use the inverse transformations. The algorithm takes the 
    //coordinates of every destination pixel, applies the transformations and 
    //obtains the coordinates of the pixel to pick it up from the source image. 
    image_mtx.invert();

    typedef agg::span_interpolator_linear<> interpolator_type;
    interpolator_type interpolator(image_mtx);
    agg::span_allocator<agg::rgba8> sa;
    
    FREE_IMAGE_TYPE type = FreeImage_GetImageType (src);

    unsigned char *src_bits = FreeImage_GetBits (src);
    
    unsigned char *dst_bits = FreeImage_GetBits (dst);

    int src_pitch = FreeImage_GetPitch (src);
    int dst_pitch = FreeImage_GetPitch (dst);

    // Create the src buffer
    agg::rendering_buffer rbuf_src (src_bits, src_width, src_height, -src_pitch);
    
    // Create the dst buffer
    agg::rendering_buffer rbuf_dst (dst_bits, dst_width, dst_height, -dst_pitch);
       
    dst_pixfmt_type pixf_dst(rbuf_dst);
    renbase_type rbase(pixf_dst);
    
    src_pixfmt_type pixf_src(rbuf_src);

    // "hardcoded" bilinear filter
    typedef agg::span_image_filter_rgba_bilinear_clip<src_pixfmt_type, interpolator_type> span_gen_type;
//    typedef agg::span_image_filter_rgba_bilinear_clone<src_pixfmt_type, interpolator_type> span_gen_type;

    span_gen_type sg(pixf_src, agg::rgba8(colour.rgbRed,colour.rgbGreen,colour.rgbBlue,
            (retain_background ? 0 : 255)), interpolator);
       
//    span_gen_type sg(pixf_src, interpolator);
 
    // The Rasterizer definition  
    agg::rasterizer_scanline_aa<> ras;
    agg::scanline_p8 scanline;

    agg::path_storage ps;

    //Clip to image boundary
    ps.move_to(0.0, 0.0);
    ps.line_to(dst_width, 0.0);
    ps.line_to(dst_width, dst_height);
    ps.line_to(0.0, dst_height);
    ps.line_to(0.0, 0.0);
    
    ras.add_path(ps);
      
    agg::render_scanlines_aa(ras, scanline, rbase, sa, sg);

    return FIA_SUCCESS;
}

FIBITMAP* DLL_CALLCONV
FIA_AffineTransform(FIBITMAP *src, int image_dst_width, int image_dst_height,
  FIA_Matrix *matrix, RGBQUAD colour, int retain_background)
{
    FIBITMAP *src32 = FreeImage_ConvertTo32Bits(src);
   
    FIBITMAP *dst = FreeImage_Allocate(image_dst_width, image_dst_height, 32, 0, 0, 0);

    if(FreeImage_GetImageType(src) != FIT_BITMAP || FreeImage_GetBPP(src) != 32) {
        src32 = FreeImage_ConvertTo32Bits(src);
    }
    else {
        src32 = FreeImage_Clone(src);
    }

    DrawTransformedImage (src32, dst, matrix->trans_affine, colour, retain_background);
        
    FreeImage_Unload(src32);
        
    return dst;
}


int DLL_CALLCONV
FIA_DrawImageFromSrcToDst(FIBITMAP *dst, FIBITMAP *src, FIA_Matrix *matrix,
              int dstLeft, int dstTop, int dstWidth, int dstHeight,
              int srcLeft, int srcTop, int srcWidth, int srcHeight, RGBQUAD colour, int retain_background)
{
    if (FreeImage_GetImageType (dst) != FIT_BITMAP)
    {
        FreeImage_OutputMessageProc (FIF_UNKNOWN,
                                     "Destination image is not of type FIT_BITMAP");
        return FIA_ERROR;
    }
    
    if (FreeImage_GetBPP (dst) != 32)
    {
        FreeImage_OutputMessageProc (FIF_UNKNOWN,
                                     "Destination image is not 32 bpp");
        return FIA_ERROR;
    }

    FIBITMAP* src_region = FIA_Copy(src, srcLeft, srcTop,
            srcLeft + srcWidth - 1, srcTop + srcHeight - 1);

    if(src_region == NULL) {

        FreeImage_OutputMessageProc (FIF_UNKNOWN, "Failed to extract region of given src rectangle");

        return FIA_ERROR;
    }
    
    FIBITMAP *src32 = NULL;
   
    if(FreeImage_GetImageType(src_region) != FIT_BITMAP || FreeImage_GetBPP(src_region) != 32) {
        src32 = FreeImage_ConvertTo32Bits(src_region);
    }
    else {
        src32 = FreeImage_Clone(src_region);
    }

    FIA_Matrix* dstMatrix = FIA_MatrixNew();
    
    double scalex = (double) dstWidth / srcWidth;
    double scaley = (double) dstHeight / srcHeight;

    FIA_MatrixTranslate(dstMatrix, dstLeft - 1, dstTop - 1, FIA_MatrixOrderPrepend);
    FIA_MatrixScale(dstMatrix, scalex, scaley, FIA_MatrixOrderPrepend);

    if(matrix != NULL) {
        dstMatrix->trans_affine *= matrix->trans_affine;
    }

    DrawTransformedImage (src32, dst, dstMatrix->trans_affine, colour, retain_background);
        
    FIA_MatrixDestroy(dstMatrix);

    FreeImage_Unload(src32);
    FreeImage_Unload(src_region);
        
    return FIA_SUCCESS;
}

int DLL_CALLCONV
FIA_DrawImageToDst(FIBITMAP *dst, FIBITMAP *src, FIA_Matrix *matrix,
              int dstLeft, int dstTop, int dstWidth, int dstHeight, RGBQUAD colour, int retain_background)
{
    if (FreeImage_GetImageType (dst) != FIT_BITMAP)
    {
        FreeImage_OutputMessageProc (FIF_UNKNOWN,
                                     "Destination image is not of type FIT_BITMAP");
        return FIA_ERROR;
    }
    
    if (FreeImage_GetBPP (dst) != 32)
    {
        FreeImage_OutputMessageProc (FIF_UNKNOWN,
                                     "Destination image is not 32 bpp");
        return FIA_ERROR;
    }

    
    FIBITMAP *src32 = NULL;
   
    if(FreeImage_GetImageType(src) != FIT_BITMAP || FreeImage_GetBPP(src) != 32) {
        src32 = FreeImage_ConvertTo32Bits(src);
    }
    else {
        src32 = FreeImage_Clone(src);
    }

    FIA_Matrix* dstMatrix = FIA_MatrixNew();
    
    double scalex = (double) dstWidth / FreeImage_GetWidth(src);
    double scaley = (double) dstHeight / FreeImage_GetHeight(src);

    FIA_MatrixTranslate(dstMatrix, dstLeft - 1, dstTop - 1, FIA_MatrixOrderPrepend);
    FIA_MatrixScale(dstMatrix, scalex, scaley, FIA_MatrixOrderPrepend);

    if(matrix != NULL) {
        dstMatrix->trans_affine *= matrix->trans_affine;
    }

    DrawTransformedImage (src32, dst, dstMatrix->trans_affine, colour, retain_background);
        
    FIA_MatrixDestroy(dstMatrix);

    FreeImage_Unload(src32);
        
    return FIA_SUCCESS;
}

template<class Rasterizer>
static void
RasterLine (Rasterizer& ras, double x1, double y1, double x2, double y2, double width)
{
    double dx = x2 - x1;
    double dy = y2 - y1;
    double d = sqrt (dx * dx + dy * dy);

    dx = 0.5 * width * (y2 - y1) / d;
    dy = 0.5 * width * (x2 - x1) / d;

    ras.move_to_d (x1 - dx, y1 + dy);
    ras.line_to_d (x2 - dx, y2 + dy);
    ras.line_to_d (x2 + dx, y2 - dy);
    ras.line_to_d (x1 + dx, y1 - dy);
}

template<class RendererType>
static int
DrawLine (RendererType& renderer, FIBITMAP * src, FIAPOINT p1, FIAPOINT p2, RGBQUAD colour, int line_width, int antialiased)
{
    // the Rasterizer definition
    agg::rasterizer_scanline_aa<> ras;
    ras.reset();
 
    agg::scanline_p8 scanline;

    RasterLine (ras, p1.x, p1.y, p2.x, p2.y, line_width);

    if (antialiased)
    {
        agg::render_scanlines_aa_solid(ras, scanline, renderer, agg::rgba8(colour.rgbRed, colour.rgbGreen, colour.rgbBlue));
    }
    else
    {
        agg::render_scanlines_bin_solid(ras, scanline, renderer, agg::rgba8 (colour.rgbRed, colour.rgbGreen, colour.rgbBlue));
    }

    return FIA_SUCCESS;
}

template<typename RendererType, typename ColorT>
static int
DrawEllipse (RendererType& renderer, FIBITMAP * src, FIARECT rect, const ColorT& colour, int solid, double line_width, int antialiased)
{
    int width = FreeImage_GetWidth (src);
    int height = FreeImage_GetHeight (src);

	if(rect.left > rect.right)
		SWAP(rect.left, rect.right);

	if(rect.top < rect.bottom)
		SWAP(rect.top, rect.bottom);

    // Allocate the framebuffer
    FIARECT tmp_rect = rect;

    // FreeImages are flipped
    tmp_rect.top = height - rect.top - 1;
    tmp_rect.bottom = height - rect.bottom - 1;

    // the Rasterizer definition
    agg::rasterizer_scanline_aa<> ras;
    ras.reset();

    agg::scanline_p8 scanline;

    agg::ellipse ellipse;

    int w = tmp_rect.right - tmp_rect.left + 1;
    int h = tmp_rect.bottom - tmp_rect.top + 1;

    double center_x = (double)(tmp_rect.left + ((double) w / 2.0));
    double center_y = (double)(tmp_rect.top + ((double) h / 2.0));

    ellipse.init(center_x, center_y, w / 2.0 - 0.5, h / 2.0 - 0.5, 360, true);
    
    if(solid) {
        ras.add_path(ellipse, 0);
    }
    else {      
        agg::conv_stroke<agg::ellipse> poly(ellipse);
        poly.width(line_width);
        ras.add_path(poly, 0);
    }

    if(antialiased)
        agg::render_scanlines_aa_solid(ras, scanline, renderer, colour);
    else
        agg::render_scanlines_bin_solid(ras, scanline, renderer, colour);

    return FIA_SUCCESS;
}

int DLL_CALLCONV
FIA_DrawColourSolidEllipse (FIBITMAP * src, FIARECT rect, RGBQUAD colour, int antialiased)
{
    int width = FreeImage_GetWidth (src);
    int height = FreeImage_GetHeight (src);

    FREE_IMAGE_TYPE type = FreeImage_GetImageType (src);
    int bpp = FreeImage_GetBPP(src);

    unsigned char *buf = FreeImage_GetBits (src);

    // Create the rendering buffer
    agg::rendering_buffer rbuf (buf, width, height, FreeImage_GetPitch (src));

    if (type == FIT_BITMAP && bpp == 32)
    {
        typedef agg::pixfmt_bgra32                       pixfmt_type;
        typedef agg::renderer_base < pixfmt_type >       renbase_type;
     
        pixfmt_type pixf(rbuf);
        renbase_type rbase(pixf);

        return DrawEllipse (rbase, src, rect, agg::rgba8 (colour.rgbRed, colour.rgbGreen, colour.rgbBlue), 1, 0, antialiased);
    }
    else if (type == FIT_BITMAP && bpp == 24)
    {
        typedef agg::pixfmt_bgr24                        pixfmt_type;
        typedef agg::renderer_base < pixfmt_type >       renbase_type;

        pixfmt_type pixf(rbuf);
        renbase_type rbase(pixf);

        return DrawEllipse (rbase, src, rect, agg::rgba8 (colour.rgbRed, colour.rgbGreen, colour.rgbBlue), 1, 0, antialiased);
    }

    return FIA_ERROR;
}


int DLL_CALLCONV
FIA_DrawSolidGreyscaleEllipse (FIBITMAP * src, FIARECT rect, unsigned char value, int antialiased)
{
    int width = FreeImage_GetWidth (src);
    int height = FreeImage_GetHeight (src);

    FREE_IMAGE_TYPE type = FreeImage_GetImageType (src);

    // Allocate the framebuffer
    unsigned char *buf = FreeImage_GetBits (src);

    switch (type)
    {
        case FIT_BITMAP:
        {                       // standard image: 1-, 4-, 8-, 16-, 24-, 32-bit
            if (FreeImage_GetBPP (src) == 8) {
                
                typedef agg::pixfmt_gray8                        pixfmt_type;
                typedef agg::renderer_base < pixfmt_type >       renbase_type;

                // Create the rendering buffer
                agg::rendering_buffer rbuf (buf, width, height, FreeImage_GetPitch (src));

                pixfmt_type pixf(rbuf);
                renbase_type rbase(pixf);

                RGBQUAD colour = FIA_RGBQUAD ((unsigned char) value, (unsigned char) value, (unsigned char) value);

                return DrawEllipse (rbase, src, rect, agg::rgba8 (colour.rgbRed, colour.rgbGreen, colour.rgbBlue), 1, 0, antialiased);
            }

            break;
        }
        case FIT_UINT16:
        {      
            typedef agg::pixfmt_gray16                       pixfmt_type;
            typedef agg::renderer_base < pixfmt_type >       renbase_type;

            // Create the rendering buffer
            agg::rendering_buffer rbuf (buf, width, height, FreeImage_GetPitch (src));

            pixfmt_type pixf(rbuf);
            renbase_type rbase(pixf);

            return DrawEllipse (rbase, src, rect, agg::gray16(value), 1, 0, antialiased);

       }
        case FIT_INT16:
        {   
            break;
        }
        case FIT_UINT32:
        {             
            break;
        }
        case FIT_INT32:
        {
            break;
        }
        case FIT_FLOAT:
        {
            break;
        }
        case FIT_DOUBLE:
        { 
            break;
        }
        default:
        {
            break;
        }
    }

    return FIA_ERROR;
}


int DLL_CALLCONV
FIA_DrawGreyscaleEllipse (FIBITMAP * src, FIARECT rect, unsigned char value, int antialiased)
{
    int width = FreeImage_GetWidth (src);
    int height = FreeImage_GetHeight (src);

    FREE_IMAGE_TYPE type = FreeImage_GetImageType (src);

    // Allocate the framebuffer
    unsigned char *buf = FreeImage_GetBits (src);

    switch (type)
    {
        case FIT_BITMAP:
        {                       // standard image: 1-, 4-, 8-, 16-, 24-, 32-bit
            if (FreeImage_GetBPP (src) == 8) {
                
                typedef agg::pixfmt_gray8                        pixfmt_type;
                typedef agg::renderer_base < pixfmt_type >       renbase_type;

                // Create the rendering buffer
                agg::rendering_buffer rbuf (buf, width, height, FreeImage_GetPitch (src));

                pixfmt_type pixf(rbuf);
                renbase_type rbase(pixf);

                RGBQUAD colour = FIA_RGBQUAD ((unsigned char) value, (unsigned char) value, (unsigned char) value);

                return DrawEllipse (rbase, src, rect, agg::rgba8 (colour.rgbRed, colour.rgbGreen, colour.rgbBlue), 0, 0.1, antialiased);
            }

            break;
        }
        case FIT_UINT16:
        {      
            typedef agg::pixfmt_gray16                       pixfmt_type;
            typedef agg::renderer_base < pixfmt_type >       renbase_type;

            // Create the rendering buffer
            agg::rendering_buffer rbuf (buf, width, height, FreeImage_GetPitch (src));

            pixfmt_type pixf(rbuf);
            renbase_type rbase(pixf);

            return DrawEllipse (rbase, src, rect, agg::gray16(value), 0, 0.1, antialiased);

       }
        case FIT_INT16:
        {   
            break;
        }
        case FIT_UINT32:
        {             
            break;
        }
        case FIT_INT32:
        {
            break;
        }
        case FIT_FLOAT:
        {
            break;
        }
        case FIT_DOUBLE:
        { 
            break;
        }
        default:
        {
            break;
        }
    }

    return FIA_ERROR;
}


template<typename RendererType, typename ColorT>
static int
DrawPolygon (RendererType& renderer, FIBITMAP * src, FIAPOINT * points, int number_of_points, const ColorT& colour, int solid, int line_width, int antialiased)
{
    // the Rasterizer definition
    agg::rasterizer_scanline_aa<> ras;
    ras.reset();

    agg::scanline_p8 scanline;

    agg::path_storage ps;

    ps.move_to (points[0].x, points[0].y);

    for(int i = 1; i < number_of_points; i++)
    {
        ps.line_to (points[i].x+0.5, points[i].y+0.5);
    }

    if(solid) {
        ras.add_path(ps, 0);
    }
    else {      
        agg::conv_stroke<agg::path_storage> poly(ps);
        poly.width(line_width);
        ras.add_path(poly, 0);
    }

    if(antialiased)
        agg::render_scanlines_aa_solid(ras, scanline, renderer, colour);
    else
        agg::render_scanlines_bin_solid(ras, scanline, renderer, colour);

    return FIA_SUCCESS;
}


int DLL_CALLCONV
FIA_DrawSolidGreyscalePolygon (FIBITMAP * src, FIAPOINT * points,
                          int number_of_points, unsigned char value, int antialiased)
{
    int width = FreeImage_GetWidth (src);
    int height = FreeImage_GetHeight (src);

    FREE_IMAGE_TYPE type = FreeImage_GetImageType (src);

    // Allocate the framebuffer
    unsigned char *buf = FreeImage_GetBits (src);

    switch (type)
    {
        case FIT_BITMAP:
        {                       // standard image: 1-, 4-, 8-, 16-, 24-, 32-bit
            if (FreeImage_GetBPP (src) == 8) {
                
                typedef agg::pixfmt_gray8                        pixfmt_type;
                typedef agg::renderer_base < pixfmt_type >       renbase_type;

                // Create the rendering buffer
                agg::rendering_buffer rbuf (buf, width, height, FreeImage_GetPitch (src));

                pixfmt_type pixf(rbuf);
                renbase_type rbase(pixf);

                RGBQUAD colour = FIA_RGBQUAD ((unsigned char) value, (unsigned char) value, (unsigned char) value);

                return DrawPolygon (rbase, src, points, number_of_points,
                            agg::rgba8 (colour.rgbRed, colour.rgbGreen, colour.rgbBlue), 1, 0, antialiased);
            }
        }
        default:
        {
            break;
        }
    }

    return FIA_ERROR;
}

int DLL_CALLCONV
FIA_DrawColourSolidPolygon (FIBITMAP * src, FIAPOINT * points,
                          int number_of_points, RGBQUAD colour, int antialiased)
{
    int width = FreeImage_GetWidth (src);
    int height = FreeImage_GetHeight (src);

    FREE_IMAGE_TYPE type = FreeImage_GetImageType (src);
    int bpp = FreeImage_GetBPP(src);

    unsigned char *buf = FreeImage_GetBits (src);

    // Create the rendering buffer
    agg::rendering_buffer rbuf (buf, width, height, FreeImage_GetPitch (src));

    if (type == FIT_BITMAP && bpp == 32)
    {
        typedef agg::pixfmt_bgra32                       pixfmt_type;
        typedef agg::renderer_base < pixfmt_type >       renbase_type;
     
        pixfmt_type pixf(rbuf);
        renbase_type rbase(pixf);


        return DrawPolygon (rbase, src, points, number_of_points,
                            agg::rgba8 (colour.rgbRed, colour.rgbGreen, colour.rgbBlue), 1, 0, antialiased);
    }
    else if (type == FIT_BITMAP && bpp == 24)
    {
        typedef agg::pixfmt_bgr24                        pixfmt_type;
        typedef agg::renderer_base < pixfmt_type >       renbase_type;

        pixfmt_type pixf(rbuf);
        renbase_type rbase(pixf);



        return DrawPolygon (rbase, src, points, number_of_points,
                            agg::rgba8 (colour.rgbRed, colour.rgbGreen, colour.rgbBlue), 1, 0, antialiased);
    }

    return FIA_ERROR;
}

// Draws a orthogonal no aa line of width one pixel
// This is for drawing rectangles fast without subpixel position as with agg.
template<typename ValueType>
static int
orthogonal_draw_line (FIBITMAP * src, int x1, int y1, int x2, int y2, ValueType value, RGBQUAD colour)
{
    if (!src)
    {
        return FIA_ERROR;
    }

    int width = FreeImage_GetWidth (src);
    int height = FreeImage_GetHeight (src);

    // Draw from the left
    if (x2 < x1)
    {
        SWAP (x1, x2);
    }

    // Draw from the top
    if (y2 < y1)
    {
        SWAP (y1, y2);
    }

    if (x2 < 0 || y2 < 0)
    {
        return FIA_ERROR;
    }

    if (x1 < 0)
    {
        x1 = 0;
    }

    if (x2 >= width)
    {
        x2 = width - 1;
    }

    if (y1 < 0)
    {
        y1 = 0;
    }

    if (y2 >= height)
    {
        y2 = height - 1;
    }

    int bytespp = FreeImage_GetLine (src) / FreeImage_GetWidth (src);
    int pitch = FreeImage_GetPitch (src);

    bool greyscale_image = true;

    if(FreeImage_GetImageType(src) == FIT_BITMAP && FreeImage_GetBPP(src) > 8) {
        greyscale_image = false;
    }

    if (x1 != x2)
    {
        // We have a horizontal line
        // Make sure y's are the same
        if (y1 != y2)
        {
            return FIA_ERROR;
        }

        BYTE *bits = (BYTE *) FreeImage_GetScanLine (src, y1) + (x1 * bytespp);

        if(greyscale_image)
        {
                while (x1 <= x2)
                {
                    *(ValueType*)bits = value;  
                    // jump to next pixel
                    bits += bytespp;
                    x1++;
                }
        }
        else {

            for(register int x = x1; x <= x2; x++)
            {
                bits[FI_RGBA_RED] = colour.rgbRed;
                bits[FI_RGBA_GREEN] = colour.rgbGreen;
                bits[FI_RGBA_BLUE] = colour.rgbBlue;

                if (bytespp == 4)
                {
                    bits[FI_RGBA_ALPHA] = 0;
                }

                // jump to next pixel
                bits += bytespp;
            }
        }
    
        return FIA_SUCCESS;
    }

    if (y1 != y2)
    {
        // We have a verticle line
        // Make sure x's are the same
        if (x1 != x2)
        {
            return FIA_ERROR;
        }

        // Get starting point
        BYTE *bits = (BYTE *) (FreeImage_GetScanLine (src, y1) + (x1 * bytespp));

        if(greyscale_image)
        {
            while (y1 <= y2)
            {
                *(ValueType*)bits = value;

                bits += pitch;

                y1++;
            }
        }
        else {

            while (y1 <= y2)
            {
                bits[FI_RGBA_RED] = colour.rgbRed;
                bits[FI_RGBA_GREEN] = colour.rgbGreen;
                bits[FI_RGBA_BLUE] = colour.rgbBlue;

                if (bytespp == 4)
                {
                    bits[FI_RGBA_ALPHA] = 0;
                }

                bits += pitch;

                y1++;
            }
        }

        return FIA_SUCCESS;
    }

    return FIA_ERROR;
}

template<typename ValueType>
static int
DrawRectangle (FIBITMAP * src, FIARECT rect, ValueType value, RGBQUAD color_value, int line_width)
{
    int err = FIA_ERROR;

    for(int i = 0; i < line_width; i++)
    {
        // Top
        err = orthogonal_draw_line (src, rect.left - line_width + 1, rect.top + i, rect.right
                                           + line_width - 1, rect.top + i, value, color_value);

        // Bottom
        err = orthogonal_draw_line (src, rect.left - line_width + 1, rect.bottom - i,
                                           rect.right + line_width - 1, rect.bottom - i, value, color_value);

        // Left
        err = orthogonal_draw_line (src, rect.left - i, rect.top, rect.left - i, rect.bottom,
                                           value, color_value);

        // Right
        err = orthogonal_draw_line (src, rect.right + i, rect.top, rect.right + i,
                                           rect.bottom, value, color_value);
    }

    if (err == FIA_ERROR)
    {
        return FIA_ERROR;
    }

    return FIA_SUCCESS;
}

template<typename ValueType>
static int
DrawSolidRectangle (FIBITMAP *src, FIARECT rect, ValueType value, RGBQUAD colour)
{
    // Seems that Anti grain method is to slow probably  because it is too advanced
    // Does accurate drawing etc with anti aliasing.
    // We just want a simple rectangle with no antialising or sub pixel position.
    // Would we ever want that for rectangles ?
  
    int width = FreeImage_GetWidth (src);
    int height = FreeImage_GetHeight (src);

    if (rect.left < 0)
    {
        rect.left = 0;
        rect.right += rect.left;
    }

    if (rect.top < 0)
    {
        rect.top = 0;
        rect.bottom += rect.top;
    }

    if (rect.right >= width)
    {
        rect.right = width - 1;
    }

    if (rect.bottom >= height)
    {
        rect.bottom = height - 1;
    }

    // Allocate the framebuffer
    FIARECT tmp_rect = rect;

    // FreeImages are flipped
    tmp_rect.top = height - rect.top - 1;
    tmp_rect.bottom = height - rect.bottom - 1;

    int right = tmp_rect.left + (tmp_rect.right - tmp_rect.left);

    bool greyscale_image = true;

    if(FreeImage_GetImageType(src) == FIT_BITMAP && FreeImage_GetBPP(src) > 8) {
        greyscale_image = false;
    }
   
    if(greyscale_image)
    {
        // Allocate the framebuffer
        ValueType *buf = NULL;
        
        for(register int y = tmp_rect.bottom; y <= tmp_rect.top; y++)
        {
            buf = (ValueType*) FreeImage_GetScanLine (src, y);

            for(register int x = tmp_rect.left; x <= right; x++)
                buf[x] = value;
        }
    }
    else {

        BYTE *buf = NULL;

        int bytespp = FreeImage_GetLine (src) / FreeImage_GetWidth (src);
        int pitch = FreeImage_GetPitch (src);

        for(register int y = tmp_rect.bottom; y <= tmp_rect.top; y++)
        {
            buf = (BYTE*) FreeImage_GetScanLine (src, y);
            buf += (tmp_rect.left * bytespp);

            for(register int x = tmp_rect.left; x <= right; x++) {
        
                buf[FI_RGBA_RED] = colour.rgbRed;
                buf[FI_RGBA_GREEN] = colour.rgbGreen;
                buf[FI_RGBA_BLUE] = colour.rgbBlue;

                if (bytespp == 4)
                {
                    buf[x + FI_RGBA_ALPHA] = 0;
                }

                buf += bytespp;

            }
        }

    }

    return FIA_SUCCESS;
}


int DLL_CALLCONV
FIA_DrawSolidGreyscaleRect (FIBITMAP * src, FIARECT rect, double value)
{
    FREE_IMAGE_TYPE type = FreeImage_GetImageType (src);

    RGBQUAD colour = FIA_RGBQUAD (0, 0, 0);

    switch (type)
    {
        case FIT_BITMAP:
        {                       // standard image: 1-, 4-, 8-, 16-, 24-, 32-bit
            if (FreeImage_GetBPP (src) == 8)
                return DrawSolidRectangle (src, rect, (unsigned char) value, colour);
            break;
        }
        case FIT_UINT16:
        {                       // array of unsigned short: unsigned 16-bit
            return DrawSolidRectangle (src, rect, (unsigned short) value, colour);
            break;
        }
        case FIT_INT16:
        {                       // array of short: signed 16-bit
            return DrawSolidRectangle (src, rect, (short) value, colour);
            break;
        }
        case FIT_UINT32:
        {                       // array of unsigned long: unsigned 32-bit
            return DrawSolidRectangle (src, rect, (unsigned long) value, colour);
            break;
        }
        case FIT_INT32:
        {                       // array of long: signed 32-bit
            return DrawSolidRectangle (src, rect, (long) value, colour);
            break;
        }
        case FIT_FLOAT:
        {                       // array of float: 32-bit
            return DrawSolidRectangle (src, rect, (float) value, colour);
            break;
        }
        case FIT_DOUBLE:
        {                       // array of double: 64-bit
            return DrawSolidRectangle (src, rect, (double) value, colour);
            break;
        }
        default:
        {
            break;
        }
    }

    return FIA_ERROR;
}

int DLL_CALLCONV
FIA_DrawColourSolidRect (FIBITMAP * src, FIARECT rect, RGBQUAD colour)
{
    return DrawSolidRectangle (src, rect, 0, colour);
}

int DLL_CALLCONV
FIA_DrawColourLine (FIBITMAP * src, FIAPOINT p1, FIAPOINT p2, RGBQUAD colour,
                    int line_width, int antialiased)
{
    int width = FreeImage_GetWidth (src);
    int height = FreeImage_GetHeight (src);

    FIAPOINT p1_tmp = p1, p2_tmp = p2;

    // FreeImages are flipped
    p1_tmp.y = height - p1.y;
    p2_tmp.y = height - p2.y;

    int bpp = FreeImage_GetBPP (src);
    FREE_IMAGE_TYPE type = FreeImage_GetImageType (src);

    // Allocate the framebuffer
    unsigned char *buf = FreeImage_GetBits (src);

    // Create the rendering buffer
    agg::rendering_buffer rbuf (buf, width, height, FreeImage_GetPitch (src));

    if (type == FIT_BITMAP && bpp == 32)
    {
        typedef agg::pixfmt_bgra32                       pixfmt_type;
        typedef agg::renderer_base < pixfmt_type >       renbase_type;

        pixfmt_type pixf(rbuf);
        renbase_type rbase(pixf);

        return DrawLine (rbase, src, p1_tmp, p2_tmp, colour, line_width, antialiased);
    }

    if (type == FIT_BITMAP && bpp == 24)
    {
    typedef agg::pixfmt_bgr24                        pixfmt_type;
        typedef agg::renderer_base < pixfmt_type >       renbase_type;

        // Create the rendering buffer
        agg::rendering_buffer rbuf (buf, width, height, FreeImage_GetPitch (src));

        pixfmt_type pixf(rbuf);
        renbase_type rbase(pixf);

        return DrawLine (rbase, src, p1_tmp, p2_tmp, colour, line_width, antialiased);
    }

    return FIA_ERROR;
}

int DLL_CALLCONV
FIA_DrawGreyscaleLine (FIBITMAP * src, FIAPOINT p1, FIAPOINT p2, double value,
                       int line_width, int antialiased)
{
    int width = FreeImage_GetWidth(src);
    int height = FreeImage_GetHeight (src);

    FIAPOINT p1_tmp = p1, p2_tmp = p2;

    // FreeImages are flipped
    p1_tmp.y = height - p1.y;
    p2_tmp.y = height - p2.y;

    int bpp = FreeImage_GetBPP (src);
    FREE_IMAGE_TYPE type = FreeImage_GetImageType (src);

    // Allocate the framebuffer
    unsigned char *buf = FreeImage_GetBits (src);

    // Create the rendering buffer
    agg::rendering_buffer rbuf (buf, width, height, FreeImage_GetPitch (src));

    if (type == FIT_BITMAP && bpp == 8)
    {
        typedef agg::pixfmt_gray8                        pixfmt_type;
        typedef agg::renderer_base < pixfmt_type >       renbase_type;

        // Create the rendering buffer
        agg::rendering_buffer rbuf (buf, width, height, FreeImage_GetPitch (src));

        pixfmt_type pixf(rbuf);
        renbase_type rbase(pixf);

        RGBQUAD colour;

        colour.rgbRed = (unsigned char) value;
        colour.rgbGreen = (unsigned char) value;
        colour.rgbBlue = (unsigned char) value;

        return DrawLine (rbase, src, p1_tmp, p2_tmp, colour, line_width, antialiased);
    }

    return FIA_ERROR;
}

int DLL_CALLCONV
FIA_DrawOnePixelIndexLine (FIBITMAP * src, FIAPOINT p1, FIAPOINT p2, BYTE value)
{
    int swapped = 0, dx, dy, abs_dy, incrN, incrE, incrNE, d, x, y, slope, tmp_y, len = 0;
	BYTE *bits = NULL;

	if (src==NULL)
		return -1;
		
    // If necessary, switch the points so we're 
    // always drawing from left to right. 
    if (p2.x < p1.x)
    {
        swapped = 1;
        SWAP (p1, p2);
    }

    dx = p2.x - p1.x;
    dy = p2.y - p1.y;
    abs_dy = abs(dy);

    if (dy < 0)
    {
        slope = -1;
        dy = -dy;
    }
    else
    {
        slope = 1;
    }

    x = p1.x;
    y = p1.y;

    if (abs_dy <= dx)
    {
        d = 2 * dy - dx;
        incrE = 2 * dy;         // E
        incrNE = 2 * (dy - dx); // NE

        bits = (BYTE*) FreeImage_GetScanLine (src, y);

        bits[x] = value;

        while (x <= p2.x)
        {
            if (d <= 0)
            {
                d += incrE;     // Going to E
                x++;
            }
            else
            {
                d += incrNE;    // Going to NE
                x++;
                y += slope;
            }

            bits = (BYTE*) FreeImage_GetScanLine (src, y);

            bits[x] = value;
            len++;
        }
    }
    else
    {
        len = 1;
        d = dy - 2 * dx;
        incrN = 2 * -dx;        // N
        incrNE = 2 * (dy - dx); // NE

        tmp_y = 0;

        bits = (BYTE*) FreeImage_GetScanLine (src, y);

        bits[x] = value;

        for(tmp_y = 0; tmp_y < abs_dy; tmp_y++)
        {
            if (d > 0)
            {

                d += incrN;     // Going to N
                y += slope;
            }
            else
            {
                d += incrNE;    // Going to NE
                y += slope;
                x++;
            }

            bits = (BYTE*) FreeImage_GetScanLine (src, y);

            bits[x] = value;

            len++;
        }
    }

    return len;
}

int DLL_CALLCONV
FIA_DrawOnePixelIndexLineFromTopLeft (FIBITMAP * src, FIAPOINT p1, FIAPOINT p2, BYTE value)
{
	int height = FreeImage_GetHeight(src);

	p1.y = height - p1.y - 1;
	p2.y = height - p2.y - 1;

	return FIA_DrawOnePixelIndexLine (src, p1, p2, value);
}

int DLL_CALLCONV
FIA_DrawColourRect (FIBITMAP * src, FIARECT rect, RGBQUAD colour, int line_width)
{
    int height = FreeImage_GetHeight (src);

    FIARECT tmp_rect = rect;

    // FreeImages are flipped
    tmp_rect.top = height - rect.top - 1;
    tmp_rect.bottom = height - rect.bottom - 1;

    return DrawRectangle (src, tmp_rect, 0, colour, line_width);
}


int DLL_CALLCONV
FIA_DrawGreyscaleRect (FIBITMAP * src, FIARECT rect, double value, int line_width)
{
    int height = FreeImage_GetHeight (src);

    FIARECT tmp_rect = rect;

    // FreeImages are flipped
    tmp_rect.top = height - rect.top - 1;
    tmp_rect.bottom = height - rect.bottom - 1;

    FREE_IMAGE_TYPE type = FreeImage_GetImageType (src);

    RGBQUAD colour = FIA_RGBQUAD (0, 0, 0);

    switch (type)
    {
        case FIT_BITMAP:
        {                       // standard image: 1-, 4-, 8-, 16-, 24-, 32-bit
            if (FreeImage_GetBPP (src) == 8)
                return DrawRectangle (src, tmp_rect, (unsigned char) value, colour, line_width);
            break;
        }
        case FIT_UINT16:
        {                       // array of unsigned short: unsigned 16-bit
            return DrawRectangle (src, tmp_rect, (unsigned short) value, colour, line_width);
            break;
        }
        case FIT_INT16:
        {                       // array of short: signed 16-bit
            return DrawRectangle (src, tmp_rect, (short) value, colour, line_width);
            break;
        }
        case FIT_UINT32:
        {                       // array of unsigned long: unsigned 32-bit
            return DrawRectangle (src, tmp_rect, (unsigned long) value, colour, line_width);
            break;
        }
        case FIT_INT32:
        {                       // array of long: signed 32-bit
            return DrawRectangle (src, tmp_rect, (long) value, colour, line_width);
            break;
        }
        case FIT_FLOAT:
        {                       // array of float: 32-bit
            return DrawRectangle (src, tmp_rect, (float) value, colour, line_width);
            break;
        }
        case FIT_DOUBLE:
        {                       // array of double: 64-bit
            return DrawRectangle (src, tmp_rect, (double) value, colour, line_width);
            break;
        }
        default:
        {
            break;
        }
    }

    return FIA_ERROR;
}

int DLL_CALLCONV
FIA_DrawGreyScaleCheckerBoard (FIBITMAP * src, int square_size)
{
    int row;   // Row number, from 0 to cols
    int col;   // Column number, from 0 to rows
    int x, y;   // Top-left corner of square
    double value;

    // Minimum grid size
    if(square_size < 5)
        square_size = 5;

    // Sret Maximum grid size
    if(square_size > 500)
        square_size = 500;

    int width =  FreeImage_GetWidth(src);
    int height = FreeImage_GetHeight(src);

    int cols = width / square_size;
    int rows = height / square_size;

    for ( row = 0;  row < rows + 1;  row++ ) {

         for ( col = 0;  col < cols + 1;  col++) {

            x = col * square_size;
            y = row * square_size;

            if ( (row % 2) == (col % 2) )
               value = 255.0;
            else
               value = 0.0;

            if(FIA_DrawSolidGreyscaleRect (src, MakeFIARect(x, y, x + square_size, y + square_size), value) == FIA_ERROR)
                return FIA_ERROR;
         }
      }

    return FIA_SUCCESS;
}

/*


 typedef agg::font_engine_win32_tt_int32 font_engine_type;
    typedef agg::font_cache_manager<font_engine_type> font_manager_type;


template<class Rasterizer, class Scanline, class RenSolid, class RenBin>
static unsigned DrawTTF(Rasterizer& ras, Scanline& sl, RenSolid& ren_solid, RenBin& ren_bin, int height, const char *text)
{
	typedef agg::conv_curve<font_manager_type::path_adaptor_type> conv_curve_type;
    typedef agg::conv_contour<conv_curve_type> conv_contour_type;

	font_engine_type             m_feng;
	font_manager_type            m_fman;

    conv_curve_type m_curves;
    conv_contour_type m_contour;

    agg::glyph_rendering gren = agg::glyph_ren_native_mono;
    unsigned num_glyphs = 0;
	int width, weight;

    if(height < 8)
        height = 8;

    if(height > 32)
        height = 32;

    width = height;
    weight = 0.5;

    m_contour.width(-weight * height * 0.05);

    m_feng.hinting(1);
    m_feng.height(height);

    // Font width in Windows is strange. MSDN says, 
    // "specifies the average width", but there's no clue what
    // this "average width" means. It'd be logical to specify 
    // the width with regard to the font height, like it's done in 
    // FreeType. That is, width == height should mean the "natural", 
    // not distorted glyphs. In Windows you have to specify
    // the absolute width, which is very stupid and hard to use 
    // in practice.
    //-------------------------
    m_feng.width((width == height) ? 0.0 : width / 2.4);
    //m_feng.italic(true);
    m_feng.flip_y(true);

    agg::trans_affine mtx;
    //mtx *= agg::trans_affine_skewing(-0.3, 0);
    mtx *= agg::trans_affine_rotation(agg::deg2rad(-4.0));
    m_feng.transform(mtx);

    if(m_feng.create_font("Arial", gren))
    {
        m_fman.precache(' ', 127);

        double x = 10.0;
        double y0 = 10.0;
        double y = y0;
        const char* p = text;

        while(*p)
        {
            const agg::glyph_cache* glyph = m_fman.glyph(*p);
            if(glyph)
            {
               
                    m_fman.add_kerning(&x, &y);
                

                if(x >= width - height)
                {
                    x = 10.0;
                    y0 -= height;
                    if(y0 <= 120) break;
                    y = y0;
                }

                m_fman.init_embedded_adaptors(glyph, x, y);

                switch(glyph->data_type)
                {
                case agg::glyph_data_mono:
                    ren_bin.color(agg::rgba8(0, 0, 0));
                    agg::render_scanlines(m_fman.mono_adaptor(), 
                                            m_fman.mono_scanline(), 
                                            ren_bin);
                    break;

                case agg::glyph_data_gray8:
                    ren_solid.color(agg::rgba8(0, 0, 0));
                    agg::render_scanlines(m_fman.gray8_adaptor(), 
                                            m_fman.gray8_scanline(), 
                                            ren_solid);
                    break;

                case agg::glyph_data_outline:
                    ras.reset();
                    if(((double)fabs(weight)) <= 0.01)
                    {
                        // For the sake of efficiency skip the
                        // contour converter if the weight is about zero.
                        //-----------------------
                        ras.add_path(m_curves);
                    }
                    else
                    {
                        ras.add_path(m_contour);
                    }
                    ren_solid.color(agg::rgba8(0, 0, 0));
                    agg::render_scanlines(ras, sl, ren_solid);
                    break;
                }

                // increment pen position
                x += glyph->advance_x;
                y += glyph->advance_y;
                ++num_glyphs;
            }
            ++p;
        }
    }
    return num_glyphs;
}


int DLL_CALLCONV
FIA_DrawColourText (FIBITMAP *src, int left, int top, const char *text, RGBQUAD colour)
{
    typedef agg::gamma_lut<agg::int8u, agg::int16u, 8, 16> gamma_type;
   // typedef agg::pixfmt_bgr24_gamma<gamma_type> pixfmt_type;
	typedef agg::pixfmt_bgr24                        pixfmt_type;
        typedef agg::renderer_base < pixfmt_type >       renbase_type;
    //typedef agg::renderer_base<pixfmt_type> base_ren_type;
    typedef agg::renderer_scanline_aa_solid<renbase_type> renderer_solid;
    typedef agg::renderer_scanline_bin_solid<renbase_type> renderer_bin;
   
	


    int width = FreeImage_GetWidth (src);
    int height = FreeImage_GetHeight (src);
    unsigned char *buf = FreeImage_GetBits (src);

    //pixfmt_type pf(rbuf_window(), m_gamma_lut);
    //base_ren_type ren_base(pf);

    agg::rendering_buffer rbuf (buf, width, height, FreeImage_GetPitch (src));

    pixfmt_type pixf(rbuf);
    renbase_type rbase(pixf);

    

    renderer_solid ren_solid(rbase);
    renderer_bin ren_bin(rbase);
    rbase.clear(agg::rgba(1,1,1));

    agg::scanline_u8 sl;
    agg::rasterizer_scanline_aa<> ras;

    DrawTTF(ras, sl, ren_solid, ren_bin, 12, text);
    

    ras.gamma(agg::gamma_power(1.0));

    return FIA_SUCCESS;
}
*/

static const agg::int8u* AggEmbeddedFonts[] =
{
	agg::gse4x6,
    agg::gse4x8,
    agg::gse5x7,
    agg::gse5x9,
    agg::gse6x12,
    agg::gse6x9,
    agg::gse7x11,
    agg::gse7x11_bold,
    agg::gse7x15,
    agg::gse7x15_bold,
    agg::gse8x16,
    agg::gse8x16_bold,
    agg::mcs11_prop,
    agg::mcs11_prop_condensed,
    agg::mcs12_prop,
    agg::mcs13_prop,
    agg::mcs5x10_mono,
    agg::mcs5x11_mono,
    agg::mcs6x10_mono,
    agg::mcs6x11_mono,
    agg::mcs7x12_mono_high,
    agg::mcs7x12_mono_low,
	agg::verdana12,
    agg::verdana12_bold,
    agg::verdana13,
    agg::verdana13_bold,
    agg::verdana14,
    agg::verdana14_bold,
    agg::verdana16,
    agg::verdana16_bold,
    agg::verdana17,
    agg::verdana17_bold,
    agg::verdana18,
    agg::verdana18_bold
};


int DLL_CALLCONV
FIA_DrawHorizontalColourText (FIBITMAP *src, int left, int top, FIA_AggEmbeddedFont font, const char *text, RGBQUAD colour)
{
    typedef agg::pixfmt_bgr24 pixfmt;
    typedef agg::renderer_base<pixfmt> ren_base;
    typedef agg::glyph_raster_bin<agg::rgba8> glyph_gen;

    int width = FreeImage_GetWidth (src);
    int height = FreeImage_GetHeight (src);
    unsigned char *buf = FreeImage_GetBits (src);

    // Create the rendering buffer
    agg::rendering_buffer rbuf (buf, width, height, FreeImage_GetPitch (src));

    glyph_gen glyph(0);
    pixfmt pixf(rbuf);
    ren_base rbase(pixf);

    agg::renderer_raster_htext_solid<ren_base, glyph_gen> renderer(rbase, glyph);
  
    renderer.color(agg::rgba8(colour.rgbRed, colour.rgbGreen, colour.rgbBlue));

    glyph.font(AggEmbeddedFonts[font]);

	top = height - top - 1;

    renderer.render_text(left, top, text, false);
    
    return FIA_SUCCESS;
}

int DLL_CALLCONV
FIA_DrawHorizontalGreyscaleText (FIBITMAP * src, int left, int top, FIA_AggEmbeddedFont font, const char *text, unsigned char value)
{
    FREE_IMAGE_TYPE type = FreeImage_GetImageType (src);

    switch (type)
    {
        case FIT_BITMAP:
        {                       // standard image: 1-, 4-, 8-, 16-, 24-, 32-bit
            if (FreeImage_GetBPP (src) == 8) {
                
                typedef agg::pixfmt_gray8 pixfmt;
                typedef agg::renderer_base<pixfmt> ren_base;
                typedef agg::glyph_raster_bin<agg::rgba8> glyph_gen;

                int width = FreeImage_GetWidth (src);
                int height = FreeImage_GetHeight (src);
                unsigned char *buf = FreeImage_GetBits (src);

                // Create the rendering buffer
                agg::rendering_buffer rbuf (buf, width, height, FreeImage_GetPitch (src));

                glyph_gen glyph(0);
                pixfmt pixf(rbuf);
                ren_base rbase(pixf);

                agg::renderer_raster_htext_solid<ren_base, glyph_gen> renderer(rbase, glyph);
              
                renderer.color(agg::rgba8(value, value,value));

                glyph.font(AggEmbeddedFonts[font]);

				top = height - top - 1;

                renderer.render_text(left, top, text, false);
            }

            break;
        }
        case FIT_UINT16:
        {      
           break;
        }
        case FIT_INT16:
        {   
            break;
        }
        case FIT_UINT32:
        {             
            break;
        }
        case FIT_INT32:
        {
            break;
        }
        case FIT_FLOAT:
        {
            break;
        }
        case FIT_DOUBLE:
        { 
            break;
        }
        default:
        {
            break;
        }
    }

    return FIA_ERROR;
}
