/*
This file is part of Jedi Academy.

    Jedi Academy is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Jedi Academy is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Jedi Academy.  If not, see <http://www.gnu.org/licenses/>.
*/
// Copyright 2001-2013 Raven Software

// tr_image.c

// leave this as first line for PCH reasons...
//
#include "../server/exe_headers.h"



#include "tr_local.h"
#ifndef _XBOX
#include "tr_jpeg_interface.h"
#else
#include "../qcommon/sstring.h"
#include "../zlib/zlib.h"
#endif
#include "../png/png.h"
#include "../qcommon/sstring.h"


static byte			 s_intensitytable[256];
static unsigned char s_gammatable[256];

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;

#define FILE_HASH_SIZE		1024	// actually, the shader code needs this (from another module, great).
//static	image_t*		hashTable[FILE_HASH_SIZE];

/*
** R_GammaCorrect
*/
void R_GammaCorrect( byte *buffer, int bufSize ) {
	int i;

	for ( i = 0; i < bufSize; i++ ) {
		buffer[i] = s_gammatable[buffer[i]];
	}
}

typedef struct {
	char *name;
	int	minimize, maximize;
} textureMode_t;

textureMode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
================
return a hash value for the filename
================
*/
long generateHashValue( const char *fname ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}

// makeup a nice clean, consistant name to query for and file under, for map<> usage...
//
char *GenerateImageMappingName( const char *name )
{
	static char sName[MAX_QPATH];
	int		i=0;
	char	letter;
	
	while (name[i] != '\0' && i<MAX_QPATH-1) 
	{
		letter = tolower(name[i]);
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		sName[i++] = letter;
	}
	sName[i]=0;
	
	return &sName[0];
}

/*
===============
GL_TextureMode
===============
*/
void GL_TextureMode( const char *string ) {
	int		i;
	image_t	*glt;

	for ( i=0 ; i< 6 ; i++ ) {
		if ( !Q_stricmp( modes[i].name, string ) ) {
			break;
		}
	}

	if ( i == 6 ) {
		VID_Printf (PRINT_ALL, "bad filter name\n");
		for ( i=0 ; i< 6 ; i++ ) {
			VID_Printf( PRINT_ALL, "%s\n",modes[i].name);
			}
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// If the level they requested is less than possible, set the max possible...
	if ( r_ext_texture_filter_anisotropic->value > glConfig.maxTextureFilterAnisotropy )
	{
		ri.Cvar_Set( "r_ext_texture_filter_anisotropic", va("%f",glConfig.maxTextureFilterAnisotropy) );
	}

	// change all the existing mipmap texture objects
//	int iNumImages = 
	   				 R_Images_StartIteration();
	while ( (glt   = R_Images_GetNextIteration()) != NULL)
	{
#ifdef _XBOX
		if ( glt->mipcount ) {
#else
		if ( glt->mipmap ) {
#endif
			GL_Bind (glt);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

			if(glConfig.maxTextureFilterAnisotropy>0) {
				if(r_ext_texture_filter_anisotropic->integer>1) {
					qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_ext_texture_filter_anisotropic->value);
				} else {
					qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
				}
			}
		}
	}
}

static float R_BytesPerTex (int format)
{
	switch ( format ) {
	case 1:
		//"I    " 
		return 1;
		break;
	case 2:
		//"IA   " 
		return 2;
		break;
	case 3:
		//"RGB  " 
		return glConfig.colorBits/8.0f;
		break;
	case 4:
		//"RGBA " 
		return glConfig.colorBits/8.0f;
		break;
		
	case GL_RGBA4:
		//"RGBA4" 
		return 2;
		break;
	case GL_RGB5:
		//"RGB5 " 
		return 2;
		break;
		
	case GL_RGBA8:
		//"RGBA8" 
		return 4;
		break;
	case GL_RGB8:
		//"RGB8" 
		return 4;
		break;
		
	case GL_RGB4_S3TC:
		//"S3TC " 
		return 0.33333f;
		break;
	case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		//"DXT1 " 
		return 0.33333f;
		break;
	case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
		//"DXT5 " 
		return 1;
		break;
#ifdef _XBOX
	case GL_DDS1_EXT:
		//"DDS1 "
		return 0.5f;
		break;
	case GL_DDS5_EXT:
		//"DDS5 "
		return 1;
		break;
	case GL_DDS_RGB16_EXT:
		//"DDS16"
		return 2;
		break;
	case GL_DDS_RGBA32_EXT:
		//"DDS32"
		return 4;
		break;
#endif
	default:
		//"???? " 
		return 4;
	}
}

/*
===============
R_SumOfUsedImages
===============
*/
#ifndef _XBOX
float R_SumOfUsedImages( qboolean bUseFormat ) 
{
	int	total = 0;
	image_t *pImage;

	   				  R_Images_StartIteration();
	while ( (pImage = R_Images_GetNextIteration()) != NULL)
	{
		if ( pImage->frameUsed == tr.frameCount- 1 ) {//it has already been advanced for the next frame, so...
			if (bUseFormat)
			{
				float  bytePerTex = R_BytesPerTex (pImage->internalFormat);
				total += bytePerTex * (pImage->width * pImage->height);
			}
			else
			{
				total += pImage->width * pImage->height;
			}
		}
	}

	return total;
}
#endif

/*
===============
R_ImageList_f
===============
*/
void R_ImageList_f( void ) {
	int		i=0;
	image_t	*image;
	int		texels = 0;
//	int		totalFileSizeK = 0;
	float	texBytes = 0.0f;
	const char *yesno[] = {"no ", "yes"};

	VID_Printf (PRINT_ALL, "\n      -w-- -h-- -fsK- -mm- -if- wrap --name-------\n");

	int iNumImages = R_Images_StartIteration();
	while ( (image = R_Images_GetNextIteration()) != NULL)
	{
		texels   += image->width*image->height;
		texBytes += image->width*image->height * R_BytesPerTex (image->internalFormat);
//		totalFileSizeK += (image->imgfileSize+1023)/1024;
#ifdef _XBOX
		VID_Printf (PRINT_ALL,  "%4i: %4i %4i  %s ",
			i, image->width, image->height, yesno[image->mipcount] );
#else
		//VID_Printf (PRINT_ALL,  "%4i: %4i %4i %5i  %s ",
		//	i, image->width, image->height,(image->fileSize+1023)/1024, yesno[image->mipmap] );
		VID_Printf (PRINT_ALL,  "%4i: %4i %4i  %s ",
			i, image->width, image->height,yesno[image->mipmap] );
#endif
		switch ( image->internalFormat ) {
		case 1:
			VID_Printf( PRINT_ALL, "I    " );
			break;
		case 2:
			VID_Printf( PRINT_ALL, "IA   " );
			break;
		case 3:
			VID_Printf( PRINT_ALL, "RGB  " );
			break;
		case 4:
			VID_Printf( PRINT_ALL, "RGBA " );
			break;
		case GL_RGBA8:
			VID_Printf( PRINT_ALL, "RGBA8" );
			break;
		case GL_RGB8:
			VID_Printf( PRINT_ALL, "RGB8 " );
			break;
		case GL_RGB4_S3TC:
			VID_Printf( PRINT_ALL, "S3TC " );
			break;
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
			VID_Printf( PRINT_ALL, "DXT1 " );
			break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
			VID_Printf( PRINT_ALL, "DXT5 " );
			break;
		case GL_RGBA4:
			VID_Printf( PRINT_ALL, "RGBA4" );
			break;
		case GL_RGB5:
			VID_Printf( PRINT_ALL, "RGB5 " );
			break;
#ifdef _XBOX
		case GL_DDS1_EXT:
			VID_Printf( PRINT_ALL, "DDS1 " );
			break;
		case GL_DDS5_EXT:
			VID_Printf( PRINT_ALL, "DDS5 " );
			break;
		case GL_DDS_RGB16_EXT:
			VID_Printf( PRINT_ALL, "DDS16" );
			break;
		case GL_DDS_RGBA32_EXT:
			VID_Printf( PRINT_ALL, "DDS32" );
			break;
#endif
		default:
			VID_Printf( PRINT_ALL, "???? " );
		}

		switch ( image->wrapClampMode ) {
		case GL_REPEAT:
			VID_Printf( PRINT_ALL, "rept " );
			break;
		case GL_CLAMP:
			VID_Printf( PRINT_ALL, "clmp " );
			break;
		case GL_CLAMP_TO_EDGE:
			VID_Printf( PRINT_ALL, "clpE " );
			break;
		default:
			VID_Printf( PRINT_ALL, "%4i ", image->wrapClampMode );
			break;
		}
		
#ifndef _XBOX
		VID_Printf( PRINT_ALL, "%s\n", image->imgName );
#endif
		i++;
	}
	VID_Printf (PRINT_ALL, " ---------\n");
	VID_Printf (PRINT_ALL, "      -w-- -h-- -mm- -if- wrap --name-------\n");
	VID_Printf (PRINT_ALL, " %i total texels (not including mipmaps)\n", texels );
//	VID_Printf (PRINT_ALL, " %iMB total filesize\n", (totalFileSizeK+1023)/1024 );
	VID_Printf (PRINT_ALL, " %.2fMB total texture mem (not including mipmaps)\n", texBytes/1048576.0f );
	VID_Printf (PRINT_ALL, " %i total images\n\n", iNumImages );
}

//=======================================================================


/*
================
R_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range
================
*/
static void R_LightScaleTexture (unsigned *in, int inwidth, int inheight, qboolean only_gamma )
{
	if ( only_gamma )
	{
		if ( !glConfig.deviceSupportsGamma )
		{
			int		i, c;
			byte	*p;

			p = (byte *)in;

			c = inwidth*inheight;
			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_gammatable[p[0]];
				p[1] = s_gammatable[p[1]];
				p[2] = s_gammatable[p[2]];
			}
		}
	}
	else
	{
		int		i, c;
		byte	*p;

		p = (byte *)in;

		c = inwidth*inheight;

		if ( glConfig.deviceSupportsGamma )
		{
			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_intensitytable[p[0]];
				p[1] = s_intensitytable[p[1]];
				p[2] = s_intensitytable[p[2]];
			}
		}
		else
		{
			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_gammatable[s_intensitytable[p[0]]];
				p[1] = s_gammatable[s_intensitytable[p[1]]];
				p[2] = s_gammatable[s_intensitytable[p[2]]];
			}
		}
	}
}


