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

#ifndef __FREEIMAGE_ALGORITHMS_FIND_MAXIMA__
#define __FREEIMAGE_ALGORITHMS_FIND_MAXIMA__

#include "FreeImageAlgorithms.h"

#ifdef __cplusplus
extern "C" {
#endif

/*! \file 
*	Provides methods when working with images of particles.
*/ 

/** Data structure for describing a peak in an image.
*/
typedef struct
{
  FIAPOINT centre;
  double value;

} FIAPeak;

/** Data structure for describing a blob in an image.
*/
typedef struct
{
	FIARECT rect;
	int area;
	int center_x;
	int center_y;

} BLOBINFO;

/** Data structure for describing all blobs in an image.
*/
typedef struct
{
	int number_of_blobs;
	BLOBINFO* blobs;

} PARTICLEINFO;


DLL_API void DLL_CALLCONV
FIA_EnableOldBrokenCodeCompatibility(void);


/** \brief Find information about particles or blobs in an image.
 *
 *  \param src FIBITMAP Image with blobs must be a binary 8bit image.
 *  \param info PARTICLEINFO** Address of pointer to hold particle information the pointer should be NULL.
 *  \param white_on_black unsigned char Determines the background intensity value.
 *  \return int FIA_SUCCESS on success or FIA_ERROR on error.
*/
DLL_API int DLL_CALLCONV
FIA_ParticleInfo(FIBITMAP* src, PARTICLEINFO** info, unsigned char white_on_black);


/** \brief Frees the data returned by FIA_ParticleInfo.
 *
 *  \param info PARTICLEINFO* pointer to particle information.
 *  \return int FIA_SUCCESS on success or FIA_ERROR on error.
*/
DLL_API void DLL_CALLCONV
FIA_FreeParticleInfo(PARTICLEINFO* info);


/** \brief Fills the hole in a particle or blob image.
 *
 *  Image data is an 8bit binary image.
 *
 *  \param src FIBITMAP Image with blobs must be a binary 8bit image.
 *  \param white_on_black unsigned char Determines the background intensity value.
 *  \return FIBITMAP on success or NULL on error.
*/
DLL_API FIBITMAP* DLL_CALLCONV
FIA_Fillholes(FIBITMAP* src,
							 unsigned char white_on_black);


/** \brief Finds the maxima within particles or blobs.
 *
 *  The code finds maxima in particles or blobs. Plateaux are cound as are peaks but
 *  shoulders are not. Shoulder are lower intensity regions attached to a peak.
 *
 *  Image data is greyscale data.
 *
 *  \param src FIBITMAP Greyscale image of particles.
 *  \param mask FIBITMAP Mask to first apply to the image.
 *  \param threshold unsigned char Threshold value to apply initially to image.
 *  \param min_separation int The mininum separation between particles.
 *  \param peaks FIAPeak ** The address of the pointer to hold returned peak positions.
 *  \param number int Number of peaks to find. If 0 then all peaks are returned.
 *  \param peaks_found int* Number of peaks returned.
 *  \return FIBITMAP on success or NULL on error.
*/
DLL_API FIBITMAP* DLL_CALLCONV
FIA_FindImageMaxima(FIBITMAP* src, FIBITMAP *mask,
                                    double threshold,
						            int min_separation, int oval_draw,
									FIAPeak **peaks,
                                    int number, int *peaks_found);

DLL_API void DLL_CALLCONV
FIA_FreePeaks(FIAPeak *peaks);

DLL_API int DLL_CALLCONV
FIA_ATrousWaveletTransform(FIBITMAP* src, int levels, FIBITMAP** W);

DLL_API FIBITMAP* DLL_CALLCONV
FIA_MultiscaleProducts(FIBITMAP* src, int start_level, int levels);

#ifdef __cplusplus
}
#endif

#endif