/*
================
R_MipMap2

Uses temp mem, but then copies back to input, quartering the size of the texture
Proper linear filter
================
*/
static void R_MipMap2( unsigned *in, int inWidth, int inHeight ) {
	int			i, j, k;
	byte		*outpix;
	int			inWidthMask, inHeightMask;
	int			total;
	int			outWidth, outHeight;
	unsigned	*temp;

	outWidth = inWidth >> 1;
	outHeight = inHeight >> 1;
	temp = (unsigned int *) Z_Malloc( outWidth * outHeight * 4, TAG_TEMP_WORKSPACE, qfalse );

	inWidthMask = inWidth - 1;
	inHeightMask = inHeight - 1;

	for ( i = 0 ; i < outHeight ; i++ ) {
		for ( j = 0 ; j < outWidth ; j++ ) {
			outpix = (byte *) ( temp + i * outWidth + j );
			for ( k = 0 ; k < 4 ; k++ ) {
				total = 
					1 * ((byte *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					1 * ((byte *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					2 * ((byte *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					4 * ((byte *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					4 * ((byte *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					2 * ((byte *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					4 * ((byte *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					4 * ((byte *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					1 * ((byte *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					1 * ((byte *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k];
				outpix[k] = total / 36;
			}
		}
	}

	memcpy( in, temp, outWidth * outHeight * 4 );
	Z_Free( temp );
}

/*
================
R_MipMap

Operates in place, quartering the size of the texture
================
*/
static void R_MipMap (byte *in, int width, int height) {
	int		i, j;
	byte	*out;
	int		row;

	if ( width == 1 && height == 1 ) {
		return;
	}

	if ( !r_simpleMipMaps->integer ) {
		R_MipMap2( (unsigned *)in, width, height );
		return;
	}

	row = width * 4;
	out = in;
	width >>= 1;
	height >>= 1;

	if ( width == 0 || height == 0 ) {
		width += height;	// get largest
		for (i=0 ; i<width ; i++, out+=4, in+=8 ) {
			out[0] = ( in[0] + in[4] )>>1;
			out[1] = ( in[1] + in[5] )>>1;
			out[2] = ( in[2] + in[6] )>>1;
			out[3] = ( in[3] + in[7] )>>1;
		}
		return;
	}

	for (i=0 ; i<height ; i++, in+=row) {
		for (j=0 ; j<width ; j++, out+=4, in+=8) {
			out[0] = (in[0] + in[4] + in[row+0] + in[row+4])>>2;
			out[1] = (in[1] + in[5] + in[row+1] + in[row+5])>>2;
			out[2] = (in[2] + in[6] + in[row+2] + in[row+6])>>2;
			out[3] = (in[3] + in[7] + in[row+3] + in[row+7])>>2;
		}
	}
}


/*
==================
R_BlendOverTexture

Apply a color blend over a set of pixels
==================
*/
static void R_BlendOverTexture( byte *data, int pixelCount, byte blend[4] ) {
	int		i;
	int		inverseAlpha;
	int		premult[3];

	inverseAlpha = 255 - blend[3];
	premult[0] = blend[0] * blend[3];
	premult[1] = blend[1] * blend[3];
	premult[2] = blend[2] * blend[3];

	for ( i = 0 ; i < pixelCount ; i++, data+=4 ) {
		data[0] = ( data[0] * inverseAlpha + premult[0] ) >> 9;
		data[1] = ( data[1] * inverseAlpha + premult[1] ) >> 9;
		data[2] = ( data[2] * inverseAlpha + premult[2] ) >> 9;
	}
}

byte	mipBlendColors[16][4] = {
	{0,0,0,0},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
};


/*
===============
Upload32

===============
*/
#ifdef _XBOX
static void Upload32( unsigned *data, 
						  int img_width, int img_height, 
						  GLenum format,
						  int mipcount, 
						  qboolean picmip, 
						  qboolean isLightmap, 
						  int *pformat )
{
	if (format == GL_RGBA)
	{
		int			samples;
		int			i, c;
		byte		*scan;
		int			width = img_width;
		int			height = img_height; 
		
		//
		// perform optional picmip operation
		//
		if ( picmip ) {
			for(i = 0; i < r_picmip->integer; i++) {
				R_MipMap( (byte *)data, width, height );
				width >>= 1;
				height >>= 1;
				if (width < 1) {
					width = 1;
				}
				if (height < 1) {
					height = 1;
				}
			}
		}
		
		//
		// clamp to the current upper OpenGL limit
		// scale both axis down equally so we don't have to
		// deal with a half mip resampling
		//
		while ( width > glConfig.maxTextureSize	|| height > glConfig.maxTextureSize ) {
			R_MipMap( (byte *)data, width, height );
			width >>= 1;
			height >>= 1;
		}
		
		//
		// scan the texture for each channel's max values
		// and verify if the alpha channel is being used or not
		//
		c = width*height;
		scan = ((byte *)data);
		samples = 3;
		for ( i = 0; i < c; i++ )
		{
			if ( scan[i*4 + 3] != 255 ) 
			{
				samples = 4;
				break;
			}
		}
		
		// select proper internal format
		if ( samples == 3 )
		{
			if ( isLightmap && r_texturebitslm->integer > 0 )
			{
				// Allow different bit depth when we are a lightmap
				if ( r_texturebitslm->integer == 16 )
				{
					*pformat = GL_RGB5;
				}
				else if ( r_texturebitslm->integer == 32 )
				{
					*pformat = GL_RGB8;
				}
			}
			else if ( r_texturebits->integer == 16 )
			{
				*pformat = GL_RGB5;
			}
			else if ( r_texturebits->integer == 32 )
			{
				*pformat = GL_RGB8;
			}
			else
			{
				*pformat = 3;
			}
		}
		else if ( samples == 4 )
		{
			if ( r_texturebits->integer == 16 )
			{
				*pformat = GL_RGBA4;
			}
			else if ( r_texturebits->integer == 32 )
			{
				*pformat = GL_RGBA8;
			}
			else
			{
				*pformat = 4;
			}
		}
		
		// copy or resample data as appropriate for first MIP level
		if (!mipcount)
		{
			qglTexImage2D (GL_TEXTURE_2D, 0, *pformat, width, height, 0, 
				GL_RGBA, GL_UNSIGNED_BYTE, data);
		}
		else
		{
			if (mipcount)
			{
				int	miplevel = 0;
				int total = 1;
				int n = width;
				if (height > n) n = height;
				while (n > 1)
				{
					n >>= 1;
					++total;
				}
				
				qglTexImage2DEXT (GL_TEXTURE_2D, 0, total, *pformat, width, height, 
					0, GL_RGBA, GL_UNSIGNED_BYTE, data );
			}
		}
	}
	else
	{
		*pformat = format;

		qglTexImage2DEXT (GL_TEXTURE_2D, 0, mipcount,
			format, img_width, img_height, 0, format, 
			GL_UNSIGNED_BYTE, data);
	}

	if (mipcount)
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		if(glConfig.textureFilterAnisotropicAvailable)
		{
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 2.0f);
		}
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	GL_CheckErrors();
}

#else // _XBOX

static void Upload32( unsigned *data, 
						  GLenum format,
						  qboolean mipmap, 
						  qboolean picmip, 
						  qboolean isLightmap, 
						  qboolean allowTC, 
						  int *pformat, 
						  USHORT *pUploadWidth, USHORT *pUploadHeight )
{
	if (format == GL_RGBA)
	{
	    int			samples;
	    int			i, c;
	    byte		*scan;
	    float		rMax = 0, gMax = 0, bMax = 0;
	    int			width = *pUploadWidth;
	    int			height = *pUploadHeight; 
    
	    //
	    // perform optional picmip operation
	    //
	    if ( picmip ) {
		    for(i = 0; i < r_picmip->integer; i++) {
			    R_MipMap( (byte *)data, width, height );
			    width >>= 1;
			    height >>= 1;
			    if (width < 1) {
				    width = 1;
			    }
			    if (height < 1) {
				    height = 1;
			    }
		    }
	    }
    
	    //
	    // clamp to the current upper OpenGL limit
	    // scale both axis down equally so we don't have to
	    // deal with a half mip resampling
	    //
	    while ( width > glConfig.maxTextureSize	|| height > glConfig.maxTextureSize ) {
		    R_MipMap( (byte *)data, width, height );
		    width >>= 1;
		    height >>= 1;
	    }
    
	    //
	    // scan the texture for each channel's max values
	    // and verify if the alpha channel is being used or not
	    //
	    c = width*height;
	    scan = ((byte *)data);
	    samples = 3;
	    for ( i = 0; i < c; i++ )
	    {
		    if ( scan[i*4+0] > rMax )
		    {
			    rMax = scan[i*4+0];
		    }
		    if ( scan[i*4+1] > gMax )
		    {
			    gMax = scan[i*4+1];
		    }
		    if ( scan[i*4+2] > bMax )
		    {
			    bMax = scan[i*4+2];
		    }
		    if ( scan[i*4 + 3] != 255 ) 
		    {
			    samples = 4;
			    break;
		    }
	    }

	    // select proper internal format
	    if ( samples == 3 )
	    {
		    if ( glConfig.textureCompression == TC_S3TC && allowTC )
		    {
			    *pformat = GL_RGB4_S3TC;
		    }
		    else if ( glConfig.textureCompression == TC_S3TC_DXT && allowTC )
		    {	// Compress purely color - no alpha
			    if ( r_texturebits->integer == 16 ) {
				    *pformat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;	//this format cuts to 16 bit
			    }
			    else {//if we aren't using 16 bit then, use 32 bit compression
				    *pformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			    }
		    }
		    else if ( isLightmap && r_texturebitslm->integer > 0 )
		    {
			    // Allow different bit depth when we are a lightmap
			    if ( r_texturebitslm->integer == 16 )
			    {
				    *pformat = GL_RGB5;
			    }
			    else if ( r_texturebitslm->integer == 32 )
			    {
				    *pformat = GL_RGB8;
			    }
		    }
		    else if ( r_texturebits->integer == 16 )
		    {
			    *pformat = GL_RGB5;
		    }
		    else if ( r_texturebits->integer == 32 )
		    {
			    *pformat = GL_RGB8;
		    }
		    else
		    {
			    *pformat = 3;
		    }
	    }
	    else if ( samples == 4 )
	    {
		    if ( glConfig.textureCompression == TC_S3TC_DXT && allowTC)
		    {	// Compress both alpha and color
			    *pformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		    }
		    else if ( r_texturebits->integer == 16 )
		    {
			    *pformat = GL_RGBA4;
		    }
		    else if ( r_texturebits->integer == 32 )
		    {
			    *pformat = GL_RGBA8;
		    }
		    else
		    {
			    *pformat = 4;
		    }
	    }

		*pUploadWidth = width;
		*pUploadHeight = height;

	    // copy or resample data as appropriate for first MIP level
	    if (!mipmap)
	    {
		    qglTexImage2D (GL_TEXTURE_2D, 0, *pformat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		    goto done;
	    }

		R_LightScaleTexture (data, width, height, !mipmap );
    
	    qglTexImage2D (GL_TEXTURE_2D, 0, *pformat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
    
	    if (mipmap)
	    {
		    int		miplevel;
    
		    miplevel = 0;
		    while (width > 1 || height > 1)
		    {
			    R_MipMap( (byte *)data, width, height );
			    width >>= 1;
			    height >>= 1;
			    if (width < 1)
				    width = 1;
			    if (height < 1)
				    height = 1;
			    miplevel++;
    
			    if ( r_colorMipLevels->integer ) 
			    {
				    R_BlendOverTexture( (byte *)data, width * height, mipBlendColors[miplevel] );
			    }
    
			    qglTexImage2D (GL_TEXTURE_2D, miplevel, *pformat, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		    }
	    }
	}
	else
	{

	}

done:

	if (mipmap)
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		if( r_ext_texture_filter_anisotropic->integer > 1 && glConfig.maxTextureFilterAnisotropy > 0 )
		{			
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_ext_texture_filter_anisotropic->value );
		}
	}
	else
	{
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	GL_CheckErrors();
}
#endif // _XBOX

#ifdef _XBOX
typedef tmap (int, image_t *)	AllocatedImages_t;
								AllocatedImages_t* AllocatedImages = NULL;
								AllocatedImages_t::iterator itAllocatedImages;
#else
class CStringComparator
{
public:
	bool operator()(const char *s1, const char *s2) const { return(stricmp(s1, s2) < 0); } 
};

typedef map <LPCSTR, image_t *, CStringComparator>	AllocatedImages_t;
													AllocatedImages_t AllocatedImages;
													AllocatedImages_t::iterator itAllocatedImages;
#endif // _XBOX
int giTextureBindNum = 1024;	// will be set to this anyway at runtime, but wtf?


// return = number of images in the list, for those interested
//
#ifdef _XBOX
int R_Images_StartIteration(void)
{
	if(!AllocatedImages)
		return 0;

	itAllocatedImages = AllocatedImages->begin();
	return AllocatedImages->size();
}

image_t *R_Images_GetNextIteration(void)
{
	if(!AllocatedImages)
		return NULL;

	if (itAllocatedImages == AllocatedImages->end())
		return NULL;

	image_t *pImage = (*itAllocatedImages).second;
	++itAllocatedImages;
	return pImage;
}
#else
int R_Images_StartIteration(void)
{
	itAllocatedImages = AllocatedImages.begin();
	return AllocatedImages.size();
}

image_t *R_Images_GetNextIteration(void)
{
	if (itAllocatedImages == AllocatedImages.end())
		return NULL;

	image_t *pImage = (*itAllocatedImages).second;
	++itAllocatedImages;
	return pImage;
}
#endif

// clean up anything to do with an image_t struct, but caller will have to clear the internal to an image_t struct ready for either struct free() or overwrite...
//
static void R_Images_DeleteImageContents( image_t *pImage )
{
	assert(pImage);	// should never be called with NULL
	if (pImage)
	{
		qglDeleteTextures( 1, &pImage->texnum );
		Z_Free(pImage);
	}
}

static void GL_ResetBinds(void)
{
	memset( glState.currenttextures, 0, sizeof( glState.currenttextures ) );
	if ( qglBindTexture ) 
	{
		if ( qglActiveTextureARB ) 
		{
			GL_SelectTexture( 1 );
			qglBindTexture( GL_TEXTURE_2D, 0 );
			GL_SelectTexture( 0 );
			qglBindTexture( GL_TEXTURE_2D, 0 );
		} 
		else 
		{
			qglBindTexture( GL_TEXTURE_2D, 0 );
		}
	}
}

// special function used in conjunction with "devmapbsp"...
//
#ifdef _XBOX
void R_Images_DeleteLightMaps(void)
{
	qboolean bEraseOccured = qfalse;
	for (AllocatedImages_t::iterator itImage = AllocatedImages->begin(); itImage != AllocatedImages->end(); bEraseOccured?itImage:++itImage)
	{			
		bEraseOccured = qfalse;

		image_t *pImage = (*itImage).second;
		
		if (pImage->isLightmap)
		{
			R_Images_DeleteImageContents(pImage);

			AllocatedImages->erase(itImage++);
			bEraseOccured = qtrue;
		}
	}

	GL_ResetBinds();
}

#else // _XBOX

void R_Images_DeleteLightMaps(void)
{
	qboolean bEraseOccured = qfalse;

	for (AllocatedImages_t::iterator itImage = AllocatedImages.begin(); itImage != AllocatedImages.end(); bEraseOccured?itImage:++itImage)
	{			
		bEraseOccured = qfalse;

		image_t *pImage = (*itImage).second;
		
		if (pImage->imgName[0] == '$' /*&& strstr(pImage->imgName,"lightmap")*/)	// loose check, but should be ok
		{
			R_Images_DeleteImageContents(pImage);
#ifdef _WIN32
			itImage = AllocatedImages.erase(itImage);
#else
            AllocatedImages_t::iterator itTemp = itImage;
            itImage++;
            AllocatedImages.erase(itTemp);
#endif

			bEraseOccured = qtrue;
		}
	}

	GL_ResetBinds();
}
#endif // _XBOX

// special function currently only called by Dissolve code...
//
#ifdef _XBOX
void R_Images_DeleteImage(image_t *pImage)
{		
	// Even though we supply the image handle, we need to get the corresponding iterator entry...
	//
	AllocatedImages_t::iterator itImage = AllocatedImages->find(pImage->imgCode);
	if (itImage != AllocatedImages->end())
	{		
		R_Images_DeleteImageContents(pImage);
		AllocatedImages->erase(itImage);
	}
	else
	{
		assert(0);
	}
}
#else // _XBOX

void R_Images_DeleteImage(image_t *pImage)
{		
	// Even though we supply the image handle, we need to get the corresponding iterator entry...
	//
	AllocatedImages_t::iterator itImage = AllocatedImages.find(pImage->imgName);
	if (itImage != AllocatedImages.end())
	{		
		R_Images_DeleteImageContents(pImage);
		AllocatedImages.erase(itImage);
	}
	else
	{
		assert(0);
	}
}
#endif // _XBOX

// called only at app startup, vid_restart, app-exit
//
void R_Images_Clear(void)
{		
	image_t *pImage;
	//	int iNumImages = 
	   				  R_Images_StartIteration();
	while ( (pImage = R_Images_GetNextIteration()) != NULL)
	{
		R_Images_DeleteImageContents(pImage);
	}

#ifdef _XBOX
	AllocatedImages->clear();
#else
	AllocatedImages.clear();
#endif

	giTextureBindNum = 1024;
}


void RE_RegisterImages_Info_f( void )
{
#ifndef _XBOX
	image_t *pImage	= NULL;
	int iImage		= 0;
	int iTexels		= 0;

	int iNumImages	= R_Images_StartIteration();
	while ( (pImage	= R_Images_GetNextIteration()) != NULL)
	{
		VID_Printf( PRINT_ALL, "%d: (%4dx%4dy) \"%s\"",iImage, pImage->width, pImage->height, pImage->imgName);
		VID_Printf( PRINT_ALL, ", levused %d",pImage->iLastLevelUsedOn);
		VID_Printf( PRINT_ALL, "\n");

		iTexels += pImage->width * pImage->height;
		iImage++;
	}
	VID_Printf( PRINT_ALL, "%d Images. %d (%.2fMB) texels total, (not including mipmaps)\n",iNumImages, iTexels, (float)iTexels / 1024.0f / 1024.0f);
	VID_Printf( PRINT_DEVELOPER, "RE_RegisterMedia_GetLevel(): %d",RE_RegisterMedia_GetLevel());
#endif // _XBOX
}


// implement this if you need to, do a find for the caller. I don't need it though, so far.
//
//void		RE_RegisterImages_LevelLoadBegin(const char *psMapName);


// currently, this just goes through all the images and dumps any not referenced on this level...
//
#ifdef _XBOX
qboolean RE_RegisterImages_LevelLoadEnd(void)
{
	qboolean bEraseOccured = qfalse;
	for (AllocatedImages_t::iterator itImage = AllocatedImages->begin(); itImage != AllocatedImages->end(); bEraseOccured?itImage:++itImage)
	{			
		bEraseOccured = qfalse;

		image_t *pImage = (*itImage).second;

		// don't un-register system shaders (*fog, *dlight, *white, *default), but DO de-register lightmaps ("$<mapname>/lightmap%d")
		if (!pImage->isSystem)
		{
			// image used on this level?
			//
			if ( pImage->iLastLevelUsedOn != RE_RegisterMedia_GetLevel() )
			{	// nope, so dump it...
				//VID_Printf( PRINT_DEVELOPER, "Dumping image \"%s\"\n",pImage->imgName);
				R_Images_DeleteImageContents(pImage);
				itImage = AllocatedImages->erase(itImage);
				bEraseOccured = qtrue;
			}
		}
	}

	GL_ResetBinds();

	return bEraseOccured;
}

#else // _XBOX
qboolean RE_RegisterImages_LevelLoadEnd(void)
{
	//VID_Printf( PRINT_DEVELOPER, "RE_RegisterImages_LevelLoadEnd():\n");

	qboolean bEraseOccured = qfalse;
	for (AllocatedImages_t::iterator itImage = AllocatedImages.begin(); itImage != AllocatedImages.end(); bEraseOccured?itImage:++itImage)
	{			
		bEraseOccured = qfalse;

		image_t *pImage = (*itImage).second;

		// don't un-register system shaders (*fog, *dlight, *white, *default), but DO de-register lightmaps ("$<mapname>/lightmap%d")
		if (pImage->imgName[0] != '*')
		{
			// image used on this level?
			//
			if ( pImage->iLastLevelUsedOn != RE_RegisterMedia_GetLevel() )
			{	// nope, so dump it...
				//VID_Printf( PRINT_DEVELOPER, "Dumping image \"%s\"\n",pImage->imgName);
				R_Images_DeleteImageContents(pImage);
#ifdef _WIN32
				itImage = AllocatedImages.erase(itImage);
#else
                AllocatedImages_t::iterator itTemp = itImage;
                itImage++;
                AllocatedImages.erase(itTemp);
#endif
                bEraseOccured = qtrue;
			}
		}
	}

	//VID_Printf( PRINT_DEVELOPER, "RE_RegisterImages_LevelLoadEnd(): Ok\n");	

	GL_ResetBinds();

	return bEraseOccured;
}
#endif // _XBOX



// returns image_t struct if we already have this, else NULL. No disk-open performed 
//	(important for creating default images).
//
// This is called by both R_FindImageFile and anything that creates default images...
//
#ifdef _XBOX
static image_t *R_FindImageFile_NoLoad(const char *name, int mipcount, qboolean allowPicmip, int glWrapClampMode )
{	
	if (!name) {
		return NULL;
	}

	char *pName = GenerateImageMappingName(name);

	//
	// see if the image is already loaded
	//
	int code = crc32(0, (const Bytef *)pName, strlen(pName));
	AllocatedImages_t::iterator itAllocatedImage = AllocatedImages->find(code);
	if (itAllocatedImage != AllocatedImages->end())
	{	
		image_t *pImage = (*itAllocatedImage).second;

		// the white image can be used with any set of parms, but other mismatches are errors...
		//
		if ( strcmp( pName, "*white" ) ) {
			if ( !!pImage->mipcount != !!mipcount ) {
				VID_Printf( PRINT_WARNING, "WARNING: reused image %s with mixed mipmap parm\n", pName );
			}
			if ( pImage->allowPicmip != !!allowPicmip ) {
				VID_Printf( PRINT_WARNING, "WARNING: reused image %s with mixed allowPicmip parm\n", pName );
			}
		}

		pImage->iLastLevelUsedOn = RE_RegisterMedia_GetLevel();
			  
		return pImage;
	}

	return NULL;
}

#else // _XBOX

static image_t *R_FindImageFile_NoLoad(const char *name, qboolean mipmap, qboolean allowPicmip, qboolean allowTC, int glWrapClampMode )
{	
	if (!name) {
		return NULL;
	}

	char *pName = GenerateImageMappingName(name);

	//
	// see if the image is already loaded
	//
	AllocatedImages_t::iterator itAllocatedImage = AllocatedImages.find(pName);
	if (itAllocatedImage != AllocatedImages.end())
	{	
		image_t *pImage = (*itAllocatedImage).second;

		// the white image can be used with any set of parms, but other mismatches are errors...
		//
		if ( strcmp( pName, "*white" ) ) {
			if ( pImage->mipmap != !!mipmap ) {
				VID_Printf( PRINT_WARNING, "WARNING: reused image %s with mixed mipmap parm\n", pName );
			}
			if ( pImage->allowPicmip != !!allowPicmip ) {
				VID_Printf( PRINT_WARNING, "WARNING: reused image %s with mixed allowPicmip parm\n", pName );
			}
			if ( pImage->wrapClampMode != glWrapClampMode ) {
				VID_Printf( PRINT_WARNING, "WARNING: reused image %s with mixed glWrapClampMode parm\n", pName );
			}
		}
			  
		pImage->iLastLevelUsedOn = RE_RegisterMedia_GetLevel();

		return pImage;
	}

	return NULL;
}
#endif // _XBOX




/*
================
R_CreateImage

This is the only way any image_t are created
================
*/
image_t *R_CreateImage( const char *name, const byte *pic, int width, int height, 
					   GLenum format, qboolean mipmap, qboolean allowPicmip, qboolean allowTC, int glWrapClampMode)
{
	image_t		*image;
	qboolean	isLightmap = qfalse;

	if (strlen(name) >= MAX_QPATH ) {
		Com_Error (ERR_DROP, "R_CreateImage: \"%s\" is too long\n", name);
	}

	if(glConfig.clampToEdgeAvailable && glWrapClampMode == GL_CLAMP) {
		glWrapClampMode = GL_CLAMP_TO_EDGE;
	}

	if (name[0] == '$')
	{
		isLightmap = qtrue;
	}

	if ( (width&(width-1)) || (height&(height-1)) )
	{
		Com_Error( ERR_FATAL, "R_CreateImage: %s dimensions (%i x %i) not power of 2!\n",name,width,height);
	}

	image = R_FindImageFile_NoLoad(name, mipmap, allowPicmip, allowTC, glWrapClampMode );
	if (image) {
		return image;
	}

	image = (image_t*) Z_Malloc( sizeof( image_t ), TAG_IMAGE_T, qtrue );

	//image->imgfileSize=fileSize;	
	
	image->texnum = 1024 + giTextureBindNum++;	// ++ is of course staggeringly important...

	// record which map it was used on...
	//
	image->iLastLevelUsedOn = RE_RegisterMedia_GetLevel();

	image->mipmap = !!mipmap;
	image->allowPicmip = !!allowPicmip;

	Q_strncpyz(image->imgName, name, sizeof(image->imgName));

	image->width = width;
	image->height = height;
	image->wrapClampMode = glWrapClampMode;

	if ( qglActiveTextureARB ) {
		GL_SelectTexture( 0 );
	}

	GL_Bind(image);

	Upload32( (unsigned *)pic,	format,
								image->mipmap,
								allowPicmip, 
								isLightmap, 
								allowTC,
								&image->internalFormat,
								&image->width,
								&image->height );

	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, glWrapClampMode );
	qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, glWrapClampMode );

	qglBindTexture( GL_TEXTURE_2D, 0 );	//jfm: i don't know why this is here, but it breaks lightmaps when there's only 1
	glState.currenttextures[glState.currenttmu] = 0;	//mark it not bound

	LPCSTR psNewName = GenerateImageMappingName(name);
	Q_strncpyz(image->imgName, psNewName, sizeof(image->imgName));
	AllocatedImages[ image->imgName ] = image;

	return image;
}

void R_CreateAutomapImage( const char *name, const byte *pic, int width, int height, 
					   qboolean mipmap, qboolean allowPicmip, qboolean allowTC, int glWrapClampMode ) 
{
	R_CreateImage(name, pic, width, height, GL_RGBA, mipmap, allowPicmip, allowTC, glWrapClampMode);
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

/*
Ghoul2 Insert Start
*/

bool LoadTGAPalletteImage ( const char *name, byte **pic, int *width, int *height)
{
	int		columns, rows, numPixels;
	byte	*buf_p;
	byte	*buffer;
	TargaHeader	targa_header;
	byte	*dataStart;

	*pic = NULL;

	//
	// load the file
	//
	ri.FS_ReadFile ( ( char * ) name, (void **)&buffer);
	if (!buffer) {
		return false;
	}

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;
	
	targa_header.colormap_index = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.colormap_length = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.y_origin = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.width = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.height = LittleShort ( *(short *)buf_p );
	buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.image_type!=1 )
	{
		Com_Error (ERR_DROP, "LoadTGAPalletteImage: Only type 1 (uncompressed pallettised) TGA images supported\n");
	}

	if ( targa_header.colormap_type == 0 )
	{
		Com_Error( ERR_DROP, "LoadTGAPalletteImage: colormaps ONLY supported\n" );
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (width)
		*width = columns;
	if (height)
		*height = rows;

	*pic = (unsigned char *) Z_Malloc (numPixels, TAG_TEMP_TGA, qfalse);
	if (targa_header.id_length != 0)
	{
		buf_p += targa_header.id_length;  // skip TARGA image comment
	}
	dataStart = buf_p + (targa_header.colormap_length * (targa_header.colormap_size / 4));
	memcpy(*pic, dataStart, numPixels);
	ri.FS_FreeFile (buffer);

	return true;
}

/*
Ghoul2 Insert End
*/


// My TGA loader...
//
//---------------------------------------------------
#pragma pack(push,1)
typedef struct
{
	byte	byIDFieldLength;	// must be 0
	byte	byColourmapType;	// 0 = truecolour, 1 = paletted, else bad
	byte	byImageType;		// 1 = colour mapped (palette), uncompressed, 2 = truecolour, uncompressed, else bad
	word	w1stColourMapEntry;	// must be 0
	word	wColourMapLength;	// 256 for 8-bit palettes, else 0 for true-colour
	byte	byColourMapEntrySize; // 24 for 8-bit palettes, else 0 for true-colour
	word	wImageXOrigin;		// ignored
	word	wImageYOrigin;		// ignored
	word	wImageWidth;		// in pixels
	word	wImageHeight;		// in pixels
	byte	byImagePlanes;		// bits per pixel	(8 for paletted, else 24 for true-colour)
	byte	byScanLineOrder;	// Image descriptor bytes
								// bits 0-3 = # attr bits (alpha chan)
								// bits 4-5 = pixel order/dir
								// bits 6-7 scan line interleave (00b=none,01b=2way interleave,10b=4way)
} TGAHeader_t;
#pragma pack(pop)


// *pic == pic, else NULL for failed.
//
//  returns false if found but had a format error, else true for either OK or not-found (there's a reason for this)
//

int LoadTGA ( const char *name, byte **pic, int *width, int *height)
{
	char sErrorString[1024];
	bool bFormatErrors = false;

	// these don't need to be declared or initialised until later, but the compiler whines that 'goto' skips them.
	//
	byte *pRGBA = NULL;	
	byte *pOut	= NULL;
	byte *pIn	= NULL;


	*pic = NULL;

#define TGA_FORMAT_ERROR(blah) {sprintf(sErrorString,blah); bFormatErrors = true; goto TGADone;}
//#define TGA_FORMAT_ERROR(blah) Com_Error( ERR_DROP, blah );

	//
	// load the file
	//
	byte *pTempLoadedBuffer = 0;
	const int filelen = ri.FS_ReadFile ( ( char * ) name, (void **)&pTempLoadedBuffer);
	if (!pTempLoadedBuffer) {
		return 0;
	}

	TGAHeader_t *pHeader = (TGAHeader_t *) pTempLoadedBuffer;

	if (pHeader->byColourmapType!=0)
	{	
		TGA_FORMAT_ERROR("LoadTGA: colourmaps not supported\n" );		
	}

	if (pHeader->byImageType != 2 && pHeader->byImageType != 3 && pHeader->byImageType != 10)
	{
		TGA_FORMAT_ERROR("LoadTGA: Only type 2 (RGB), 3 (gray), and 10 (RLE-RGB) images supported\n");		
	}
		
	if (pHeader->w1stColourMapEntry != 0)
	{
		TGA_FORMAT_ERROR("LoadTGA: colourmaps not supported\n" );		
	}

	if (pHeader->wColourMapLength !=0 && pHeader->wColourMapLength != 256)
	{
		TGA_FORMAT_ERROR("LoadTGA: ColourMapLength must be either 0 or 256\n" );
	}

	if (pHeader->byColourMapEntrySize != 0 && pHeader->byColourMapEntrySize != 24)
	{
		TGA_FORMAT_ERROR("LoadTGA: ColourMapEntrySize must be either 0 or 24\n" );
	}

	if ( ( pHeader->byImagePlanes != 24 && pHeader->byImagePlanes != 32) && (pHeader->byImagePlanes != 8 && pHeader->byImageType != 3))
	{
		TGA_FORMAT_ERROR("LoadTGA: Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported\n");
	}

	if ((pHeader->byScanLineOrder&0x30)!=0x00 &&
		(pHeader->byScanLineOrder&0x30)!=0x10 &&
		(pHeader->byScanLineOrder&0x30)!=0x20 &&
		(pHeader->byScanLineOrder&0x30)!=0x30
		)
	{
		TGA_FORMAT_ERROR("LoadTGA: ScanLineOrder must be either 0x00,0x10,0x20, or 0x30\n");		
	}



	// these last checks are so i can use ID's RLE-code. I don't dare fiddle with it or it'll probably break...
	//
	if ( pHeader->byImageType == 10)
	{
		if ((pHeader->byScanLineOrder & 0x30) != 0x00)
		{
			TGA_FORMAT_ERROR("LoadTGA: RLE-RGB Images (type 10) must be in bottom-to-top format\n");
		}
		if (pHeader->byImagePlanes != 24 && pHeader->byImagePlanes != 32)	// probably won't happen, but avoids compressed greyscales?
		{
			TGA_FORMAT_ERROR("LoadTGA: RLE-RGB Images (type 10) must be 24 or 32 bit\n");
		}
	}

	// now read the actual bitmap in...
	//
	// Image descriptor bytes
	// bits 0-3 = # attr bits (alpha chan)
	// bits 4-5 = pixel order/dir
	// bits 6-7 scan line interleave (00b=none,01b=2way interleave,10b=4way)
	//
	int iYStart,iXStart,iYStep,iXStep;

	switch(pHeader->byScanLineOrder & 0x30)		
	{
		default:	// default case stops the compiler complaining about using uninitialised vars
		case 0x00:					//	left to right, bottom to top

			iXStart = 0;
			iXStep  = 1;

			iYStart = pHeader->wImageHeight-1;
			iYStep  = -1;

			break;

		case 0x10:					//  right to left, bottom to top

			iXStart = pHeader->wImageWidth-1;
			iXStep  = -1;

			iYStart = pHeader->wImageHeight-1;
			iYStep	= -1;

			break;

		case 0x20:					//  left to right, top to bottom

			iXStart = 0;
			iXStep  = 1;

			iYStart = 0;
			iYStep  = 1;

			break;

		case 0x30:					//  right to left, top to bottom

			iXStart = pHeader->wImageWidth-1;
			iXStep  = -1;

			iYStart = 0;
			iYStep  = 1;

			break;
	}

	// feed back the results...
	//
	if (width)
		*width = pHeader->wImageWidth;
	if (height)
		*height = pHeader->wImageHeight;

	pRGBA	= (byte *) Z_Malloc (pHeader->wImageWidth * pHeader->wImageHeight * 4, TAG_TEMP_TGA, qfalse);
	*pic	= pRGBA;
	pOut	= pRGBA;
	pIn		= pTempLoadedBuffer + sizeof(*pHeader);

	// I don't know if this ID-thing here is right, since comments that I've seen are at the end of the file, 
	//	with a zero in this field. However, may as well...
	//
	if (pHeader->byIDFieldLength != 0)
		pIn += pHeader->byIDFieldLength;	// skip TARGA image comment

	byte red,green,blue,alpha;

	if ( pHeader->byImageType == 2 || pHeader->byImageType == 3 )	// RGB or greyscale
	{
		for (int y=iYStart, iYCount=0; iYCount<pHeader->wImageHeight; y+=iYStep, iYCount++)
		{
			pOut = pRGBA + y * pHeader->wImageWidth *4;			
			for (int x=iXStart, iXCount=0; iXCount<pHeader->wImageWidth; x+=iXStep, iXCount++)
			{
				switch (pHeader->byImagePlanes)
				{
					case 8:
						blue	= *pIn++;
						green	= blue;
						red		= blue;
						*pOut++ = red;
						*pOut++ = green;
						*pOut++ = blue;
						*pOut++ = 255;
						break;

					case 24:
						blue	= *pIn++;
						green	= *pIn++;
						red		= *pIn++;
						*pOut++ = red;
						*pOut++ = green;
						*pOut++ = blue;
						*pOut++ = 255;
						break;

					case 32:
						blue	= *pIn++;
						green	= *pIn++;
						red		= *pIn++;
						alpha	= *pIn++;
						*pOut++ = red;
						*pOut++ = green;
						*pOut++ = blue;
						*pOut++ = alpha;
						break;
					
					default:
						assert(0);	// if we ever hit this, someone deleted a header check higher up
						TGA_FORMAT_ERROR("LoadTGA: Image can only have 8, 24 or 32 planes for RGB/greyscale\n");						
						break;
				}
			}		
		}
	}
	else 
	if (pHeader->byImageType == 10)   // RLE-RGB
	{
		// I've no idea if this stuff works, I normally reject RLE targas, but this is from ID's code
		//	so maybe I should try and support it...
		//
		byte packetHeader, packetSize, j;

		for (int y = pHeader->wImageHeight-1; y >= 0; y--)
		{
			pOut = pRGBA + y * pHeader->wImageWidth *4;
			for (int x=0; x<pHeader->wImageWidth;)
			{
				packetHeader = *pIn++;
				packetSize   = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80)         // run-length packet
				{
					switch (pHeader->byImagePlanes) 
					{
						case 24:

							blue	= *pIn++;
							green	= *pIn++;
							red		= *pIn++;
							alpha	= 255;
							break;

						case 32:
							
							blue	= *pIn++;
							green	= *pIn++;
							red		= *pIn++;
							alpha	= *pIn++;
							break;

						default:
							assert(0);	// if we ever hit this, someone deleted a header check higher up
							TGA_FORMAT_ERROR("LoadTGA: RLE-RGB can only have 24 or 32 planes\n");
							break;
					}
	
					for (j=0; j<packetSize; j++) 
					{
						*pOut++	= red;
						*pOut++	= green;
						*pOut++	= blue;
						*pOut++	= alpha;
						x++;
						if (x == pHeader->wImageWidth)  // run spans across rows
						{
							x = 0;
							if (y > 0)
								y--;
							else
								goto breakOut;
							pOut = pRGBA + y * pHeader->wImageWidth * 4;
						}
					}
				}
				else 
				{	// non run-length packet

					for (j=0; j<packetSize; j++) 
					{
						switch (pHeader->byImagePlanes) 
						{
							case 24:

								blue	= *pIn++;
								green	= *pIn++;
								red		= *pIn++;
								*pOut++ = red;
								*pOut++ = green;
								*pOut++ = blue;
								*pOut++ = 255;
								break;

							case 32:
								blue	= *pIn++;
								green	= *pIn++;
								red		= *pIn++;
								alpha	= *pIn++;
								*pOut++ = red;
								*pOut++ = green;
								*pOut++ = blue;
								*pOut++ = alpha;
								break;

							default:
								assert(0);	// if we ever hit this, someone deleted a header check higher up
								TGA_FORMAT_ERROR("LoadTGA: RLE-RGB can only have 24 or 32 planes\n");
								break;
						}
						x++;
						if (x == pHeader->wImageWidth)  // pixel packet run spans across rows
						{
							x = 0;
							if (y > 0)
								y--;
							else
								goto breakOut;
							pOut = pRGBA + y * pHeader->wImageWidth * 4;
						}
					}
				}
			}
			breakOut:;
		}
	}

TGADone:

	ri.FS_FreeFile (pTempLoadedBuffer);

	if (bFormatErrors)
	{
		Com_Error( ERR_DROP, "%s( File: \"%s\" )\n",sErrorString,name);
	}
	return filelen;
}

//===================================================================

/*
=================
R_LoadImage

Loads any of the supported image types into a cannonical
32 bit format.
=================
*/
void R_LoadImage( const char *shortname, byte **pic, int *width, int *height, GLenum *format ) {
	int		bytedepth;
	char	name[MAX_QPATH];

	*pic = NULL;
	*width = 0;
	*height = 0;

	//handle external LMs
	if (shortname[0] == '$') {
		Q_strncpyz( name, shortname+1, sizeof( name ) );
	} else {
		Q_strncpyz( name, shortname, sizeof( name ) );
	}

	*format = GL_RGBA;
	COM_StripExtension(name,name);
	COM_DefaultExtension(name, sizeof(name), ".jpg");

	//First try .jpg
	LoadJPG( name, pic, width, height );
	if (*pic)
	{
		return;
	}

	COM_StripExtension(name,name);
	COM_DefaultExtension(name, sizeof(name), ".png");	

	//No .jpg existed, try .png
	LoadPNG32( name, pic, width, height, &bytedepth );
	if (*pic)
	{
		return;
	}

	COM_StripExtension(name,name);
	COM_DefaultExtension(name, sizeof(name), ".tga");

	//No .jpg existed and no .png existed, try .tga as a last resort.
	LoadTGA( name, pic, width, height );
}

void R_LoadDataImage( const char *name, byte **pic, int *width, int *height)
{
	int		len;
	char	work[MAX_QPATH];

	*pic = NULL;
	*width = 0;
	*height = 0;

	len = strlen(name);
	if(len >= MAX_QPATH)
	{
		return;
	}
	if (len < 5)
	{
		return;
	}
//	MD_PushTag(TAG_DATA_IMAGE_LOAD);

	strcpy(work, name);

	COM_DefaultExtension( work, sizeof( work ), ".png" );
	LoadPNG8( work, pic, width, height );
	
	if (!pic || !*pic)
	{ //png load failed, try jpeg
		strcpy(work, name);
		COM_DefaultExtension( work, sizeof( work ), ".jpg" );
		LoadJPG( work, pic, width, height );
	}

	if (!pic || !*pic)
	{ //both png and jpeg failed, try targa
		strcpy(work, name);
		COM_DefaultExtension( work, sizeof( work ), ".tga" );
		LoadTGA( work, pic, width, height );
	}

	if(*pic)
	{
//		MD_PopTag();
		return;
	}
	// Dataimage loading failed
	Com_Printf("Couldn't read %s -- dataimage load failed\n", name);
//	MD_PopTag();
}

void R_InvertImage(byte *data, int width, int height, int depth)
{
	byte	*newData;
	byte	*oldData;
	byte	*saveData;
	int		y, stride;

	stride = width * depth;

	oldData = data + ((height - 1) * stride);
	newData = (byte *)Z_Malloc(height * stride, TAG_TEMP_WORKSPACE, qfalse );
	saveData = newData;

	for(y = 0; y < height; y++)
	{
		memcpy(newData, oldData, stride);
		newData += stride;
		oldData -= stride;
	}
	memcpy(data, saveData, height * stride);
	Z_Free(saveData);
}

// Lanczos3 image resampling. Better than bicubic, based on sin(x)/x algorithm

#define	LANCZOS3	(3.0f)
#define M_PI_OVER_3	(M_PI / 3.0f)

typedef struct
{
	int		pixel;
	float	weight;
} contrib_t;

typedef struct
{
	int			n;		// number of contributors
	contrib_t	*p;		// pointer to list of contributions
} contrib_list_t;

// sin(x)/x * sin(x/3)/(x/3)

float Lanczos3(float t)
{
	if(!t)
	{
		return(1.0f);
	}
	t = (float)fabs(t);
	if(t < 3.0f)
	{
		return(sinf(t * M_PI) * sinf(t * M_PI_OVER_3) / (t * M_PI * t * M_PI_OVER_3));
	}
	return(0.0f);
}

void R_Resample(byte *source, int swidth, int sheight, byte *dest, int dwidth, int dheight, int components)
{
	int				i, j, k, l, count, left, right, num;
	int				pixel;
	byte			*raster;
	float			center, weight, scale, width, height;
	contrib_list_t	*contributors;
	const memtag_t	usedTag = TAG_TEMP_WORKSPACE;

	byte *work = (byte *)Z_Malloc(dwidth * sheight * components, usedTag, qfalse);

	// Pre calculate filter contributions for rows
	contributors = (contrib_list_t *)Z_Malloc(sizeof(contrib_list_t) * dwidth, usedTag, qfalse);

	float xscale = (float)dwidth / (float)swidth;

	if(xscale < 1.0f)
	{
		width = ceilf(LANCZOS3 / xscale);
		scale = xscale;
	}
	else
	{
		width = LANCZOS3;
		scale = 1.0f;
	}
	num = ((int)width * 2) + 1;

	for(i = 0; i < dwidth; i++)
	{
		contributors[i].n = 0;
		contributors[i].p = (contrib_t *)Z_Malloc(num * sizeof(contrib_t), usedTag, qfalse);

		center = (float)i / xscale;
		left = (int)ceilf(center - width);
		right = (int)floorf(center + width);

		for(j = left; j <= right; j++)
		{
			weight = Lanczos3((center - (float)j) * scale) * scale;
			if(j < 0)
			{
				pixel = -j;
			}
			else if(j >= swidth)
			{
				pixel = (swidth - j) + swidth - 1;
			}
			else
			{
				pixel = j;
			}
			count = contributors[i].n++;
			contributors[i].p[count].pixel = pixel;
			contributors[i].p[count].weight = weight;
		}
	}
	// Apply filters to zoom horizontally from source to work
	for(k = 0; k < sheight; k++)
	{
		raster = source + (k * swidth * components);
		for(i = 0; i < dwidth; i++)
		{
			for(l = 0; l < components; l++)
			{
				weight = 0.0f;
				for(j = 0; j < contributors[i].n; j++)
				{
					weight += raster[(contributors[i].p[j].pixel * components) + l] * contributors[i].p[j].weight;
				}
				pixel = (byte)Com_Clamp(0.0f, 255.0f, weight);
				work[(k * dwidth * components) + (i * components) + l] = pixel;
			}
		}
	}
	// Clean up
	for(i = 0; i < dwidth; i++)
	{
		Z_Free(contributors[i].p);
	}
	Z_Free(contributors);

	// Columns
	contributors = (contrib_list_t *)Z_Malloc(sizeof(contrib_list_t) * dheight, usedTag, qfalse);

	float yscale = (float)dheight / (float)sheight;
	if(yscale < 1.0f)
	{
		height = ceilf(LANCZOS3 / yscale);
		scale = yscale;
	}
	else
	{
		height = LANCZOS3;
		scale = 1.0f;
	}
	num = ((int)height * 2) + 1;

	for(i = 0; i < dheight; i++)
	{
		contributors[i].n = 0;
		contributors[i].p = (contrib_t *)Z_Malloc(num * sizeof(contrib_t), usedTag, qfalse);

		center = (float)i / yscale;
		left = (int)ceilf(center - height);
		right = (int)floorf(center + height);

		for(j = left; j <= right; j++)
		{
			weight = Lanczos3((center - (float)j) * scale) * scale;
			if(j < 0)
			{
				pixel = -j;
			}
			else if(j >= sheight)
			{
				pixel = (sheight - j) + sheight - 1;
			}
			else
			{
				pixel = j;
			}
			count = contributors[i].n++;
			contributors[i].p[count].pixel = pixel;
			contributors[i].p[count].weight = weight;
		}
	}
	// Apply filter to columns
	for(k = 0; k < dwidth; k++)
	{
		for(l = 0; l < components; l++)
		{
			for(i = 0; i < dheight; i++)
			{
				weight = 0.0f;
				for(j = 0; j < contributors[i].n; j++)
				{
					weight += work[(contributors[i].p[j].pixel * dwidth * components) + (k * components) + l] * contributors[i].p[j].weight;
				}
				pixel = (byte)Com_Clamp(0.0f, 255.0f, weight);
				dest[(i * dwidth * components) + (k * components) + l] = pixel;
			}
		}
	}
	// Clean up
	for(i = 0; i < dheight; i++)
	{
		Z_Free(contributors[i].p);
	}
	Z_Free(contributors);
	Z_Free(work);

//	MD_PopTag();
}

/*
===============
R_FindImageFile

Finds or loads the given image.
Returns NULL if it fails, not a default image.
==============
*/
image_t	*R_FindImageFile( const char *name, qboolean mipmap, qboolean allowPicmip, qboolean allowTC, int glWrapClampMode ) {
	image_t	*image;
	int		width, height;
	byte	*pic;
	GLenum	format;
//	long	hash;
   
	if (!name) {
		return NULL;
	}

	// need to do this here as well as in R_CreateImage, or R_FindImageFile_NoLoad() may complain about
	//	different clamp parms used...
	//
	if(glConfig.clampToEdgeAvailable && glWrapClampMode == GL_CLAMP) {
		glWrapClampMode = GL_CLAMP_TO_EDGE;
	}

	image = R_FindImageFile_NoLoad(name, mipmap, allowPicmip, allowTC, glWrapClampMode );
	if (image) {
		return image;
	}

	//
	// load the pic from disk
	//
	R_LoadImage( name, &pic, &width, &height, &format );
	if ( !pic ) {
        return NULL;            
	}

	image = R_CreateImage( ( char * ) name, pic, width, height, format, mipmap, allowPicmip, allowTC, glWrapClampMode );
	Z_Free( pic );
	return image;
}



// EF dlight image creation code
/*
================
R_CreateDlightImage
================
*/
/*
#define	DLIGHT_SIZE	16
static void R_CreateDlightImage( void ) {
	int		x,y;
	byte	data[DLIGHT_SIZE][DLIGHT_SIZE][4];
	int		b;

	// make a centered inverse-square falloff blob for dynamic lighting
	for (x=0 ; x<DLIGHT_SIZE ; x++) {
		for (y=0 ; y<DLIGHT_SIZE ; y++) {
			float	d;

			d = ( DLIGHT_SIZE/2 - 0.5 - x ) * ( DLIGHT_SIZE/2 - 0.5 - x ) +
				( DLIGHT_SIZE/2 - 0.5 - y ) * ( DLIGHT_SIZE/2 - 0.5 - y );
			b = 4000 / d;
			if (b > 255) {
				b = 255;
			} else if ( b < 75 ) {
				b = 0;
			}
			data[y][x][0] = 
			data[y][x][1] = 
			data[y][x][2] = 255;
			data[y][x][3] = b/8;			
		}
	}
	tr.dlightImage = R_CreateImage("*dlight", (byte *)data, DLIGHT_SIZE, DLIGHT_SIZE, qfalse, qfalse, GL_CLAMP );
}
*/
// Holomatch dlight image creation code
/*
================
R_CreateDlightImage
================
*/
#define	DLIGHT_SIZE	64
static void R_CreateDlightImage( void ) 
{
#ifndef _XBOX
	int		width, height;
	byte	*pic;
	GLenum	format;

	R_LoadImage("gfx/2d/dlight", &pic, &width, &height, &format);
	if (pic)
	{                                    
		tr.dlightImage = R_CreateImage("*dlight", pic, width, height, GL_RGBA, qfalse, qfalse, qfalse, GL_CLAMP );
		Z_Free(pic);
	}
	else
	{	// if we dont get a successful load
		int		x,y;
		byte	data[DLIGHT_SIZE][DLIGHT_SIZE][4];
		int		b;

		// make a centered inverse-square falloff blob for dynamic lighting
		for (x=0 ; x<DLIGHT_SIZE ; x++) {
			for (y=0 ; y<DLIGHT_SIZE ; y++) {
				float	d;

				d = ( DLIGHT_SIZE/2 - 0.5f - x ) * ( DLIGHT_SIZE/2 - 0.5f - x ) +
					( DLIGHT_SIZE/2 - 0.5f - y ) * ( DLIGHT_SIZE/2 - 0.5f - y );
				b = 4000 / d;
				if (b > 255) {
					b = 255;
				} else if ( b < 75 ) {
					b = 0;
				}
				data[y][x][0] = 
					data[y][x][1] = 
					data[y][x][2] = b;
				data[y][x][3] = 255;			
			}
		}
		tr.dlightImage = R_CreateImage("*dlight", (byte *)data, DLIGHT_SIZE, DLIGHT_SIZE, GL_RGBA, qfalse, qfalse, qfalse, GL_CLAMP );
	}
#endif
}

/*
=================
R_InitFogTable
=================
*/
void R_InitFogTable( void ) {
	int		i;
	float	d;
	float	exp;
	
	exp = 0.5;

	for ( i = 0 ; i < FOG_TABLE_SIZE ; i++ ) {
		d = pow ( (float)i/(FOG_TABLE_SIZE-1), exp );

		tr.fogTable[i] = d;
	}
}

/*
================
R_FogFactor

Returns a 0.0 to 1.0 fog density value
This is called for each texel of the fog texture on startup
and for each vertex of transparent shaders in fog dynamically
================
*/
float	R_FogFactor( float s, float t ) {
	float	d;

	s -= 1.0/512;
	if ( s < 0 ) {
		return 0;
	}
	if ( t < 1.0/32 ) {
		return 0;
	}
	if ( t < 31.0/32 ) {
		s *= (t - 1.0/32) / (30.0/32);
	}

	// we need to leave a lot of clamp range
	s *= 8;

	if ( s > 1.0 ) {
		s = 1.0;
	}

	d = tr.fogTable[ (int)(s * (FOG_TABLE_SIZE-1)) ];

	return d;
}

/*
================
R_CreateFogImage
================
*/
#define	FOG_S	256
#define	FOG_T	32
static void R_CreateFogImage( void ) {
	int		x,y;
	byte	*data;
	float	g;
	float	d;
	float	borderColor[4];

	data = (byte*) Z_Malloc( FOG_S * FOG_T * 4, TAG_TEMP_WORKSPACE, qfalse );

	g = 2.0;

	// S is distance, T is depth
	for (x=0 ; x<FOG_S ; x++) {
		for (y=0 ; y<FOG_T ; y++) {
			d = R_FogFactor( ( x + 0.5 ) / FOG_S, ( y + 0.5 ) / FOG_T );

			data[(y*FOG_S+x)*4+0] = 
			data[(y*FOG_S+x)*4+1] = 
			data[(y*FOG_S+x)*4+2] = 255;
			data[(y*FOG_S+x)*4+3] = 255*d;
		}
	}
	// standard openGL clamping doesn't really do what we want -- it includes
	// the border color at the edges.  OpenGL 1.2 has clamp-to-edge, which does
	// what we want.
#ifdef _XBOX
	tr.fogImage = R_CreateImage("*fog", (byte *)data, FOG_S, FOG_T, GL_RGBA, qfalse, qfalse, GL_CLAMP);
#else 
	tr.fogImage = R_CreateImage("*fog", (byte *)data, FOG_S, FOG_T, GL_RGBA, qfalse, qfalse, qfalse, GL_CLAMP);
#endif
	Z_Free( data );

	borderColor[0] = 1.0;
	borderColor[1] = 1.0;
	borderColor[2] = 1.0;
	borderColor[3] = 1;

	qglTexParameterfv( GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor );
}

/*
==================
R_CreateDefaultImage
==================
*/
#define	DEFAULT_SIZE	16
static void R_CreateDefaultImage( void ) {
	int		x;
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	// the default image will be a box, to allow you to see the mapping coordinates
	memset( data, 32, sizeof( data ) );
	for ( x = 0 ; x < DEFAULT_SIZE ; x++ ) {
		data[0][x][0] =
		data[0][x][1] =
		data[0][x][2] =
		data[0][x][3] = 255;

		data[x][0][0] =
		data[x][0][1] =
		data[x][0][2] =
		data[x][0][3] = 255;

		data[DEFAULT_SIZE-1][x][0] =
		data[DEFAULT_SIZE-1][x][1] =
		data[DEFAULT_SIZE-1][x][2] =
		data[DEFAULT_SIZE-1][x][3] = 255;

		data[x][DEFAULT_SIZE-1][0] =
		data[x][DEFAULT_SIZE-1][1] =
		data[x][DEFAULT_SIZE-1][2] =
		data[x][DEFAULT_SIZE-1][3] = 255;
	}
#ifdef _XBOX
	tr.defaultImage = R_CreateImage("*default", (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, GL_RGBA, qtrue, qfalse, GL_REPEAT);
#else
	tr.defaultImage = R_CreateImage("*default", (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, GL_RGBA, qtrue, qfalse, qtrue, GL_REPEAT);
#endif
}

/*
==================
R_CreateBuiltinImages
==================
*/
void R_UpdateSaveGameImage(const char *filename);

void R_CreateBuiltinImages( void ) {
	int		x,y;
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	R_CreateDefaultImage();

	// we use a solid white image instead of disabling texturing
	memset( data, 255, sizeof( data ) );
#ifdef _XBOX
	tr.whiteImage = R_CreateImage("*white", (byte *)data, 8, 8, GL_RGBA, qfalse, qfalse, GL_REPEAT);

	tr.screenImage = R_CreateImage("*screen", (byte *)data, 8, 8, GL_RGBA, 1, qfalse, GL_REPEAT );

	tr.saveGameImage = R_CreateImage("*savegame", (byte *)data, 8, 8, GL_RGBA, 1, qfalse, GL_REPEAT );
	
	// load the last saveimage
	R_UpdateSaveGameImage("u:\\saveimage.xbx");
#else
	tr.whiteImage = R_CreateImage("*white", (byte *)data, 8, 8, GL_RGBA, qfalse, qfalse, qtrue, GL_REPEAT);

	tr.screenImage = R_CreateImage("*screen", (byte *)data, 8, 8, GL_RGBA, qfalse, qfalse, qfalse, GL_REPEAT );
#endif


#ifndef _XBOX	// GLOWXXX
	// Create the scene glow image. - AReis
	tr.screenGlow = 1024 + giTextureBindNum++;
	qglDisable( GL_TEXTURE_2D );
	qglEnable( GL_TEXTURE_RECTANGLE_EXT );
	qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.screenGlow );
	qglTexImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA16, glConfig.vidWidth, glConfig.vidHeight, 0, GL_RGB, GL_FLOAT, 0 );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	// Create the scene image. - AReis
	tr.sceneImage = 1024 + giTextureBindNum++;
	qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.sceneImage );
	qglTexImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA16, glConfig.vidWidth, glConfig.vidHeight, 0, GL_RGB, GL_FLOAT, 0 );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	// Create the minimized scene blur image.
	if ( r_DynamicGlowWidth->integer > glConfig.vidWidth  )
	{
		r_DynamicGlowWidth->integer = glConfig.vidWidth;
	}
	if ( r_DynamicGlowHeight->integer > glConfig.vidHeight  )
	{
		r_DynamicGlowHeight->integer = glConfig.vidHeight;
	}
	tr.blurImage = 1024 + giTextureBindNum++;
	qglBindTexture( GL_TEXTURE_RECTANGLE_EXT, tr.blurImage );
	qglTexImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA16, r_DynamicGlowWidth->integer, r_DynamicGlowHeight->integer, 0, GL_RGB, GL_FLOAT, 0 );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	qglTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	qglDisable( GL_TEXTURE_RECTANGLE_EXT );
	qglEnable( GL_TEXTURE_2D );


	// with overbright bits active, we need an image which is some fraction of full color,
	// for default lightmaps, etc
	for (x=0 ; x<DEFAULT_SIZE ; x++) {
		for (y=0 ; y<DEFAULT_SIZE ; y++) {
			data[y][x][0] = 
			data[y][x][1] = 
			data[y][x][2] = tr.identityLightByte;
			data[y][x][3] = 255;			
		}
	}
#endif	// _XBOX

#ifdef _XBOX
	tr.identityLightImage = R_CreateImage("*identityLight", (byte *)data, 8, 8, GL_RGBA, qfalse, qfalse, GL_REPEAT);
#else
	tr.identityLightImage = R_CreateImage("*identityLight", (byte *)data, 8, 8, GL_RGBA, qfalse, qfalse, qtrue, GL_REPEAT);
#endif

	// scratchimage is usually used for cinematic drawing
	for(x=0;x<NUM_SCRATCH_IMAGES;x++) {
		// scratchimage is usually used for cinematic drawing
#ifdef _XBOX
		tr.scratchImage[x] = R_CreateImage(va("*scratch%d",x), (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, GL_RGBA, qfalse, qfalse, GL_CLAMP);
#else
		tr.scratchImage[x] = R_CreateImage(va("*scratch%d",x), (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, GL_RGBA, qfalse, qfalse, qfalse, GL_CLAMP);
#endif
	}

	R_CreateDlightImage();
	R_CreateFogImage();
}


/*
===============
R_SetColorMappings
===============
*/
void R_SetColorMappings( void ) {
	int		i, j;
	float	g;
	int		inf;
	int		shift;

	// setup the overbright lighting
	tr.overbrightBits = r_overBrightBits->integer;
	if ( !glConfig.deviceSupportsGamma ) {
		tr.overbrightBits = 0;		// need hardware gamma for overbright
	}

	// never overbright in windowed mode
	if ( !glConfig.isFullscreen )
	{
		tr.overbrightBits = 0;
	}

	if ( tr.overbrightBits > 1 ) {
		tr.overbrightBits = 1;
	}
	if ( tr.overbrightBits < 0 ) {
		tr.overbrightBits = 0;
	}

	tr.identityLight = 1.0 / ( 1 << tr.overbrightBits );
	tr.identityLightByte = 255 * tr.identityLight;


	if ( r_intensity->value < 1.0f ) {
		ri.Cvar_Set( "r_intensity", "1.0" );
	}

	if ( r_gamma->value < 0.5f ) {
		ri.Cvar_Set( "r_gamma", "0.5" );
	} else if ( r_gamma->value > 3.0f ) {
		ri.Cvar_Set( "r_gamma", "3.0" );
	}

	g = r_gamma->value;

	shift = tr.overbrightBits;

	for ( i = 0; i < 256; i++ ) {
		if ( g == 1 ) {
			inf = i;
		} else {
			inf = 255 * pow ( i/255.0f, 1.0f / g ) + 0.5f;
		}
		inf <<= shift;
		if (inf < 0) {
			inf = 0;
		}
		if (inf > 255) {
			inf = 255;
		}
		s_gammatable[i] = inf;
	}

	for (i=0 ; i<256 ; i++) {
		j = i * r_intensity->value;
		if (j > 255) {
			j = 255;
		}
		s_intensitytable[i] = j;
	}

#ifndef _XBOX
	if ( glConfig.deviceSupportsGamma )
	{
		GLimp_SetGamma( s_gammatable, s_gammatable, s_gammatable );
	}
#endif
}

/*
===============
R_InitImages
===============
*/
void	R_InitImages( void ) {
	//memset(hashTable, 0, sizeof(hashTable));	// DO NOT DO THIS NOW (because of image cacheing)	-ste.

	// build brightness translation tables
	R_SetColorMappings();

	// create default texture and white texture
	R_CreateBuiltinImages();
}

/*
===============
R_DeleteTextures
===============
*/
// (only gets called during vid_restart now (and app exit), not during map load)
//
void R_DeleteTextures( void ) {

	R_Images_Clear();	
	GL_ResetBinds();
}

/*
============================================================================

SKINS

============================================================================
*/

/*
==================
CommaParse

This is unfortunate, but the skin files aren't
compatable with our normal parsing rules.
==================
*/
static char *CommaParse( char **data_p ) {
	int c = 0, len;
	char *data;
	static	char	com_token[MAX_TOKEN_CHARS];

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	// make sure incoming data is valid
	if ( !data ) {
		*data_p = NULL;
		return com_token;
	}

	while ( 1 ) {
		// skip whitespace
		while( (c = *data) <= ' ') {
			if( !c ) {
				break;
			}
			data++;
		}


		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' )
		{
			while (*data && *data != '\n')
				data++;
		}
		// skip /* */ comments
		else if ( c=='/' && data[1] == '*' ) 
		{
			while ( *data && ( *data != '*' || data[1] != '/' ) ) 
			{
				data++;
			}
			if ( *data ) 
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	if ( c == 0 ) {
		return "";
	}

	// handle quoted strings
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				*data_p = ( char * ) data;
				return com_token;
			}
			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c>32 && c != ',' );

	if (len == MAX_TOKEN_CHARS)
	{
//		Com_Printf ("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		len = 0;
	}
	com_token[len] = 0;

	*data_p = ( char * ) data;
	return com_token;
}

/*
class CStringComparator
{
public:
	bool operator()(const char *s1, const char *s2) const { return(stricmp(s1, s2) < 0); } 
};
*/
typedef map<sstring_t,char * /*, CStringComparator*/ >	AnimationCFGs_t;
													AnimationCFGs_t AnimationCFGs;

// I added this function for development purposes (and it's VM-safe) so we don't have problems
//	with our use of cached models but uncached animation.cfg files (so frame sequences are out of sync
//	if someone rebuild the model while you're ingame and you change levels)...
//
// Usage:  call with psDest == NULL for a size enquire (for malloc), 
//				then with NZ ptr for it to copy to your supplied buffer...
//
int RE_GetAnimationCFG(const char *psCFGFilename, char *psDest, int iDestSize)
{
	char *psText = NULL;

	AnimationCFGs_t::iterator it = AnimationCFGs.find(psCFGFilename);
	if (it != AnimationCFGs.end())
	{
		psText = (*it).second;
	}
	else
	{
		// not found, so load it...
		//
		fileHandle_t f;
		int iLen = ri.FS_FOpenFileRead( psCFGFilename, &f, FS_READ );
		if (iLen <= 0)
		{
			return 0;
		}

		psText = (char *) ri.Z_Malloc( iLen+1, TAG_ANIMATION_CFG, qfalse, 4 );

		ri.FS_Read( psText, iLen, f );
		psText[iLen] = '\0';
		ri.FS_FCloseFile( f );

		AnimationCFGs[psCFGFilename] = psText;
	}

	if (psText)	// sanity, but should always be NZ
	{
		if (psDest)
		{
			Q_strncpyz(psDest,psText,iDestSize);
		}

		return strlen(psText);
	}

	return 0;
}

// only called from devmapbsp, devmapall, or ...
//
void RE_AnimationCFGs_DeleteAll(void)
{
	for (AnimationCFGs_t::iterator it = AnimationCFGs.begin(); it != AnimationCFGs.end(); ++it)
	{
		char *psText = (*it).second;
		Z_Free(psText);
	}

	AnimationCFGs.clear();
}

/*
===============
RE_SplitSkins
input = skinname, possibly being a macro for three skins
return= true if three part skins found
output= qualified names to three skins if return is true, undefined if false
===============
*/
bool RE_SplitSkins(const char *INname, char *skinhead, char *skintorso, char *skinlower)
{	//INname= "models/players/jedi_tf/|head01_skin1|torso01|lower01";
	if (strchr(INname, '|'))
	{
		char name[MAX_QPATH];
		strcpy(name, INname);
		char *p = strchr(name, '|');
		*p=0;
		p++;
		//fill in the base path
		strcpy (skinhead, name);
		strcpy (skintorso, name);
		strcpy (skinlower, name);

		//now get the the individual files
		
		//advance to second
		char *p2 = strchr(p, '|'); 
		assert(p2);
		*p2=0;
		p2++;
		strcat (skinhead, p);
		strcat (skinhead, ".skin");


		//advance to third
		p = strchr(p2, '|');
		assert(p);
		if (!p)
		{
			return false;
		}
		*p=0;
		p++;
		strcat (skintorso,p2);
		strcat (skintorso, ".skin");

		strcat (skinlower,p);
		strcat (skinlower, ".skin");
		
		return true;
	}
	return false;
}


qhandle_t RE_RegisterIndividualSkin( const char *name , qhandle_t hSkin);
/*
===============
RE_RegisterSkin

===============
*/
qhandle_t RE_RegisterSkin( const char *name) {
	qhandle_t	hSkin;
	skin_t		*skin;

//	if (!cls.cgameStarted && !cls.uiStarted)		
//	{
		//rww - added uiStarted exception because we want ghoul2 models in the menus.
		// gwg well we need our skins to set surfaces on and off, so we gotta get em
		//return 1;	// cope with Ghoul2's calling-the-renderer-before-its-even-started hackery, must be any NZ amount here to trigger configstring setting
//	}

	if (!tr.numSkins)
	{
		R_InitSkins(); //make sure we have numSkins set to at least one.
	}

	if ( !name || !name[0] ) {
		Com_Printf( "Empty name passed to RE_RegisterSkin\n" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		Com_Printf( "Skin name exceeds MAX_QPATH\n" );
		return 0;
	}

	// see if the skin is already loaded
	for ( hSkin = 1; hSkin < tr.numSkins ; hSkin++ ) {
		skin = tr.skins[hSkin];
		if ( !Q_stricmp( skin->name, name ) ) {
			if( skin->numSurfaces == 0 ) {
				return 0;		// default skin
			}
			return(hSkin);
		}
	}

	if ( tr.numSkins == MAX_SKINS )	{
		VID_Printf( PRINT_WARNING, "WARNING: RE_RegisterSkin( '%s' ) MAX_SKINS hit\n", name );
		return 0;
	}
	// allocate a new skin
	tr.numSkins++;
	skin = (skin_t*) Hunk_Alloc( sizeof( skin_t ), qtrue );
	tr.skins[hSkin] = skin;
	Q_strncpyz( skin->name, name, sizeof( skin->name ) );	//always make one so it won't search for it again

	// If not a .skin file, load as a single shader	- then return
	if ( strcmp( name + strlen( name ) - 5, ".skin" ) ) {
/*		skin->numSurfaces = 1;
		skin->surfaces[0] = (skinSurface_t *) Hunk_Alloc( sizeof(skin->surfaces[0]), qtrue );
		skin->surfaces[0]->shader = R_FindShader( name, lightmapsNone, stylesDefault, qtrue );
		return hSkin;
*/
	}

	char skinhead[MAX_QPATH]={0};
	char skintorso[MAX_QPATH]={0};
	char skinlower[MAX_QPATH]={0};
	if ( RE_SplitSkins(name, (char*)&skinhead, (char*)&skintorso, (char*)&skinlower ) )
	{//three part
		hSkin = RE_RegisterIndividualSkin(skinhead, hSkin);
		if (hSkin)
		{
			hSkin = RE_RegisterIndividualSkin(skintorso, hSkin);
			if (hSkin)
			{
				hSkin = RE_RegisterIndividualSkin(skinlower, hSkin);
			}
		}
	}
	else
	{//single skin
		hSkin = RE_RegisterIndividualSkin(name, hSkin);
	}
	return(hSkin);
}

// given a name, go get the skin we want and return
qhandle_t RE_RegisterIndividualSkin( const char *name , qhandle_t hSkin) 
{
	skin_t		*skin;
	skinSurface_t	*surf;
	char		*text, *text_p;
	char		*token;
	char		surfName[MAX_QPATH];

	// load and parse the skin file
    ri.FS_ReadFile( name, (void **)&text );
	if ( !text ) {
		VID_Printf( PRINT_ERROR, "WARNING: RE_RegisterSkin( '%s' ) failed to load!\n", name );
		return 0;
	}

	assert (tr.skins[hSkin]);	//should already be setup, but might be an 3part append

	skin = tr.skins[hSkin];

	text_p = text;
	while ( text_p && *text_p ) {
		// get surface name
		token = CommaParse( &text_p );
		Q_strncpyz( surfName, token, sizeof( surfName ) );

		if ( !token[0] ) {
			break;
		}
		// lowercase the surface name so skin compares are faster
		Q_strlwr( surfName );

		if ( *text_p == ',' ) {
			text_p++;
		}

		if ( !strncmp( token, "tag_", 4 ) ) {	//these aren't in there, but just in case you load an id style one...
			continue;
		}
		
		// parse the shader name
		token = CommaParse( &text_p );

		if ( !strcmp( &surfName[strlen(surfName)-4], "_off") )
		{
			if ( !strcmp( token ,"*off" ) )
			{
				continue;	//don't need these double offs
			}
			surfName[strlen(surfName)-4] = 0;	//remove the "_off"
		}
		if (sizeof( skin->surfaces) / sizeof( skin->surfaces[0] ) <= skin->numSurfaces)
		{
			assert( sizeof( skin->surfaces) / sizeof( skin->surfaces[0] ) > skin->numSurfaces );
			VID_Printf( PRINT_ERROR, "WARNING: RE_RegisterSkin( '%s' ) more than %d surfaces!\n", name, sizeof( skin->surfaces) / sizeof( skin->surfaces[0] ) );
			break;
		}
		surf = skin->surfaces[ skin->numSurfaces ] = (skinSurface_t *) Hunk_Alloc( sizeof( *skin->surfaces[0] ), qtrue );
		Q_strncpyz( surf->name, surfName, sizeof( surf->name ) );
		surf->shader = R_FindShader( token, lightmapsNone, stylesDefault, qtrue );
		skin->numSurfaces++;
	}

	ri.FS_FreeFile( text );


	// never let a skin have 0 shaders
	if ( skin->numSurfaces == 0 ) {
		return 0;		// use default skin
	}

	return hSkin;
}


/*
===============
R_InitSkins
===============
*/
void	R_InitSkins( void ) {
	skin_t		*skin;

	tr.numSkins = 1;

	// make the default skin have all default shaders
	skin = tr.skins[0] = (skin_t*) Hunk_Alloc( sizeof( skin_t ), qtrue );
	Q_strncpyz( skin->name, "<default skin>", sizeof( skin->name )  );
	skin->numSurfaces = 1;
	skin->surfaces[0] = (skinSurface_t *) Hunk_Alloc( sizeof( *skin->surfaces[0] ), qtrue );
	skin->surfaces[0]->shader = tr.defaultShader;
}

/*
===============
R_GetSkinByHandle
===============
*/
skin_t	*R_GetSkinByHandle( qhandle_t hSkin ) {
	if ( hSkin < 1 || hSkin >= tr.numSkins ) {
		return tr.skins[0];
	}
	return tr.skins[ hSkin ];
}

/*
===============
R_SkinList_f
===============
*/
void	R_SkinList_f (void) {
	int			i, j;
	skin_t		*skin;

	VID_Printf (PRINT_ALL, "------------------\n");

	for ( i = 0 ; i < tr.numSkins ; i++ ) {
		skin = tr.skins[i];
		VID_Printf( PRINT_ALL, "%3i:%s\n", i, skin->name );
		for ( j = 0 ; j < skin->numSurfaces ; j++ ) {
			VID_Printf( PRINT_ALL, "       %s = %s\n", 
				skin->surfaces[j]->name, skin->surfaces[j]->shader->name );
		}
	}
	VID_Printf (PRINT_ALL, "------------------\n");
}

#ifdef _XBOX

extern BOOL LoadCompressedScreenshot(const char* filename);

/*
===============
R_UpdateSaveGameImage
filename	- .xbx format file with a screenshot
===============
*/
void R_UpdateSaveGameImage(const char* filename)
{
	// bind the savegame image
	GL_Bind(tr.saveGameImage);

	// replace the texture with the one from the file
	LoadCompressedScreenshot(filename);
}

#endif // _XBOX

