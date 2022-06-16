#ifndef __SYNTHESIS__
#include <stdio.h>
#endif
#include <assert.h>
#include "v_hscaler_config.h"
#include "v_hscaler.h"
#include <algorithm>

//#define DBG_PRINT

const U8 rgb = 0;
const U8 yuv444 = 1;
const U8 yuv422 = 2;
const U8 yuv420 = 3;

#if  (HSC_SCALE_MODE == HSC_BICUBIC)
void hscale_core_bicubic (HSC_STREAM_MULTIPIX& SrcImg,
                          U16 &Height,
                          U16 &WidthIn,
                          U16 &WidthOut,
                          U32 &PixelRate,
                          U8 &ColorMode,
                          HSC_PHASE_CTRL phasesH[HSC_MAX_WIDTH/HSC_SAMPLES_PER_CLOCK],
                          HSC_STREAM_MULTIPIX& OutImg
                          );

void hscale_bicubic  (YUV_PIXEL PixArray[HSC_ARRAY_SIZE],
                      U16 PhasesH[HSC_SAMPLES_PER_CLOCK],
                      U8 ArrayIdx[HSC_SAMPLES_PER_CLOCK],
                      YUV_MULTI_PIXEL *OutPix
                      );
#endif

#if  (HSC_SCALE_MODE == HSC_POLYPHASE)
void hscale_core_polyphase(HSC_STREAM_MULTIPIX& SrcImg,
                 	 	   U16 &Height,
                 	 	   U16 &WidthIn,
                 	 	   U16 &WidthOut,
                 	 	   U32 &PixelRate,
                 	 	   U8 &ColorMode,
                 	 	   I16 hfltCoeff[HSC_PHASES][HSC_TAPS],
                           HSC_PHASE_CTRL phasesH[HSC_MAX_WIDTH/HSC_SAMPLES_PER_CLOCK],
                 	 	   HSC_STREAM_MULTIPIX& OutImg
                 	 	   );

void hscale_polyphase (I16 FiltCoeff[HSC_PHASES][HSC_TAPS][HSC_SAMPLES_PER_CLOCK],
        			   YUV_PIXEL PixArray[HSC_ARRAY_SIZE],
                       U16 PhasesH[HSC_SAMPLES_PER_CLOCK],
                       U8 ArrayIdx[HSC_SAMPLES_PER_CLOCK],
                       YUV_MULTI_PIXEL *OutPix
                      );
#endif

#if  (HSC_SCALE_MODE == HSC_BILINEAR)
void hscale_core_bilinear(HSC_STREAM_MULTIPIX& SrcImg,
                          U16 &Height,
                          U16 &WidthIn,
                          U16 &WidthOut,
                          U32 &PixelRate,
                          U8 &ColorMode,
                          HSC_PHASE_CTRL phasesH[HSC_MAX_WIDTH/HSC_SAMPLES_PER_CLOCK],
                          HSC_STREAM_MULTIPIX& OutImg
                          );

void hscale_bilinear (YUV_PIXEL PixArray[HSC_ARRAY_SIZE],
                      U16 PhasesH[HSC_SAMPLES_PER_CLOCK],
                      U8 ArrayIdx[HSC_SAMPLES_PER_CLOCK],
                      YUV_MULTI_PIXEL *OutPix
                      );
#endif

static int AXIvideo2MultiPixStream(HSC_AXI_STREAM_IN& AXI_video_strm,
                                   HSC_STREAM_MULTIPIX& img,
                                   U16 &Height, U16 &WidthIn,
                                   U8 &ColorMode
                                   );

static int MultiPixStream2AXIvideo(HSC_STREAM_MULTIPIX& StrmMPix,
                                   HSC_AXI_STREAM_OUT& AXI_video_strm,
                                   U16 &Height,
                                   U16 &Width,
                                   U8 &ColorMode
                                   );

static void v_hcresampler_core(HSC_STREAM_MULTIPIX& srcImg,
                               U16 &height,
                               U16 &width,
                               const U8 &colorMode,
                               bool &bPassThru,
                               HSC_STREAM_MULTIPIX& outImg);

void v_vcresampler_core(HSC_STREAM_MULTIPIX& srcImg,
                        U16 &height,
                        U16 &width,
						const U8 &inColorMode,
                        bool &bPassThru,
						HSC_STREAM_MULTIPIX& outImg);

static void v_csc_core(
	HSC_STREAM_MULTIPIX& srcImg,
    U16 &height,
    U16 &width,
    U8 &colorMode,
    bool &bPassThru,
	HSC_STREAM_MULTIPIX& outImg);

/*********************************************************************************
* Function:    hscale_top
* Parameters:  Stream of input/output pixels, image resolution, type of scaling etc
* Return:
* Description: Top level function to perform horizontal image resizing
* submodules - AXIvideo2MultiPixStream
*              hscale_core
*              MultiPixStream2AXIvideo
**********************************************************************************/
void v_hscaler  (HSC_AXI_STREAM_IN& s_axis_video,
           	 	U16 &Height,
		        U16 &WidthIn,
	         	U16 &WidthOut,
		        U8 &ColorMode,
				U32 &PixelRate,
		        U8 &ColorModeOut,
				I16 hfltCoeff[HSC_PHASES][HSC_TAPS],
				HSC_PHASE_CTRL phasesH[HSC_MAX_WIDTH/HSC_SAMPLES_PER_CLOCK],
                HSC_AXI_STREAM_OUT& m_axis_video
                )
{

__xilinx_ip_top(0);

#pragma HLS INTERFACE axis port=&s_axis_video register
#pragma HLS INTERFACE axis port=&m_axis_video register

#pragma HLS INTERFACE s_axilite port=return bundle=CTRL
#pragma HLS INTERFACE s_axilite port=&Height bundle=CTRL
#pragma HLS INTERFACE s_axilite port=&WidthIn bundle=CTRL
#pragma HLS INTERFACE s_axilite port=&WidthOut bundle=CTRL
#pragma HLS INTERFACE s_axilite port=&ColorMode bundle=CTRL
#pragma HLS INTERFACE s_axilite port=&PixelRate bundle=CTRL
#pragma HLS INTERFACE s_axilite port=&ColorModeOut bundle=CTRL
#if (HSC_SCALE_MODE == HSC_POLYPHASE)
#pragma HLS INTERFACE s_axilite port=&hfltCoeff bundle=CTRL offset=0x800
#endif
#pragma HLS INTERFACE s_axilite port=&phasesH bundle=CTRL offset=0x4000

#pragma HLS RESOURCE variable=&phasesH core=RAM_1P
#if (HSC_SCALE_MODE == HSC_POLYPHASE)
#pragma HLS RESOURCE variable=&hfltCoeff core=RAM_1P
#endif

    bool bPassThruHcr1 = true;
    bool bPassThruVcr = true;
    bool bPassThruHcr2 = true;
    bool bPassThruCsc = true;

 	assert ((Height > 0) && (WidthIn > 0));
    assert ((Height <= HSC_MAX_HEIGHT) && (WidthOut <= HSC_MAX_WIDTH));
    assert (WidthIn <= HSC_MAX_WIDTH);

#if (HSC_ENABLE_422==1)
	bPassThruHcr1 = (ColorMode!=yuv422) ? true : false;
#endif

#if (HSC_ENABLE_CSC==1)
	bPassThruCsc = ((ColorMode==rgb && ColorModeOut!=rgb) || (ColorMode!=rgb && ColorModeOut==rgb)) ? false : true;
#endif

#if (HSC_ENABLE_422==1)
	bPassThruHcr2 = (ColorModeOut==yuv422 || ColorModeOut==yuv420) ? false : true;
#endif

#if (HSC_ENABLE_420==1)
	bPassThruVcr = (ColorModeOut==yuv420) ? false : true;
#endif
	  HSC_STREAM_MULTIPIX stream_in;
	  HSC_STREAM_MULTIPIX stream_upsampled;
	  HSC_STREAM_MULTIPIX stream_scaled;
	  HSC_STREAM_MULTIPIX stream_scaled_csc;
	  HSC_STREAM_MULTIPIX stream_out_422;
	  HSC_STREAM_MULTIPIX stream_out_420;

	#pragma HLS INTERFACE ap_stable port=Height
	#pragma HLS INTERFACE ap_stable port=WidthIn
	#pragma HLS INTERFACE ap_stable port=WidthOut
	#pragma HLS INTERFACE ap_stable port=PixelRate
	#pragma HLS INTERFACE ap_stable port=ColorModeOut

	#pragma HLS DATAFLOW

	#pragma HLS stream variable=stream_in depth=16
	#pragma HLS stream variable=stream_upsampled depth=16
	#pragma HLS stream variable=stream_scaled depth=16
	#pragma HLS stream variable=stream_scaled_csc depth=16
	#pragma HLS stream variable=stream_out_422 depth=16
	#pragma HLS stream variable=stream_out_420 depth=16

	    //convert AXI Video Stream to Mat Image
	    AXIvideo2MultiPixStream(s_axis_video, stream_in, Height, WidthIn, ColorMode);

		#if (HSC_ENABLE_422==1)
			//bPassThruHcr = (ColorMode!=yuv422) ? true : false;
			v_hcresampler_core(stream_in, Height, WidthIn, yuv422, bPassThruHcr1, stream_upsampled);
			#define STREAM_IN stream_upsampled
		#else
			#define STREAM_IN stream_in
		#endif

	    #if (HSC_SCALE_MODE == HSC_BILINEAR)
	        hscale_core_bilinear(STREAM_IN, Height, WidthIn, WidthOut, PixelRate, ColorMode, phasesH, stream_scaled);
		#endif

	    #if (HSC_SCALE_MODE == HSC_POLYPHASE)
	    	 hscale_core_polyphase(STREAM_IN, Height, WidthIn, WidthOut, PixelRate, ColorMode, hfltCoeff, phasesH, stream_scaled);
	    #endif

		#if(HSC_SCALE_MODE == HSC_BICUBIC)
	         hscale_core_bicubic(STREAM_IN, Height, WidthIn, WidthOut, PixelRate, ColorMode, phasesH, stream_scaled);
	    #endif

		#if (HSC_ENABLE_CSC==1)
			//bPassThruCsc = ((ColorMode==rgb && ColorModeOut!=rgb) || (ColorMode!=rgb && ColorModeOut==rgb)) ? false : true;
			v_csc_core(stream_scaled, Height, WidthOut, ColorMode, bPassThruCsc, stream_scaled_csc);
			#define STREAM_OUT_CSC stream_scaled_csc
		#else
			#define STREAM_OUT_CSC stream_scaled
		#endif

		#if (HSC_ENABLE_422==1)
			//bPassThruHcr = (ColorModeOut==yuv422 || ColorModeOut==yuv420) ? false : true;
			v_hcresampler_core(STREAM_OUT_CSC, Height, WidthOut, yuv444, bPassThruHcr2, stream_out_422);
			#define STREAM_OUT_422 stream_out_422
		#else
			#define STREAM_OUT_422 STREAM_OUT_CSC
		#endif

		#if (HSC_ENABLE_420==1)
			//bPassThruVcr = (ColorModeOut==yuv420) ? false : true;
			v_vcresampler_core(STREAM_OUT_422, Height, WidthOut, yuv422, bPassThruVcr, stream_out_420);
			#define STREAM_OUT stream_out_420
		#else
			#define STREAM_OUT STREAM_OUT_422
		#endif

	    MultiPixStream2AXIvideo(STREAM_OUT, m_axis_video,  Height, WidthOut, ColorModeOut);
}

#if  (HSC_SCALE_MODE == HSC_BILINEAR)

/*********************************************************************************
* Function:    hscale_core
* Parameters:  Stream of input/output pixels, image resolution, type of scaling etc
* Return:
* Description: Perform horizontal image resizing
* Sub modules - hscale_linear
*               hscale_cubic
*               hscale_poly
**********************************************************************************/
void hscale_core_bilinear (HSC_STREAM_MULTIPIX& SrcImg,
                           U16 &Height,
                           U16 &InPixels,
                           U16 &OutPixels,
                           U32 &Rate,
                           U8 &RegColorMode,
                           HSC_PHASE_CTRL arrPhasesH[HSC_MAX_WIDTH/HSC_SAMPLES_PER_CLOCK],
                           HSC_STREAM_MULTIPIX& OutImg
                           )
{

    ap_uint<HSC_SAMPLES_PER_CLOCK> OutputWriteEn;

    U8    ArrayLoc[HSC_SAMPLES_PER_CLOCK];
    U16  xReadPos, xWritePos;
    U16 y, x;
    U16 x_dsent;
    bool WriteEn; //?
    U8  PackCnt;
    U8  ComputedPixels;

    YUV_MULTI_PIXEL SrcPix, OutPix, OutPixPrv;
    YUV_PIXEL PixArray[HSC_ARRAY_SIZE];
    YUV_MULTI_PIXEL OutMultiPix;

    assert(Height <= HSC_MAX_HEIGHT);
    assert(Height >= MIN_PIXELS);

    U16 TotalPixels  = std::max(OutPixels, InPixels);
    assert(TotalPixels <= HSC_MAX_WIDTH);
    assert(TotalPixels >= MIN_PIXELS);

    U16 LoopSize     = TotalPixels + HSC_ARRAY_SIZE - HSC_SAMPLES_PER_CLOCK;

    assert((LoopSize%HSC_SAMPLES_PER_CLOCK) == 0);

#ifdef DBG_PRINT
    int dbgNrRds =0;
    int dbgNrWrs =0;
#endif

#pragma HLS ARRAY_PARTITION variable=&SrcPix      complete dim=0
#pragma HLS ARRAY_PARTITION variable=&PixArray    complete dim=0
#pragma HLS ARRAY_PARTITION variable=&OutPix      complete dim=0
#pragma HLS ARRAY_PARTITION variable=&ArrayLoc   complete dim=0
#pragma HLS ARRAY_PARTITION variable=&OutMultiPix  complete dim=0
#pragma HLS ARRAY_PARTITION variable=&OutPixPrv  complete dim=0

loop_height:
    for (y = 0; y < Height; y++)
    {
        int ReadEn = 1;
        xReadPos       = 0;
        xWritePos      = 0;
        x_dsent        = 0;
        PackCnt        = 0;
        OutputWriteEn  = 0;

        U16 PhaseH[HSC_SAMPLES_PER_CLOCK];

        U8 nrWrsIdxPrev = 0;
        U8 nrWrsPrev = 0;
        U8 nrWrsClck = 0;
        U8 nrWrsAccu = 0;
loop_width:
        for (x = 0; x <= LoopSize; x += HSC_SAMPLES_PER_CLOCK) // the loop runs for the max of (in,out)
        {
            //_Pragma("HLS loop_tripcount max=((MSC_MAX_WIDTH+HSC_ARRAY_SIZE-HSC_SAMPLES_PER_CLOCK)/HSC_SAMPLES_PER_CLOCK)")
#pragma HLS LOOP_FLATTEN OFF
#pragma HLS PIPELINE

        	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        	//
        	// Pixel buffer control
        	//
        	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            if (ReadEn)   // If new pixels to be got, shift the old ones
            {
				// Shift the pixels in the array over by HSC_SAMPLES_PER_CLOCK
                for (int i = 0; i <= (HSC_ARRAY_SIZE-1-HSC_SAMPLES_PER_CLOCK); i++)
                {
					PixArray[i] = PixArray[i+HSC_SAMPLES_PER_CLOCK]; // always shift out the same number of pixels as samples per clock
                }
                // Read new pixels if we didn't exceed the width of frame yet
                if ((xReadPos + HSC_ARRAY_SIZE-1) < InPixels)
                {
                	// Get new pixels from stream and insert
          #ifdef DBG_PRINT
                	dbgNrRds++;
          #endif
                    SrcImg >> SrcPix;
                    for (int i = 0; i < HSC_SAMPLES_PER_CLOCK; i++)
                    {
                        for(int k=0; k < HSC_NR_COMPONENTS; k++)
                        {
                            PixArray[HSC_ARRAY_SIZE-1-HSC_SAMPLES_PER_CLOCK+1+i].val[k] = SrcPix.val[HSC_NR_COMPONENTS*i + k];
                        }
                    }
                }
                else
                {
                	// Right border handling
                    for (int i = 0; i < HSC_SAMPLES_PER_CLOCK; i++)
                    {
                        PixArray[HSC_ARRAY_SIZE-1-HSC_SAMPLES_PER_CLOCK+1+i] = PixArray[HSC_ARRAY_SIZE-1-HSC_SAMPLES_PER_CLOCK];
                    }

                }
            }

#ifdef DBG_PRINT
if ((y==0) && (ReadEn))
{
	for (int i = 0; i < (HSC_ARRAY_SIZE); i++)
	{
		printf("[%2d] %3d.%3d.%3d ", (int)i, (int)PixArray[i].val[0], (int)PixArray[i].val[1], (int)PixArray[i].val[2]);
	}
	printf("\n");
}
#endif
            if (x >= (HSC_ARRAY_SIZE-HSC_SAMPLES_PER_CLOCK))
            {
loop_samples:
				U16 xbySamples = x-(HSC_ARRAY_SIZE-HSC_SAMPLES_PER_CLOCK);
				xbySamples = xbySamples/HSC_SAMPLES_PER_CLOCK;
                // Note 'reg()' below.  Under most circumstances, this is not the preferred way to do this.  In most cases,
                // HLS supports the ability to set the latency of reads from a memory.  However, in this case, because the
                // memory is part of the axi_lite adapter, that doesn't work.
                HSC_PHASE_CTRL phases = reg(arrPhasesH[xbySamples]);
                for (int s = 0; s < HSC_SAMPLES_PER_CLOCK; s++)
                {
                	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                	//
                    // Step and phase calculation
                	//
                	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                    PhaseH[s]        = phases(HSC_PHASE_CTRL_PHASE_MSB + s*HSC_PHASE_CTRL_BITS, HSC_PHASE_CTRL_PHASE_LSB + s*HSC_PHASE_CTRL_BITS);
                    ArrayLoc[s]      = phases(HSC_PHASE_CTRL_INDEX_MSB + s*HSC_PHASE_CTRL_BITS, HSC_PHASE_CTRL_INDEX_LSB + s*HSC_PHASE_CTRL_BITS);
                    OutputWriteEn[s] = phases[                                                  HSC_PHASE_CTRL_ENABLE_LSB + s*HSC_PHASE_CTRL_BITS];
                }
                ReadEn = (ArrayLoc[HSC_SAMPLES_PER_CLOCK-1]>=HSC_SAMPLES_PER_CLOCK);

#ifdef DBG_PRINT
                if (y==0)
                {
                	for (int s=0; s<HSC_SAMPLES_PER_CLOCK; s++)
                	{
                		printf("x %5d, phase %5d, pos %5d, readpos %5d writepos %5d\n", (int)x+s, (int)PhaseH[s], (int)ArrayLoc[s], (int)xReadPos, xWritePos);
                	}
                	printf("rden %d\n", ReadEn);
                }
#endif
				hscale_bilinear (PixArray, PhaseH, ArrayLoc, &OutPix);

                if (ReadEn)
                {
                    xReadPos += HSC_SAMPLES_PER_CLOCK;
                }

				#if (HSC_SAMPLES_PER_CLOCK==1)
                {
                	if (OutputWriteEn && (x < LoopSize))
                	{
                        OutImg << OutPix;
#ifdef DBG_PRINT
                        dbgNrWrs++;
#endif
                	}
                }
				#else

#if (HSC_SAMPLES_PER_CLOCK == 8)

                const U8 BitSetCnt[] =
                                              { 0,  1,  1,  2,	1,	2,	2,	3,	1,	2,	2,	3,	2,	3,	3,	4,
                                            	1,	2,	2,	3,	2,	3,	3,	4,	2,	3,	3,	4,	3,	4,	4,	5,
                								1,	2,	2,	3,	2,	3,	3,	4,	2,	3,  3,	4,	3,	4,	4,	5,
                								2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
                								1,	2,	2,	3,	2,	3,	3,	4,	2,	3,	3,	4,	3,	4,	4,	5,
                								2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
                								2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
                								3,	4,	4,	5,	4,	5,	5,	6,	4,	5,	5,	6,	5,	6,	6,	7,
                								1,	2,	2,	3,	2,	3,	3,	4,	2,	3,	3,	4,	3,	4,	4,	5,
                								2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
                								2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
                								3,	4,	4,	5,	4,	5,	5,	6,	4,	5,  5,	6,	5,	6,	6,	7,
                								2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
                								3,	4,	4,	5,	4,	5,	5,	6,	4,	5,	5,	6,	5,	6,	6,	7,
                								3,	4,	4,	5,	4,	5,	5,	6,	4,	5,	5,	6,	5,	6,	6,	7,
                								4,	5,	5,	6,	5,	6,	6,	7,	5,	6,	6,	7,	6,	7,	7,	8
                                              }; // 2^ MAX_SMPLS_PERCLK
#else
                const U8 BitSetCnt[] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4}; // 2^ MAX_SMPLS_PERCLK
#endif


                 nrWrsClck = BitSetCnt[OutputWriteEn];

                // Below table provides an index to the n-th [0..3] 1 bit for numbers between [0..15]
#if (HSC_SAMPLES_PER_CLOCK == 8)

                const U8 OneBitIdx[8][256] = {
                				            {0  ,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,5	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,6	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,5	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,7	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,5	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,6	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,5      ,0      ,1      ,0      ,2      ,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0 },
                				            {0	,0	,0	,1	,0	,2	,2	,1	,0	,3	,3	,1	,3	,2	,2	,1	,0	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,0	,5	,5	,1	,5	,2	,2	,1	,5	,3	,3	,1	,3	,2	,2	,1	,5	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,0	,6	,6	,1	,6	,2	,2	,1	,6	,3	,3	,1	,3	,2	,2	,1	,6	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,6	,5	,5	,1	,5	,2	,2	,1	,5	,3	,3	,1	,3	,2	,2	,1	,5	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,0	,7	,7	,1	,7	,2	,2	,1	,7	,3	,3	,1	,3	,2	,2	,1	,7	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,7	,5	,5	,1	,5	,2	,2	,1	,5	,3	,3	,1	,3	,2	,2	,1	,5	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,7	,6	,6	,1	,6	,2	,2	,1	,6	,3	,3	,1	,3	,2	,2	,1	,6	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,6	,5	,5	,1      ,5      ,2	,2	,1	,5	,3	,3	,1	,3	,2	,2	,1	,5	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1 },
                				            {0	,0	,0	,0	,0	,0	,0	,2	,0	,0	,0	,3	,0	,3	,3	,2	,0	,0	,0	,4	,0	,4	,4	,2	,0	,4	,4	,3	,4	,3	,3	,2	,0	,0	,0	,5	,0	,5	,5	,2	,0	,5	,5	,3	,5	,3	,3	,2	,0	,5	,5	,4	,5	,4	,4	,2	,5	,4	,4	,3	,4	,3	,3	,2	,0	,0	,0	,6	,0	,6	,6	,2	,0	,6	,6	,3	,6	,3	,3	,2	,0	,6	,6	,4	,6	,4	,4	,2	,6	,4	,4	,3	,4	,3	,3	,2	,0	,6	,6	,5	,6	,5	,5	,2	,6	,5	,5	,3	,5	,3	,3	,2	,6	,5	,5	,4	,5	,4	,4	,2	,5	,4	,4	,3	,4	,3	,3	,2	,0	,0	,0	,7	,0	,7	,7	,2	,0	,7	,7	,3	,7	,3	,3	,2	,0	,7	,7	,4	,7	,4	,4	,2	,7	,4	,4	,3	,4	,3	,3	,2	,0	,7	,7	,5	,7	,5	,5	,2	,7	,5	,5	,3	,5	,3	,3	,2	,7	,5	,5	,4	,5	,4	,4	,2	,5	,4	,4	,3	,4	,3	,3	,2	,0	,7	,7	,6	,7	,6	,6	,2	,7	,6	,6	,3	,6	,3	,3	,2	,7	,6	,6	,4	,6	,4	,4	,2	,6	,4	,4	,3	,4	,3	,3	,2	,7      ,6      ,6      ,5	,6      ,5	,5	,2	,6	,5	,5	,3	,5	,3	,3	,2	,6	,5	,5	,4	,5	,4	,4	,2	,5	,4	,4	,3	,4	,3	,3	,2 },
                				            {0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,3	,0	,0	,0	,0      ,0	,0	,0	,4	,0	,0	,0	,4	,0	,4	,4	,3	,0	,0	,0	,0	,0	,0	,0	,5	,0	,0	,0	,5	,0	,5	,5	,3	,0	,0	,0	,5	,0	,5	,5	,4	,0	,5	,5	,4	,5	,4	,4	,3	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,6	,0	,6	,6	,3	,0	,0	,0	,6	,0	,6	,6	,4	,0	,6	,6	,4	,6	,4	,4	,3	,0	,0	,0	,6	,0	,6	,6	,5	,0	,6	,6	,5	,6	,5	,5	,3	,0	,6	,6	,5	,6	,5	,5	,4	,6	,5	,5	,4	,5	,4	,4	,3	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,3	,0	,0	,0	,7	,0	,7	,7	,4	,0	,7	,7	,4	,7	,4	,4	,3	,0	,0	,0	,7	,0	,7	,7	,5	,0	,7	,7	,5	,7	,5	,5	,3	,0	,7	,7	,5	,7	,5	,5	,4	,7	,5	,5	,4	,5	,4	,4	,3	,0	,0	,0	,7	,0	,7	,7	,6	,0	,7	,7	,6	,7	,6	,6	,3	,0	,7	,7	,6	,7	,6	,6	,4	,7	,6	,6	,4	,6	,4	,4	,3	,0	,7      ,7      ,6	,7      ,6	,6	,5	,7	,6	,6	,5	,6	,5	,5	,3	,7	,6	,6	,5	,6	,5	,5	,4	,6	,5	,5	,4	,5	,4	,4	,3 },
                							{0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,4	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,5	,0	,0	,0	,0	,0	,0	,0	,5	,0	,0	,0	,5	,0	,5	,5	,4	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0      ,0	,0	,0	,0	,6	,0	,0	,0	,6	,0	,6	,6	,4	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,6	,0	,6	,6	,5	,0	,0	,0	,6	,0	,6	,6	,5	,0	,6	,6	,5	,6	,5	,5	,4	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,4	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,5	,0	,0	,0	,7	,0	,7	,7	,5	,0	,7	,7	,5	,7	,5	,5	,4	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,6	,0	,0	,0	,7	,0	,7	,7	,6	,0	,7	,7	,6	,7	,6 	,6	,4	,0	,0      ,0	,7      ,0	,7	,7	,6	,0	,7	,7	,6	,7	,6	,6	,5	,0	,7	,7	,6	,7	,6	,6	,5	,7	,6	,6	,5	,6	,5	,5	,4 },
                							{0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,5	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,6	,0	,6	,6	,5	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,5	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,6	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,6	,0	,0	,0	,7	,0	,7	,7	,6	,0	,7	,7	,6	,7	,6	,6	,5 },
                							{0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0      ,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,6 },
                							{0	,0  ,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0 	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0      ,7 }
                				            };
#else
                const U8 OneBitIdx[4][16] = {
                                                               {0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0},
                                                               {0, 0, 0, 1, 0, 2, 2, 1, 0, 3, 3, 1, 3, 2, 2, 1},
                                                               {0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 3, 0, 3, 3, 2},
                                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3}
                                                           };
#endif

                // If more than HSC_SAMPLES_PER_CLOCK pixels are available we can write them out
                // This loop combines any pixels that were still waiting to be written with the pixels that were generated this clock cycle
                nrWrsAccu = nrWrsPrev+nrWrsClck;
				if ((nrWrsAccu>=HSC_SAMPLES_PER_CLOCK) && (xWritePos < OutPixels))
				{
	                for (int s = 0; s < HSC_SAMPLES_PER_CLOCK; s++)
	                {
	                	for (int c=0; c<HSC_NR_COMPONENTS; c++)
	                	{
	                		OutMultiPix.val[s*HSC_NR_COMPONENTS+c] = (s<nrWrsPrev) ? OutPixPrv.val[s*HSC_NR_COMPONENTS+c] : OutPix.val[OneBitIdx[s-nrWrsPrev][OutputWriteEn]*HSC_NR_COMPONENTS+c]; // OutPixCur.val[(s-nrWrsPrev)*HSC_NR_COMPONENTS+c];
	                	}
	                }
					OutImg << OutMultiPix;
#ifdef DBG_PRINT
					dbgNrWrs++;
#endif
					xWritePos += HSC_SAMPLES_PER_CLOCK;
				}

                // This loop combines any pixels that are still waiting to be written
                for (int s = 0; s < HSC_SAMPLES_PER_CLOCK; s++)
                {
					for (int c=0; c<HSC_NR_COMPONENTS; c++)
					{
						OutPixPrv.val[s*HSC_NR_COMPONENTS+c] = (nrWrsAccu>=HSC_SAMPLES_PER_CLOCK && s<((nrWrsAccu) % HSC_SAMPLES_PER_CLOCK)) ? OutPix.val[OneBitIdx[s+HSC_SAMPLES_PER_CLOCK-nrWrsPrev][OutputWriteEn]*HSC_NR_COMPONENTS+c] : ((s<nrWrsPrev) ? OutPixPrv.val[s*HSC_NR_COMPONENTS+c] : OutPix.val[OneBitIdx[s-nrWrsPrev][OutputWriteEn]*HSC_NR_COMPONENTS+c]);
					}
                }
                nrWrsPrev = (nrWrsAccu) % HSC_SAMPLES_PER_CLOCK;

                #endif
            }
        }
    }
#ifdef DBG_PRINT
    printf("dbgNrRds %d dbgNrWrs %d\n", dbgNrRds, dbgNrWrs);
#endif
}

void hscale_bilinear (YUV_PIXEL PixArray[HSC_ARRAY_SIZE],
                      U16 PhasesH[HSC_SAMPLES_PER_CLOCK],
                      U8 ArrayIdx[HSC_SAMPLES_PER_CLOCK],
                      YUV_MULTI_PIXEL *OutPix)

{
#pragma HLS inline
    U32 sum, norm;

    for (int s = 0; s < HSC_SAMPLES_PER_CLOCK; s++) // samples
    {
        U16 PhaseH = PhasesH[s];
        U8 idx   = ArrayIdx[s];
        U8 idxP1 = ArrayIdx[s]+1;
        for (int c = 0; c < HSC_NR_COMPONENTS; c++)
        {
            // Instead of using 2 multpliers following expression: sum = (PixArray[0].p[s].val[c]* PhaseInv) + (PixArray[1].p[s].val[c] * Phase);
            // is rewritten using just 1 multiplier
            sum = (PixArray[idx].val[c]*(HSC_PHASES)) - (PixArray[idx].val[c] - PixArray[idxP1].val[c]) * PhaseH;
            norm = (sum + (HSC_PHASES>>1)) >> HSC_PHASE_SHIFT;
            norm = CLAMP(norm, 0, (U32)((1<<HSC_BITS_PER_COMPONENT)-1));
            OutPix->val[HSC_NR_COMPONENTS*s+c] = norm;
        }
    }
}

#endif

#if  (HSC_SCALE_MODE == HSC_POLYPHASE)

/*********************************************************************************
* Function:    hscale_core
* Parameters:  Stream of input/output pixels, image resolution, type of scaling etc
* Return:
* Description: Perform horizontal image resizing
* Sub modules - hscale_linear
*               hscale_cubic
*               hscale_poly
**********************************************************************************/
void hscale_core_polyphase(HSC_STREAM_MULTIPIX& SrcImg,
                           U16 &Height,
                           U16 &InPixels,
                           U16 &OutPixels,
                           U32 &Rate,
                           U8 &RegColorMode,
                 	 	   I16 hfltCoeff[HSC_PHASES][HSC_TAPS],
                           HSC_PHASE_CTRL arrPhasesH[HSC_MAX_WIDTH/HSC_SAMPLES_PER_CLOCK],
                           HSC_STREAM_MULTIPIX& OutImg
                           )
{

    ap_uint<HSC_SAMPLES_PER_CLOCK> OutputWriteEn;

    U8 ArrayLoc[HSC_SAMPLES_PER_CLOCK];
    U16 xReadPos, xWritePos;
    U16 y, x;
    U16 x_dsent;
    U8 PackCnt;
    U8 ComputedPixels;

    YUV_MULTI_PIXEL SrcPix, OutPix, OutPixPrv;
    YUV_PIXEL PixArray[HSC_ARRAY_SIZE];
    YUV_MULTI_PIXEL OutMultiPix;
    I16 FiltCoeff[HSC_PHASES][HSC_TAPS][HSC_SAMPLES_PER_CLOCK];

    assert(Height <= HSC_MAX_HEIGHT);
    assert(Height >= MIN_PIXELS);

    U16 TotalPixels  = std::max(OutPixels, InPixels);
    assert(TotalPixels <= HSC_MAX_WIDTH);
    assert(TotalPixels >= MIN_PIXELS);

    U16 LoopSize     = TotalPixels + HSC_ARRAY_SIZE - HSC_SAMPLES_PER_CLOCK - (HSC_TAPS>>1);
    LoopSize = ((LoopSize + (HSC_SAMPLES_PER_CLOCK-1))/HSC_SAMPLES_PER_CLOCK) * (HSC_SAMPLES_PER_CLOCK);
    assert((LoopSize%HSC_SAMPLES_PER_CLOCK) == 0);
#ifdef DBG_PRINT
    int dbgNrRds =0;
    int dbgNrWrs =0;
#endif

#pragma HLS ARRAY_PARTITION variable=&SrcPix      complete dim=0
#pragma HLS ARRAY_PARTITION variable=&PixArray    complete dim=0
#pragma HLS ARRAY_PARTITION variable=&OutPix      complete dim=0
#pragma HLS ARRAY_PARTITION variable=&OutPixPrv   complete dim=0
#pragma HLS ARRAY_PARTITION variable=&OutMultiPix complete dim=0
#pragma HLS ARRAY_PARTITION variable=&ArrayLoc     complete dim=0
#pragma HLS ARRAY_PARTITION variable=&FiltCoeff    complete dim=2
#pragma HLS ARRAY_PARTITION variable=&FiltCoeff    complete dim=3
#pragma HLS RESOURCE variable=&FiltCoeff core=RAM_1P_LUTRAM

    // Make local copy of filter coefficients for performance reasons, mapped to LUTRAM
loop_init_coeff_phase:
	for (int i = 0; i < HSC_PHASES; i++)
	{
loop_init_coeff_tap:
		for (int j = 0; j < HSC_TAPS; j++)
		{
            for (int k = 0; k < HSC_SAMPLES_PER_CLOCK; k++) {
                FiltCoeff[i][j][k] = hfltCoeff[i][j];
            }
		}
	}

loop_height:
    for (y = 0; y < Height; y++)
    {
        U16 PhaseH[HSC_SAMPLES_PER_CLOCK];
        int ReadEn = 1;
        xReadPos       = 0;
        xWritePos      = 0;
        x_dsent        = 0;
        PackCnt        = 0;
        OutputWriteEn  = 0;

        U8 nrWrsIdxPrev = 0;
        U8 nrWrsPrev = 0;
        U8 nrWrsClck = 0;
        U8 nrWrsAccu = 0;

loop_width:
        for (x = 0; x <= LoopSize; x += HSC_SAMPLES_PER_CLOCK) // the loop runs for the max of (in,out)
        {
            //            _Pragma("HLS loop_tripcount max=((MSC_MAX_WIDTH+HSC_ARRAY_SIZE-HSC_SAMPLES_PER_CLOCK - (HSC_TAPS>>1))/HSC_SAMPLES_PER_CLOCK)")
#pragma HLS LOOP_FLATTEN OFF
#pragma HLS PIPELINE II=1

         	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        	//
        	// Pixel buffer control
        	//
        	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            if (ReadEn)   // If new pixels to be got, shift the old ones
            {
				// Shift the pixels in the array over by HSC_SAMPLES_PER_CLOCK
                for (int i = 0; i <= (HSC_ARRAY_SIZE-1-HSC_SAMPLES_PER_CLOCK); i++)
                {
                	// Includes left border handling
                	if(((x/HSC_SAMPLES_PER_CLOCK)<=1) && i < (HSC_ARRAY_SIZE-2*HSC_SAMPLES_PER_CLOCK))
                		PixArray[i] = PixArray[HSC_ARRAY_SIZE-1-HSC_SAMPLES_PER_CLOCK+1];
                	else
                		PixArray[i] = PixArray[i+HSC_SAMPLES_PER_CLOCK];
                }
                // Read new pixels if we didn't exceed the width of frame yet
                if ((xReadPos + HSC_ARRAY_SIZE-1-(HSC_TAPS>>1)) < InPixels)
                {
                	// Get new pixels from stream and insert
#ifdef DBG_PRINT
                	dbgNrRds++;
#endif
                    SrcImg >> SrcPix;
                    for (int i = 0; i < HSC_SAMPLES_PER_CLOCK; i++)
                    {
                        for(int k=0; k < HSC_NR_COMPONENTS; k++)
                        {
                            PixArray[HSC_ARRAY_SIZE-1-HSC_SAMPLES_PER_CLOCK+1+i].val[k] = SrcPix.val[HSC_NR_COMPONENTS*i + k];
                        }
                    }
                }
                else
                {
                	// Right border handling
                    for (int i = 0; i < HSC_SAMPLES_PER_CLOCK; i++)
                    {
                        PixArray[HSC_ARRAY_SIZE-1-HSC_SAMPLES_PER_CLOCK+1+i] = PixArray[HSC_ARRAY_SIZE-1-HSC_SAMPLES_PER_CLOCK];
                    }

                }
            }

#ifdef DBG_PRINT
if ((y==0) && (ReadEn))
{
	for (int i = 0; i < (HSC_ARRAY_SIZE); i++)
	{
		printf("[%2d] %3d.%3d.%3d ", (int)i, (int)PixArray[i].val[0], (int)PixArray[i].val[1], (int)PixArray[i].val[2]);
	}
	printf("\n");
}
#endif

 if  (x >= (HSC_ARRAY_SIZE-HSC_SAMPLES_PER_CLOCK-(HSC_TAPS>>1)))
            {
				U16 xbySamples = x-(HSC_ARRAY_SIZE-HSC_SAMPLES_PER_CLOCK-(HSC_TAPS>>1));
				xbySamples = xbySamples/HSC_SAMPLES_PER_CLOCK;
                // Note 'reg()' below.  Under most circumstances, this is not the preferred way to do this.  In most cases,
                // HLS supports the ability to set the latency of reads from a memory.  However, in this case, because the
                // memory is part of the axi_lite adapter, that doesn't work.
                HSC_PHASE_CTRL phases = reg(arrPhasesH[xbySamples]);
loop_samples:
                for (int s = 0; s < HSC_SAMPLES_PER_CLOCK; s++)
                {
                	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                	//
                    // Step and phase calculation
                	//
                	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                    PhaseH[s]        = phases(HSC_PHASE_CTRL_PHASE_MSB + s*HSC_PHASE_CTRL_BITS, HSC_PHASE_CTRL_PHASE_LSB + s*HSC_PHASE_CTRL_BITS);
                    ArrayLoc[s]      = phases(HSC_PHASE_CTRL_INDEX_MSB + s*HSC_PHASE_CTRL_BITS, HSC_PHASE_CTRL_INDEX_LSB + s*HSC_PHASE_CTRL_BITS);
                    OutputWriteEn[s] = phases[                                                  HSC_PHASE_CTRL_ENABLE_LSB + s*HSC_PHASE_CTRL_BITS];
                }
                ReadEn = (ArrayLoc[HSC_SAMPLES_PER_CLOCK-1]>=HSC_SAMPLES_PER_CLOCK);

#ifdef DBG_PRINT
                if (y==0)
                {
                	for (int s=0; s<HSC_SAMPLES_PER_CLOCK; s++)
                	{
                		printf("x %5d, phase %5d, pos %5d, readpos %5d writepos %5d\n", (int)x+s, (int)PhaseH[s], (int)ArrayLoc[s], (int)xReadPos, xWritePos);
                	}
                	printf("rden %d\n", ReadEn);
                }
#endif
                hscale_polyphase (FiltCoeff, PixArray, PhaseH, ArrayLoc, &OutPix);
                if (ReadEn)
                {
                    xReadPos += HSC_SAMPLES_PER_CLOCK;
                }

				#if (HSC_SAMPLES_PER_CLOCK==1)
				{
					if (OutputWriteEn && (x < LoopSize))
					{
						OutImg << OutPix;
#ifdef DBG_PRINT
						dbgNrWrs++;
#endif
					}
				}
				#else
#if (HSC_SAMPLES_PER_CLOCK == 8)

                const U8 BitSetCnt[] =
                                              { 0,  1,  1,  2,	1,	2,	2,	3,	1,	2,	2,	3,	2,	3,	3,	4,
                                            	1,	2,	2,	3,	2,	3,	3,	4,	2,	3,	3,	4,	3,	4,	4,	5,
                								1,	2,	2,	3,	2,	3,	3,	4,	2,	3,  3,	4,	3,	4,	4,	5,
                								2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
                								1,	2,	2,	3,	2,	3,	3,	4,	2,	3,	3,	4,	3,	4,	4,	5,
                								2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
                								2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
                								3,	4,	4,	5,	4,	5,	5,	6,	4,	5,	5,	6,	5,	6,	6,	7,
                								1,	2,	2,	3,	2,	3,	3,	4,	2,	3,	3,	4,	3,	4,	4,	5,
                								2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
                								2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
                								3,	4,	4,	5,	4,	5,	5,	6,	4,	5,  5,	6,	5,	6,	6,	7,
                								2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
                								3,	4,	4,	5,	4,	5,	5,	6,	4,	5,	5,	6,	5,	6,	6,	7,
                								3,	4,	4,	5,	4,	5,	5,	6,	4,	5,	5,	6,	5,	6,	6,	7,
                								4,	5,	5,	6,	5,	6,	6,	7,	5,	6,	6,	7,	6,	7,	7,	8
                                              }; // 2^ MAX_SMPLS_PERCLK
#else
                const U8 BitSetCnt[] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4}; // 2^ MAX_SMPLS_PERCLK
#endif
				nrWrsClck = BitSetCnt[OutputWriteEn];

#if (HSC_SAMPLES_PER_CLOCK == 8)

                const U8 OneBitIdx[8][256] = {
                				            {0  ,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,5	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,6	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,5	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,7	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,5	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,6	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,5      ,0      ,1      ,0      ,2      ,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0 },
                				            {0	,0	,0	,1	,0	,2	,2	,1	,0	,3	,3	,1	,3	,2	,2	,1	,0	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,0	,5	,5	,1	,5	,2	,2	,1	,5	,3	,3	,1	,3	,2	,2	,1	,5	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,0	,6	,6	,1	,6	,2	,2	,1	,6	,3	,3	,1	,3	,2	,2	,1	,6	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,6	,5	,5	,1	,5	,2	,2	,1	,5	,3	,3	,1	,3	,2	,2	,1	,5	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,0	,7	,7	,1	,7	,2	,2	,1	,7	,3	,3	,1	,3	,2	,2	,1	,7	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,7	,5	,5	,1	,5	,2	,2	,1	,5	,3	,3	,1	,3	,2	,2	,1	,5	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,7	,6	,6	,1	,6	,2	,2	,1	,6	,3	,3	,1	,3	,2	,2	,1	,6	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,6	,5	,5	,1      ,5      ,2	,2	,1	,5	,3	,3	,1	,3	,2	,2	,1	,5	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1 },
                				            {0	,0	,0	,0	,0	,0	,0	,2	,0	,0	,0	,3	,0	,3	,3	,2	,0	,0	,0	,4	,0	,4	,4	,2	,0	,4	,4	,3	,4	,3	,3	,2	,0	,0	,0	,5	,0	,5	,5	,2	,0	,5	,5	,3	,5	,3	,3	,2	,0	,5	,5	,4	,5	,4	,4	,2	,5	,4	,4	,3	,4	,3	,3	,2	,0	,0	,0	,6	,0	,6	,6	,2	,0	,6	,6	,3	,6	,3	,3	,2	,0	,6	,6	,4	,6	,4	,4	,2	,6	,4	,4	,3	,4	,3	,3	,2	,0	,6	,6	,5	,6	,5	,5	,2	,6	,5	,5	,3	,5	,3	,3	,2	,6	,5	,5	,4	,5	,4	,4	,2	,5	,4	,4	,3	,4	,3	,3	,2	,0	,0	,0	,7	,0	,7	,7	,2	,0	,7	,7	,3	,7	,3	,3	,2	,0	,7	,7	,4	,7	,4	,4	,2	,7	,4	,4	,3	,4	,3	,3	,2	,0	,7	,7	,5	,7	,5	,5	,2	,7	,5	,5	,3	,5	,3	,3	,2	,7	,5	,5	,4	,5	,4	,4	,2	,5	,4	,4	,3	,4	,3	,3	,2	,0	,7	,7	,6	,7	,6	,6	,2	,7	,6	,6	,3	,6	,3	,3	,2	,7	,6	,6	,4	,6	,4	,4	,2	,6	,4	,4	,3	,4	,3	,3	,2	,7      ,6      ,6      ,5	,6      ,5	,5	,2	,6	,5	,5	,3	,5	,3	,3	,2	,6	,5	,5	,4	,5	,4	,4	,2	,5	,4	,4	,3	,4	,3	,3	,2 },
                				            {0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,3	,0	,0	,0	,0      ,0	,0	,0	,4	,0	,0	,0	,4	,0	,4	,4	,3	,0	,0	,0	,0	,0	,0	,0	,5	,0	,0	,0	,5	,0	,5	,5	,3	,0	,0	,0	,5	,0	,5	,5	,4	,0	,5	,5	,4	,5	,4	,4	,3	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,6	,0	,6	,6	,3	,0	,0	,0	,6	,0	,6	,6	,4	,0	,6	,6	,4	,6	,4	,4	,3	,0	,0	,0	,6	,0	,6	,6	,5	,0	,6	,6	,5	,6	,5	,5	,3	,0	,6	,6	,5	,6	,5	,5	,4	,6	,5	,5	,4	,5	,4	,4	,3	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,3	,0	,0	,0	,7	,0	,7	,7	,4	,0	,7	,7	,4	,7	,4	,4	,3	,0	,0	,0	,7	,0	,7	,7	,5	,0	,7	,7	,5	,7	,5	,5	,3	,0	,7	,7	,5	,7	,5	,5	,4	,7	,5	,5	,4	,5	,4	,4	,3	,0	,0	,0	,7	,0	,7	,7	,6	,0	,7	,7	,6	,7	,6	,6	,3	,0	,7	,7	,6	,7	,6	,6	,4	,7	,6	,6	,4	,6	,4	,4	,3	,0	,7      ,7      ,6	,7      ,6	,6	,5	,7	,6	,6	,5	,6	,5	,5	,3	,7	,6	,6	,5	,6	,5	,5	,4	,6	,5	,5	,4	,5	,4	,4	,3 },
                							{0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,4	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,5	,0	,0	,0	,0	,0	,0	,0	,5	,0	,0	,0	,5	,0	,5	,5	,4	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0      ,0	,0	,0	,0	,6	,0	,0	,0	,6	,0	,6	,6	,4	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,6	,0	,6	,6	,5	,0	,0	,0	,6	,0	,6	,6	,5	,0	,6	,6	,5	,6	,5	,5	,4	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,4	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,5	,0	,0	,0	,7	,0	,7	,7	,5	,0	,7	,7	,5	,7	,5	,5	,4	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,6	,0	,0	,0	,7	,0	,7	,7	,6	,0	,7	,7	,6	,7	,6 	,6	,4	,0	,0      ,0	,7      ,0	,7	,7	,6	,0	,7	,7	,6	,7	,6	,6	,5	,0	,7	,7	,6	,7	,6	,6	,5	,7	,6	,6	,5	,6	,5	,5	,4 },
                							{0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,5	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,6	,0	,6	,6	,5	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,5	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,6	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,6	,0	,0	,0	,7	,0	,7	,7	,6	,0	,7	,7	,6	,7	,6	,6	,5 },
                							{0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0      ,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,6 },
                							{0	,0  ,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0 	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0      ,7 }
                				            };
#else
                const U8 OneBitIdx[4][16] = {
                                                               {0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0},
                                                               {0, 0, 0, 1, 0, 2, 2, 1, 0, 3, 3, 1, 3, 2, 2, 1},
                                                               {0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 3, 0, 3, 3, 2},
                                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3}
                                                           };
#endif

                // If more than HSC_SAMPLES_PER_CLOCK pixels are available we can write them out
                // This loop combines any pixels that were still waiting to be written with the pixels that were generated this clock cycle
                nrWrsAccu = nrWrsPrev+nrWrsClck;
				if ((nrWrsAccu>=HSC_SAMPLES_PER_CLOCK) && (xWritePos < OutPixels))
				{
	                for (int s = 0; s < HSC_SAMPLES_PER_CLOCK; s++)
	                {
	                	for (int c=0; c<HSC_NR_COMPONENTS; c++)
	                	{
	                	   OutMultiPix.val[s*HSC_NR_COMPONENTS+c] = (s<nrWrsPrev) ? OutPixPrv.val[s*HSC_NR_COMPONENTS+c] : OutPix.val[OneBitIdx[s-nrWrsPrev][OutputWriteEn]*HSC_NR_COMPONENTS+c]; // OutPixCur.val[(s-nrWrsPrev)*HSC_NR_COMPONENTS+c];
	                    }
	                 }
					OutImg << OutMultiPix;
#ifdef DBG_PRINT
					dbgNrWrs++;
#endif
					xWritePos += HSC_SAMPLES_PER_CLOCK;
				}

                // This loop combines any pixels that are still waiting to be written
                for (int s = 0; s < HSC_SAMPLES_PER_CLOCK; s++)
                {
					for (int c=0; c<HSC_NR_COMPONENTS; c++)
					{
						OutPixPrv.val[s*HSC_NR_COMPONENTS+c] = (nrWrsAccu>=HSC_SAMPLES_PER_CLOCK && s<((nrWrsAccu) % HSC_SAMPLES_PER_CLOCK)) ? OutPix.val[OneBitIdx[s+HSC_SAMPLES_PER_CLOCK-nrWrsPrev][OutputWriteEn]*HSC_NR_COMPONENTS+c] : ((s<nrWrsPrev) ? OutPixPrv.val[s*HSC_NR_COMPONENTS+c] : OutPix.val[OneBitIdx[s-nrWrsPrev][OutputWriteEn]*HSC_NR_COMPONENTS+c]);
					}
                }
                nrWrsPrev = (nrWrsAccu) % HSC_SAMPLES_PER_CLOCK;

				#endif

            }
        }
    }
#ifdef DBG_PRINT
    printf("dbgNrRds %d dbgNrWrs %d\n", dbgNrRds, dbgNrWrs);
#endif
}

void hscale_polyphase(I16 FiltCoeff[HSC_PHASES][HSC_TAPS][HSC_SAMPLES_PER_CLOCK],
		   	   	      YUV_PIXEL PixArray[HSC_ARRAY_SIZE],
                      U16 PhasesH[HSC_SAMPLES_PER_CLOCK],
                      U8 ArrayIdx[HSC_SAMPLES_PER_CLOCK],
                      YUV_MULTI_PIXEL *OutPix)
{
#pragma HLS inline self off
    for (int s = 0; s < HSC_SAMPLES_PER_CLOCK; s++)
    {
        U16 PhaseH = PhasesH[s];
        U8 idx   = ArrayIdx[s]+((HSC_SAMPLES_PER_CLOCK==1) ? 1 : 0);
        I16 FiltCoeffRead[HSC_TAPS];
#pragma HLS array_partition variable=&FiltCoeffRead complete
        for (int t = 0; t < HSC_TAPS; t++) {
            FiltCoeffRead[t] = FiltCoeff[PhaseH][t][s];
        }
        for (int c = 0; c < HSC_NR_COMPONENTS; c++)
        {
#pragma HLS expression_balance off
            I32 sum = (COEFF_PRECISION>>1);  //MP todo 32 bits might not be enough
        	I32 norm = 0;

        	// Center tap is tap with index (VSC_TAPS>>1), 0 is left most, HSC_ARRAY_SIZE-1 is right most pixel
        	for (int t = 0; t < HSC_TAPS; t++)
            {
                assert(PhaseH < HSC_PHASES);
                assert(t < HSC_TAPS);
                assert(idx+t < HSC_ARRAY_SIZE);
                sum += PixArray[idx+t].val[c] * FiltCoeffRead[t];
            }
            norm = sum >> COEFF_PRECISION_SHIFT;
            norm = CLAMP(norm, 0, (1<<HSC_BITS_PER_COMPONENT)-1);

            OutPix->val[HSC_NR_COMPONENTS*s+c] = norm;
        }
    }
}
#endif

#if  (HSC_SCALE_MODE == HSC_BICUBIC)

/*********************************************************************************
* Function:    hscale_core
* Parameters:  Stream of input/output pixels, image resolution, type of scaling etc
* Return:
* Description: Perform horizontal image resizing
**********************************************************************************/
void hscale_core_bicubic  (HSC_STREAM_MULTIPIX& SrcImg,
                           U16 &Height,
                           U16 &InPixels,
                           U16 &OutPixels,
                           U32 &Rate,
                           U8 &RegColorMode,
                           HSC_PHASE_CTRL arrPhasesH[HSC_MAX_WIDTH/HSC_SAMPLES_PER_CLOCK],
                           HSC_STREAM_MULTIPIX& OutImg
                           )
{

    ap_uint<HSC_SAMPLES_PER_CLOCK> OutputWriteEn;

    U8    ArrayLoc[HSC_SAMPLES_PER_CLOCK];
    U16  xReadPos, xWritePos;
    U16 y, x;
    U16 x_dsent;
    U8  PackCnt;
    U8  ComputedPixels;

    YUV_MULTI_PIXEL SrcPix, OutPix, OutPixPrv;
    YUV_PIXEL PixArray[HSC_ARRAY_SIZE];
    YUV_MULTI_PIXEL OutMultiPix;

    assert(Height <= HSC_MAX_HEIGHT);
    assert(Height >= MIN_PIXELS);

    U16 TotalPixels  = std::max(OutPixels, InPixels);
    assert(TotalPixels <= HSC_MAX_WIDTH);
    assert(TotalPixels >= MIN_PIXELS);

    U16 LoopSize     = TotalPixels + HSC_ARRAY_SIZE - HSC_SAMPLES_PER_CLOCK - (HSC_TAPS>>1);

    assert((LoopSize%HSC_SAMPLES_PER_CLOCK) == 0);
#ifdef DBG_PRINT
    int dbgNrRds =0;
    int dbgNrWrs =0;
#endif

#pragma HLS ARRAY_PARTITION variable=&SrcPix      complete dim=0
#pragma HLS ARRAY_PARTITION variable=&PixArray    complete dim=0
#pragma HLS ARRAY_PARTITION variable=&OutPix      complete dim=0
#pragma HLS ARRAY_PARTITION variable=&OutPixPrv   complete dim=0
#pragma HLS ARRAY_PARTITION variable=&OutMultiPix complete dim=0
#pragma HLS ARRAY_PARTITION variable=&ArrayLoc   complete dim = 0

loop_height:
    for (y = 0; y < Height; y++)
    {
        U16 PhaseH[HSC_SAMPLES_PER_CLOCK];
        int ReadEn = 1;
        xReadPos       = 0;
        xWritePos      = 0;
        x_dsent        = 0;
        PackCnt        = 0;
        OutputWriteEn  = 0;

        U8 nrWrsIdxPrev = 0;
        U8 nrWrsPrev = 0;
        U8 nrWrsClck = 0;
        U8 nrWrsAccu = 0;

loop_width:
        for (x = 0; x <= LoopSize; x += HSC_SAMPLES_PER_CLOCK) // the loop runs for the max of (in,out)
        {
            //            _Pragma("HLS loop_tripcount max=((MSC_MAX_WIDTH+HSC_ARRAY_SIZE-HSC_SAMPLES_PER_CLOCK - (HSC_TAPS>>1)))")
#pragma HLS LOOP_FLATTEN OFF
#pragma HLS PIPELINE

        	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        	//
        	// Pixel buffer control
        	//
        	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            if (ReadEn)   // If new pixels to be got, shift the old ones
            {
				// Shift the pixels in the array over by HSC_SAMPLES_PER_CLOCK
                for (int i = 0; i <= (HSC_ARRAY_SIZE-1-HSC_SAMPLES_PER_CLOCK); i++)
                {
                	// Includes left border handling
					PixArray[i] = (((x/HSC_SAMPLES_PER_CLOCK)<=1) && i < (HSC_ARRAY_SIZE-2*HSC_SAMPLES_PER_CLOCK)) ? PixArray[HSC_ARRAY_SIZE-1-HSC_SAMPLES_PER_CLOCK+1] : PixArray[i+HSC_SAMPLES_PER_CLOCK];
                }
                // Read new pixels if we didn't exceed the width of frame yet
                if ((xReadPos + HSC_ARRAY_SIZE-1-(HSC_TAPS>>1)) < InPixels)
                {
                	// Get new pixels from stream and insert
#ifdef DBG_PRINT
                	dbgNrRds++;
#endif
                    SrcImg >> SrcPix;
                    for (int i = 0; i < HSC_SAMPLES_PER_CLOCK; i++)
                    {
                        for(int k=0; k < HSC_NR_COMPONENTS; k++)
                        {
                            PixArray[HSC_ARRAY_SIZE-1-HSC_SAMPLES_PER_CLOCK+1+i].val[k] = SrcPix.val[HSC_NR_COMPONENTS*i + k];
                        }
                    }
                }
                else
                {
                	// Right border handling
                    for (int i = 0; i < HSC_SAMPLES_PER_CLOCK; i++)
                    {
                        PixArray[HSC_ARRAY_SIZE-1-HSC_SAMPLES_PER_CLOCK+1+i] = PixArray[HSC_ARRAY_SIZE-1-HSC_SAMPLES_PER_CLOCK];
                    }

                }
            }

#ifdef DBG_PRINT
if ((y==0) && (ReadEn))
{
	for (int i = 0; i < (HSC_ARRAY_SIZE); i++)
	{
		printf("[%2d] %3d.%3d.%3d ", (int)i, (int)PixArray[i].val[0], (int)PixArray[i].val[1], (int)PixArray[i].val[2]);
	}
	printf("\n");
}
#endif
            if (x >= (HSC_ARRAY_SIZE-HSC_SAMPLES_PER_CLOCK-(HSC_TAPS>>1)))
            {
				U16 xbySamples = x-(HSC_ARRAY_SIZE-HSC_SAMPLES_PER_CLOCK-(HSC_TAPS>>1));
				xbySamples = xbySamples/HSC_SAMPLES_PER_CLOCK;
                // Note 'reg()' below.  Under most circumstances, this is not the preferred way to do this.  In most cases,
                // HLS supports the ability to set the latency of reads from a memory.  However, in this case, because the
                // memory is part of the axi_lite adapter, that doesn't work.
                HSC_PHASE_CTRL phases = reg(arrPhasesH[xbySamples]);
loop_samples:
                for (int s = 0; s < HSC_SAMPLES_PER_CLOCK; s++)
                {
                	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                	//
                    // Step and phase calculation
                	//
                	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                    PhaseH[s]        = phases(HSC_PHASE_CTRL_PHASE_MSB + s*HSC_PHASE_CTRL_BITS, HSC_PHASE_CTRL_PHASE_LSB + s*HSC_PHASE_CTRL_BITS);
                    ArrayLoc[s]      = phases(HSC_PHASE_CTRL_INDEX_MSB + s*HSC_PHASE_CTRL_BITS, HSC_PHASE_CTRL_INDEX_LSB + s*HSC_PHASE_CTRL_BITS);
                    OutputWriteEn[s] = phases[                                                  HSC_PHASE_CTRL_ENABLE_LSB + s*HSC_PHASE_CTRL_BITS];
                }
                ReadEn = (ArrayLoc[HSC_SAMPLES_PER_CLOCK-1]>=HSC_SAMPLES_PER_CLOCK);

#ifdef DBG_PRINT
                if (y==0)
                {
                	for (int s=0; s<HSC_SAMPLES_PER_CLOCK; s++)
                	{
                		printf("x %5d, phase %5d, pos %5d, readpos %5d writepos %5d\n", (int)x+s, (int)PhaseH[s], (int)ArrayLoc[s], (int)xReadPos, xWritePos);
                	}
                	printf("rden %d\n", ReadEn);
                }
#endif
				hscale_bicubic(PixArray, PhaseH, ArrayLoc, &OutPix);

                if (ReadEn)
                {
                    xReadPos += HSC_SAMPLES_PER_CLOCK;
                }

				#if (HSC_SAMPLES_PER_CLOCK==1)
				{
					if (OutputWriteEn && (x < LoopSize))
					{
						OutImg << OutPix;
#ifdef DBG_PRINT
						dbgNrWrs++;
#endif
						xWritePos += HSC_SAMPLES_PER_CLOCK;
					}
				}
				#else
#if (HSC_SAMPLES_PER_CLOCK == 8)
				const U8 BitSetCnt[] =
				                              { 0,  1,  1,  2,	1,	2,	2,	3,	1,	2,	2,	3,	2,	3,	3,	4,
				                            	1,	2,	2,	3,	2,	3,	3,	4,	2,	3,	3,	4,	3,	4,	4,	5,
												1,	2,	2,	3,	2,	3,	3,	4,	2,	3,  3,	4,	3,	4,	4,	5,
												2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
												1,	2,	2,	3,	2,	3,	3,	4,	2,	3,	3,	4,	3,	4,	4,	5,
												2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
												2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
												3,	4,	4,	5,	4,	5,	5,	6,	4,	5,	5,	6,	5,	6,	6,	7,
												1,	2,	2,	3,	2,	3,	3,	4,	2,	3,	3,	4,	3,	4,	4,	5,
												2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
												2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
												3,	4,	4,	5,	4,	5,	5,	6,	4,	5,  5,	6,	5,	6,	6,	7,
												2,	3,	3,	4,	3,	4,	4,	5,	3,	4,	4,	5,	4,	5,	5,	6,
												3,	4,	4,	5,	4,	5,	5,	6,	4,	5,	5,	6,	5,	6,	6,	7,
												3,	4,	4,	5,	4,	5,	5,	6,	4,	5,	5,	6,	5,	6,	6,	7,
												4,	5,	5,	6,	5,	6,	6,	7,	5,	6,	6,	7,	6,	7,	7,	8
				                              }; // 2^ MAX_SMPLS_PERCLK
#else
			    const U8 BitSetCnt[] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4}; // 2^ MAX_SMPLS_PERCLK
#endif
			    nrWrsClck = BitSetCnt[OutputWriteEn];

				// Below table provides an index to the n-th [0..3] 1 bit for numbers between [0..15]

 #if (HSC_SAMPLES_PER_CLOCK == 8)

                const U8 OneBitIdx[8][256] = {
                				            {0  ,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,5	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,6	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,5	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,7	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,5	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,6	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,5      ,0      ,1      ,0      ,2      ,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0	,4	,0	,1	,0	,2	,0	,1	,0	,3	,0	,1	,0	,2	,0	,1	,0 },
                				            {0	,0	,0	,1	,0	,2	,2	,1	,0	,3	,3	,1	,3	,2	,2	,1	,0	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,0	,5	,5	,1	,5	,2	,2	,1	,5	,3	,3	,1	,3	,2	,2	,1	,5	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,0	,6	,6	,1	,6	,2	,2	,1	,6	,3	,3	,1	,3	,2	,2	,1	,6	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,6	,5	,5	,1	,5	,2	,2	,1	,5	,3	,3	,1	,3	,2	,2	,1	,5	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,0	,7	,7	,1	,7	,2	,2	,1	,7	,3	,3	,1	,3	,2	,2	,1	,7	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,7	,5	,5	,1	,5	,2	,2	,1	,5	,3	,3	,1	,3	,2	,2	,1	,5	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,7	,6	,6	,1	,6	,2	,2	,1	,6	,3	,3	,1	,3	,2	,2	,1	,6	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1	,6	,5	,5	,1      ,5      ,2	,2	,1	,5	,3	,3	,1	,3	,2	,2	,1	,5	,4	,4	,1	,4	,2	,2	,1	,4	,3	,3	,1	,3	,2	,2	,1 },
                				            {0	,0	,0	,0	,0	,0	,0	,2	,0	,0	,0	,3	,0	,3	,3	,2	,0	,0	,0	,4	,0	,4	,4	,2	,0	,4	,4	,3	,4	,3	,3	,2	,0	,0	,0	,5	,0	,5	,5	,2	,0	,5	,5	,3	,5	,3	,3	,2	,0	,5	,5	,4	,5	,4	,4	,2	,5	,4	,4	,3	,4	,3	,3	,2	,0	,0	,0	,6	,0	,6	,6	,2	,0	,6	,6	,3	,6	,3	,3	,2	,0	,6	,6	,4	,6	,4	,4	,2	,6	,4	,4	,3	,4	,3	,3	,2	,0	,6	,6	,5	,6	,5	,5	,2	,6	,5	,5	,3	,5	,3	,3	,2	,6	,5	,5	,4	,5	,4	,4	,2	,5	,4	,4	,3	,4	,3	,3	,2	,0	,0	,0	,7	,0	,7	,7	,2	,0	,7	,7	,3	,7	,3	,3	,2	,0	,7	,7	,4	,7	,4	,4	,2	,7	,4	,4	,3	,4	,3	,3	,2	,0	,7	,7	,5	,7	,5	,5	,2	,7	,5	,5	,3	,5	,3	,3	,2	,7	,5	,5	,4	,5	,4	,4	,2	,5	,4	,4	,3	,4	,3	,3	,2	,0	,7	,7	,6	,7	,6	,6	,2	,7	,6	,6	,3	,6	,3	,3	,2	,7	,6	,6	,4	,6	,4	,4	,2	,6	,4	,4	,3	,4	,3	,3	,2	,7      ,6      ,6      ,5	,6      ,5	,5	,2	,6	,5	,5	,3	,5	,3	,3	,2	,6	,5	,5	,4	,5	,4	,4	,2	,5	,4	,4	,3	,4	,3	,3	,2 },
                				            {0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,3	,0	,0	,0	,0      ,0	,0	,0	,4	,0	,0	,0	,4	,0	,4	,4	,3	,0	,0	,0	,0	,0	,0	,0	,5	,0	,0	,0	,5	,0	,5	,5	,3	,0	,0	,0	,5	,0	,5	,5	,4	,0	,5	,5	,4	,5	,4	,4	,3	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,6	,0	,6	,6	,3	,0	,0	,0	,6	,0	,6	,6	,4	,0	,6	,6	,4	,6	,4	,4	,3	,0	,0	,0	,6	,0	,6	,6	,5	,0	,6	,6	,5	,6	,5	,5	,3	,0	,6	,6	,5	,6	,5	,5	,4	,6	,5	,5	,4	,5	,4	,4	,3	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,3	,0	,0	,0	,7	,0	,7	,7	,4	,0	,7	,7	,4	,7	,4	,4	,3	,0	,0	,0	,7	,0	,7	,7	,5	,0	,7	,7	,5	,7	,5	,5	,3	,0	,7	,7	,5	,7	,5	,5	,4	,7	,5	,5	,4	,5	,4	,4	,3	,0	,0	,0	,7	,0	,7	,7	,6	,0	,7	,7	,6	,7	,6	,6	,3	,0	,7	,7	,6	,7	,6	,6	,4	,7	,6	,6	,4	,6	,4	,4	,3	,0	,7      ,7      ,6	,7      ,6	,6	,5	,7	,6	,6	,5	,6	,5	,5	,3	,7	,6	,6	,5	,6	,5	,5	,4	,6	,5	,5	,4	,5	,4	,4	,3 },
                							{0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,4	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,5	,0	,0	,0	,0	,0	,0	,0	,5	,0	,0	,0	,5	,0	,5	,5	,4	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0      ,0	,0	,0	,0	,6	,0	,0	,0	,6	,0	,6	,6	,4	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,6	,0	,6	,6	,5	,0	,0	,0	,6	,0	,6	,6	,5	,0	,6	,6	,5	,6	,5	,5	,4	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,4	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,5	,0	,0	,0	,7	,0	,7	,7	,5	,0	,7	,7	,5	,7	,5	,5	,4	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,6	,0	,0	,0	,7	,0	,7	,7	,6	,0	,7	,7	,6	,7	,6 	,6	,4	,0	,0      ,0	,7      ,0	,7	,7	,6	,0	,7	,7	,6	,7	,6	,6	,5	,0	,7	,7	,6	,7	,6	,6	,5	,7	,6	,6	,5	,6	,5	,5	,4 },
                							{0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,5	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,6	,0	,6	,6	,5	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,5	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,6	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,6	,0	,0	,0	,7	,0	,7	,7	,6	,0	,7	,7	,6	,7	,6	,6	,5 },
                							{0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,6	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0      ,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,0	,0	,0	,0	,7	,0	,0	,0	,7	,0	,7	,7	,6 },
                							{0	,0  ,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0 	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0	,0      ,7 }
                				            };
#else
                const U8 OneBitIdx[4][16] = {
                                                               {0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0},
                                                               {0, 0, 0, 1, 0, 2, 2, 1, 0, 3, 3, 1, 3, 2, 2, 1},
                                                               {0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 3, 0, 3, 3, 2},
                                                               {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3}
                                                           };
#endif

                // If more than HSC_SAMPLES_PER_CLOCK pixels are available we can write them out
                // This loop combines any pixels that were still waiting to be written with the pixels that were generated this clock cycle
                nrWrsAccu = nrWrsPrev+nrWrsClck;
				if ((nrWrsAccu>=HSC_SAMPLES_PER_CLOCK) && (xWritePos < OutPixels))
				{
	                for (int s = 0; s < HSC_SAMPLES_PER_CLOCK; s++)
	                {
	                	for (int c=0; c<HSC_NR_COMPONENTS; c++)
	                	{
	                		OutMultiPix.val[s*HSC_NR_COMPONENTS+c] = (s<nrWrsPrev) ? OutPixPrv.val[s*HSC_NR_COMPONENTS+c] : OutPix.val[OneBitIdx[s-nrWrsPrev][OutputWriteEn]*HSC_NR_COMPONENTS+c]; // OutPixCur.val[(s-nrWrsPrev)*HSC_NR_COMPONENTS+c];
	                	}
	                }
					OutImg << OutMultiPix;
#ifdef DBG_PRINT
					dbgNrWrs++;
#endif
					xWritePos += HSC_SAMPLES_PER_CLOCK;
				}

                // This loop combines any pixels that are still waiting to be written
                for (int s = 0; s < HSC_SAMPLES_PER_CLOCK; s++)
                {
					for (int c=0; c<HSC_NR_COMPONENTS; c++)
					{
						OutPixPrv.val[s*HSC_NR_COMPONENTS+c] = (nrWrsAccu>=HSC_SAMPLES_PER_CLOCK && s<((nrWrsAccu) % HSC_SAMPLES_PER_CLOCK)) ? OutPix.val[OneBitIdx[s+HSC_SAMPLES_PER_CLOCK-nrWrsPrev][OutputWriteEn]*HSC_NR_COMPONENTS+c] : ((s<nrWrsPrev) ? OutPixPrv.val[s*HSC_NR_COMPONENTS+c] : OutPix.val[OneBitIdx[s-nrWrsPrev][OutputWriteEn]*HSC_NR_COMPONENTS+c]);
					}
                }
                nrWrsPrev = (nrWrsAccu) % HSC_SAMPLES_PER_CLOCK;

				#endif
            }
        }
    }
#ifdef DBG_PRINT
    printf("dbgNrRds %d dbgNrWrs %d\n", dbgNrRds, dbgNrWrs);
#endif
}

void hscale_bicubic (YUV_PIXEL PixArray[HSC_ARRAY_SIZE],
                     U16 PhasesH[HSC_SAMPLES_PER_CLOCK],
                     U8 ArrayIdx[HSC_SAMPLES_PER_CLOCK],
                     YUV_MULTI_PIXEL *OutPix)
{
#pragma HLS inline
    I64 sum;
    I32 norm;
    I32 a, b, c;
    ap_uint<28> d;
    ap_int<52> ax3;
    ap_int<40> bx2;
    ap_int<28> cx;

    for (int s = 0; s < HSC_SAMPLES_PER_CLOCK; s++) // samples
    {
        U16 PhaseH = PhasesH[s];
        U8 idxM1 = ArrayIdx[s]+1;
        U8 idx   = ArrayIdx[s]+2;
        U8 idxP1 = ArrayIdx[s]+3;
        U8 idxP2 = ArrayIdx[s]+4;

        for (int i = 0; i < HSC_NR_COMPONENTS; i++)
        {
            a = ((PixArray[idx].val[i] * 3) - (PixArray[idxP1].val[i] * 3) - (PixArray[idxM1].val[i]* 1) + (PixArray[idxP2].val[i] * 1)) >> 1;
            b = ((PixArray[idxP1].val[i] * 4) - (PixArray[idx].val[i] * 5) + (PixArray[idxM1].val[i] * 2) - (PixArray[idxP2].val[i] * 1)) >> 1;
            c = ((PixArray[idxP1].val[i] * 1) - (PixArray[idxM1].val[i] * 1)) >> 1;
            d = PixArray[idx].val[i] * HSC_PHASES;

            ax3 =  (((ap_int<52>) a * PhaseH * PhaseH * PhaseH) + HSC_PHASES) >> (HSC_PHASE_SHIFT +  HSC_PHASE_SHIFT); // mapped to 5 DSPs
            bx2 =  (((ap_int<40>) b * PhaseH * PhaseH) + (HSC_PHASES >> 1)) >> (HSC_PHASE_SHIFT); // mapped to 3 DSPs, reuses PhaseH*PhaseH
            cx  =  ((ap_int<28>) c * PhaseH);	// mapped to 3 DSPs

            sum = (I64) ((ax3 + bx2 + cx)  + (I32) d);
            norm = (I32) (sum + (HSC_PHASES >> 1)) >> HSC_PHASE_SHIFT;
            norm = CLAMP(norm, 0, (I32) ((1<<HSC_BITS_PER_COMPONENT)-1));
            OutPix->val[HSC_NR_COMPONENTS*s+i] = norm;
        }
    }
}
#endif

/************************************************************************************
* Function:    AXIvideo2MultiPixStream
* Parameters:  Multiple Pixel AXI Stream, User Stream, Image Resolution
* Return:      None
* Description: Read data from multiple pixel/clk AXI stream into user defined
*              stream
**************************************************************************************/
int AXIvideo2MultiPixStream(HSC_AXI_STREAM_IN& AXI_video_strm,
                            HSC_STREAM_MULTIPIX& img,
                            U16 &Height,
                            U16 &WidthIn,
                            U8 &colorMode
                            )
{
    int res = 0;
    ap_axiu<(HSC_BITS_PER_CLOCK),1,1,1> axi;
    YUV_MULTI_PIXEL pix;

    int rows         = Height;
    int cols         = WidthIn;
    assert(rows <= HSC_MAX_HEIGHT);
    assert(cols <= HSC_MAX_WIDTH);
    assert(rows >= MIN_PIXELS);
    assert(cols >= MIN_PIXELS);

    assert(cols % HSC_SAMPLES_PER_CLOCK == 0);

    int depth        = HSC_BITS_PER_COMPONENT; //BitsPerCol;

    bool sof = 0;
    //#pragma HLS array_partition complete dim=0 variable=&pix.p
loop_wait_for_start:
    while (!sof)
    {
#pragma HLS pipeline II=1
#pragma HLS loop_tripcount avg=0 max=0
        AXI_video_strm >> axi;
        sof = axi.user.to_int();
    }
loop_height:
    for (int i = 0; i < rows; i++)
    {

        bool eol = 0;
loop_width:
        for (int j = 0; j < cols/HSC_SAMPLES_PER_CLOCK; j++)
        {
#pragma HLS loop_flatten off
#pragma HLS pipeline II=1
            if (sof || eol)
            {
                sof = 0;
            }
            else
            {
                AXI_video_strm >> axi;
             }
            eol = axi.last.to_int();
            if (eol && (j != cols/HSC_SAMPLES_PER_CLOCK-1))
            {
                // will work only for integral values of image width to samplesperclk
                res |= ERROR_IO_EOL_EARLY;
            }
            for (int l = 0; l < HSC_SAMPLES_PER_CLOCK; l++)
            {
				for (HLS_CHANNEL_T k = 0; k < HSC_NR_COMPONENTS; k++)
				{
                	ap_uint<HSC_BITS_PER_COMPONENT> pix_rgb, pix_444, pix_422;
                    const int map[3] = {2, 0, 1};
#pragma HLS ARRAY_PARTITION variable=&map complete dim=0
            switch(colorMode)
                    {
                    case 0x0:
			hls::AXIGetBitFields(axi, (map[k] + l * 3 ) * depth, depth, pix_rgb);
                        pix.val[k + l * HSC_NR_COMPONENTS] = pix_rgb;
                        break;
                    case 0x1:
 			hls::AXIGetBitFields(axi, (k + l * 3) * depth, depth, pix_444);
                        pix.val[k + l * HSC_NR_COMPONENTS] = pix_444;
                        break;
                    default:
			hls::AXIGetBitFields(axi, (k + l * 2) * depth, depth, pix_422);
                        pix.val[k + l * HSC_NR_COMPONENTS] = pix_422;
                        break;
                    }
				}
            }
            img << pix;
        }
loop_wait_for_eol:
        while (!eol)
        {
#pragma HLS pipeline II=1
#pragma HLS loop_tripcount avg=0 max=0
            // Keep reading until we get to EOL
            AXI_video_strm >> axi;
            eol = axi.last.to_int();
            res |= ERROR_IO_EOL_LATE;
        }
    }
    return res;
}

/*********************************************************************************
* Function:    MultiPixStream2AXIvideo
* Parameters:  Multi Pixel Stream, AXI Video Stream, Image Resolution
* Return:      None
* Description: Convert a m pixel/clk stream to AXI Video
*              (temporary function until official hls:video library is updated
*               to provide the required functionality)
**********************************************************************************/
int MultiPixStream2AXIvideo(HSC_STREAM_MULTIPIX& StrmMPix,
                            HSC_AXI_STREAM_OUT& AXI_video_strm,
                            U16 &Height,
                            U16 &WidthOut,
                            U8 &ColorMode
                            )
{
    int res = 0;
    YUV_MULTI_PIXEL pix;

    ap_axiu<(HSC_BITS_PER_CLOCK),1,1,1> axi;
    int depth = HSC_BITS_PER_COMPONENT; //BitsPerCol;

    int rows = Height;
    int cols = WidthOut;
    assert(rows <= HSC_MAX_HEIGHT);
    assert(cols <= HSC_MAX_WIDTH);
    assert(rows >= MIN_PIXELS);
    assert(cols >= MIN_PIXELS);
    assert(cols % HSC_SAMPLES_PER_CLOCK == 0);

#if (HSC_SAMPLES_PER_CLOCK == 1)
    const ap_uint<5> mapComp[4][3] =  {
                                        {1,  2,  0},     //RGB
                                        {0,  1,  2},     //4:4:4
                                        {0,  1,  2},     //4:2:2
                                        {0,  1,  2}      //4:2:0
                                        };
#endif
#if (HSC_SAMPLES_PER_CLOCK == 2)
    const ap_uint<5> mapComp[4][6] =  {
                                        {1,  2,  0,  4,  5,  3},     //RGB
                                        {0,  1,  2,  3,  4,  5},     //4:4:4
                                        {0,  1,  3,  4,  5,  2},     //4:2:2
                                        {0,  1,  3,  4,  5,  2}      //4:2:0
                                        };
#endif
#if (HSC_SAMPLES_PER_CLOCK == 4)
    const ap_uint<5> mapComp[4][12] =  {
                                        {1,  2,  0,  4,  5,  3,  7,  8,  6, 10, 11,  9},     //RGB
                                        {0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11},     //4:4:4
                                        {0,  1,  3,  4,  6,  7,  9, 10, 11,  8,  5,  2},     //4:2:2
                                        {0,  1,  3,  4,  6,  7,  9, 10, 11,  8,  5,  2}      //4:2:0
                                        };
#endif

#if (HSC_SAMPLES_PER_CLOCK == 8)
    const ap_uint<5> mapComp[4][24] =  {
                                        {1,  2,  0,  4,  5,  3,  7,  8,  6, 10, 11,  9, 13, 14, 12, 16, 17, 15, 19, 20, 18, 22, 23, 21},     //RGB
                                        {0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23},     //4:4:4
                                        {0,  1,  3,  4,  6,  7,  9, 10, 12, 13, 15, 16, 18, 19, 21, 22, 23, 20, 17, 14, 11,  8,  5,  2},     //4:2:2
                                        {0,  1,  3,  4,  6,  7,  9, 10, 12, 13, 15, 16, 18, 19, 21, 22, 23, 20, 17, 14, 11,  8,  5,  2}     //4:2:0
                                        };
#endif

    ap_uint<5> map[HSC_NR_COMPONENTS*HSC_SAMPLES_PER_CLOCK];
#pragma HLS ARRAY_PARTITION variable=&map complete dim=0
    for (int i=0; i<(HSC_NR_COMPONENTS*HSC_SAMPLES_PER_CLOCK); i++)
    {
      map[i] = mapComp[ColorMode][i];
    }

    bool sof = 1;
    for (int i = 0; i < rows; i++)
    {
        //#pragma HLS loop_tripcount max=2160
        for (int j = 0; j < cols/HSC_SAMPLES_PER_CLOCK; j++)
        {
            //#pragma HLS loop_tripcount max=3840 //+SmplsPerClk
#pragma HLS loop_flatten off
#pragma HLS pipeline II=1
            if (sof)
            {
                axi.user = 1;
                sof = 0;
            }
            else
            {
                axi.user = 0;
            }
            if (j == (cols/HSC_SAMPLES_PER_CLOCK-1))
            {
                axi.last = 1;
            }
            else
            {
                axi.last = 0;
            }
            StrmMPix >> pix;
            axi.data = -1;


            for (int l = 0; l < HSC_SAMPLES_PER_CLOCK; l++)
            {
                for (int k = 0; k < HSC_NR_COMPONENTS; k++)
                {
                	ap_uint<5> kMap = map[k + l * HSC_NR_COMPONENTS];
                    int start = (k + l * HSC_NR_COMPONENTS) * depth;
                    axi.data(start + depth - 1, start) = pix.val[kMap];
                }
            }
            axi.keep = -1;

            AXI_video_strm << axi;
        }
    }
    return res;
}


#if (HSC_ENABLE_422==1)

#define KERNEL_H_SIZE   4

static void v_hcresampler_core(HSC_STREAM_MULTIPIX& srcImg,
                               U16 &height,
                               U16 &width,
                               const U8 &colorMode,
                               bool &bPassThru,
                               HSC_STREAM_MULTIPIX& outImg)
{
    I16 y,x,k;
    I16 CRpix, first_pix, last_pix, chroma_out_pix, odd_col;
    I16 center_tap;
    I16 shift;
    I16 yOffset = 0;
    I16 xOffset; //offset between streaming pixel and processing pixel
    I16 loopHeight= height + yOffset;
    I16 loopWidth;
    I16 out_x;

    YUV_MULTI_PIXEL inpix;
    YUV_MULTI_PIXEL outpix;
    Y_MULTI_PIXEL mpix_y;
    C_MULTI_PIXEL mpix_cb, mpix_cr;

    const U8 PIXBUF_C_DEPTH = (((((((KERNEL_H_SIZE/2)+1)+HSC_SAMPLES_PER_CLOCK-1)+(HSC_SAMPLES_PER_CLOCK-1))/HSC_SAMPLES_PER_CLOCK)*HSC_SAMPLES_PER_CLOCK)+(KERNEL_H_SIZE/2)-1);
    const U8 PIXBUF_Y_DEPTH = ((PIXBUF_C_DEPTH-((KERNEL_H_SIZE/2)-1))*2);

    Y_PIXEL pixbuf_y[PIXBUF_Y_DEPTH];
    C_PIXEL pixbuf_cb[PIXBUF_C_DEPTH], pixbuf_cr[PIXBUF_C_DEPTH];

    if (bPassThru)
    {
        xOffset = 0;
        center_tap = 0;
    }
    else if (colorMode == yuv422)
    {
        xOffset = ((PIXBUF_Y_DEPTH/HSC_SAMPLES_PER_CLOCK)-1);
        center_tap = 0;
    }
    else
    {
        xOffset = ((PIXBUF_C_DEPTH-(KERNEL_H_SIZE/2))/HSC_SAMPLES_PER_CLOCK);
        center_tap = (PIXBUF_Y_DEPTH-((xOffset+1)*HSC_SAMPLES_PER_CLOCK));
    }

    loopWidth = (width/HSC_SAMPLES_PER_CLOCK) + xOffset;

    for(y=0; y<loopHeight; ++y)
    {

        for(x=0; x<loopWidth; ++x)
        {

#pragma HLS LOOP_FLATTEN OFF
#pragma HLS PIPELINE II=1

            //processing (output) pixel coordinates

            out_x = x - xOffset;

            if (x < (width/HSC_SAMPLES_PER_CLOCK))
            {
                srcImg >> inpix;

                // create multi-pixel samples of luma only or chroma only
                for (I16 s=0; s<HSC_SAMPLES_PER_CLOCK; ++s)
                {
                    mpix_y.val[s]  = inpix.val[s*3];
                    if (colorMode == yuv444)
                    {
                        mpix_cb.val[s] = inpix.val[s*3+1];
                        mpix_cr.val[s] = inpix.val[s*3+2];
                    }
                    else
                    {
                        if (((x*HSC_SAMPLES_PER_CLOCK)+s) & 1)
                        {
                            mpix_cr.val[s/2] = inpix.val[s*3+1];
                        }
                        else
                        {
                            mpix_cb.val[s/2] = inpix.val[s*3+1];
                        }
                    }
                }
            }
            //luma pixel buffer
#pragma HLS ARRAY_PARTITION variable=&pixbuf_y      complete dim=0
            //shift right by HSC_SAMPLES_PER_CLOCK to make space for next HSC_SAMPLES_PER_CLOCK inputs at top
            for(I16 i=0 ; i<(PIXBUF_Y_DEPTH-HSC_SAMPLES_PER_CLOCK); i++)
            {
                pixbuf_y[i] = pixbuf_y[i+HSC_SAMPLES_PER_CLOCK];
            }
            //push read pixels at FIFO top
            for(k=0; k<HSC_SAMPLES_PER_CLOCK; k++)
            {
                pixbuf_y[PIXBUF_Y_DEPTH-1-k].val[0] = mpix_y.val[HSC_SAMPLES_PER_CLOCK-1-k];
            }

            //chroma pixel buffer
#pragma HLS ARRAY_PARTITION variable=&pixbuf_cb      complete dim=0
#pragma HLS ARRAY_PARTITION variable=&pixbuf_cr      complete dim=0

#if (HSC_SAMPLES_PER_CLOCK == 1)
            shift = 1;
#else
            shift = (colorMode == yuv444) ? 1 : 2;
#endif

            //push read pixels at FIFO top
            for(k=0; k<HSC_SAMPLES_PER_CLOCK; k++)
            {

#if (HSC_SAMPLES_PER_CLOCK == 1)
                first_pix = (colorMode == yuv444) ? (x==0) : (x==1);
                CRpix = (colorMode == yuv444) ? 1 : (x & 1);
#else
                first_pix = (colorMode == yuv444) ? (x==0) : ((x==0) && (k==1));
                CRpix = (colorMode == yuv444) ? 1 : (k & 1);
#endif

                if (CRpix == 1) // work with Cb/Cr pairs
                {
                    //shift right to make space for next input at top
                    for(I16 i=0 ; i<(PIXBUF_C_DEPTH-1); i++)
                    {
                        pixbuf_cb[i] = pixbuf_cb[i+1];
                        pixbuf_cr[i] = pixbuf_cr[i+1];
                    }
                    if (x < (width/HSC_SAMPLES_PER_CLOCK))
                    {
                        pixbuf_cb[PIXBUF_C_DEPTH-1].val[0] = mpix_cb.val[k/shift];
                        pixbuf_cr[PIXBUF_C_DEPTH-1].val[0] = mpix_cr.val[k/shift];
                    }
                    else //right edge padding
                    {
                        pixbuf_cb[PIXBUF_C_DEPTH-1].val[0] = mpix_cb.val[(HSC_SAMPLES_PER_CLOCK/shift)-1];
                        pixbuf_cr[PIXBUF_C_DEPTH-1].val[0] = mpix_cr.val[(HSC_SAMPLES_PER_CLOCK/shift)-1];
                    }
                }
                //left edge padding
                if (first_pix==1)
                {
                    for(I16 i=(PIXBUF_C_DEPTH-HSC_SAMPLES_PER_CLOCK); i>=0; --i)
                    {
                        pixbuf_cb[i].val[0] = mpix_cb.val[0];
                        pixbuf_cr[i].val[0] = mpix_cr.val[0];
                    }
                }
            }

            for(k=0; k<HSC_SAMPLES_PER_CLOCK; ++k)
            {

#if (HSC_SAMPLES_PER_CLOCK == 1)
                odd_col = (out_x & 1);

#else
                odd_col = (k & 1);

#endif

                long filt_res0, filt_res1;
                outpix.val[k*3] = pixbuf_y[center_tap+k].val[0];
                if (colorMode == yuv444)
                {
                    // 444 to 422 fixed coef filtering [ 1/4 1/2 1/4 ]
                    // luma component
                    if(odd_col == 0)
                    {
                        // filter
                        filt_res0 = (pixbuf_cb[0+((k/2)*2)].val[0] + 2*pixbuf_cb[1+((k/2)*2)].val[0] + pixbuf_cb[2+((k/2)*2)].val[0] + 2) / 4;
                        filt_res1 = (pixbuf_cr[0+((k/2)*2)].val[0] + 2*pixbuf_cr[1+((k/2)*2)].val[0] + pixbuf_cr[2+((k/2)*2)].val[0] + 2) / 4;
                    }
                    outpix.val[k*3+1] = (odd_col) ? filt_res1 : filt_res0;
                    outpix.val[k*3+2] =  0;
                }
                else
                {
                    // 422 to 444 fixed coef filtering [ 1/2 1/2 ]
                    // luma component

                    if (odd_col) // odd cols (1, 3, 5, etc)
                    {
                        //interpolate by averaging nearest neighbors
                        outpix.val[k*3+1] = (pixbuf_cb[2+(k/2)].val[0] + pixbuf_cb[1+(k/2)].val[0] + 1) / 2;
                        outpix.val[k*3+2] = (pixbuf_cr[2+(k/2)].val[0] + pixbuf_cr[1+(k/2)].val[0] + 1) / 2;
                    }
                    else // even cols (0, 2, 4, etc)
                    {
                        // passthru co-sited pixel
                        outpix.val[k*3+1] = pixbuf_cb[1+(k/2)].val[0];
                        outpix.val[k*3+2] = pixbuf_cr[1+(k/2)].val[0];
                    }
                }

            }

            if((y>=0) && (out_x>=0))
            {
                if(bPassThru)
                	outImg << inpix;
                else
                	outImg << outpix;
            }
        }
    }
}
#endif

#if (HSC_ENABLE_420==1)
#define MAX_COLS                HSC_MAX_WIDTH
#define NUM_VIDEO_COMPONENTS    HSC_NR_COMPONENTS

#define KERNEL_V_SIZE   3
#define CHROMA_LINES    3
#define LUMA_LINES      2

typedef hls::Scalar<1, PIXEL_TYPE> Y_PIXEL;
typedef hls::Scalar<HSC_SAMPLES_PER_CLOCK, PIXEL_TYPE>  Y_MULTI_PIXEL;
typedef hls::Scalar<1, PIXEL_TYPE> C_PIXEL;
typedef hls::Scalar<HSC_SAMPLES_PER_CLOCK, PIXEL_TYPE>  C_MULTI_PIXEL;

typedef hls::LineBuffer<LUMA_LINES,   (MAX_COLS/HSC_SAMPLES_PER_CLOCK), Y_MEM_PIXEL_TYPE>    LINE_BUFFER_Y;
typedef hls::LineBuffer<CHROMA_LINES, (MAX_COLS/HSC_SAMPLES_PER_CLOCK), C_MEM_PIXEL_TYPE>    LINE_BUFFER_C;

void v_vcresampler_core(HSC_STREAM_MULTIPIX& srcImg,
                        U16 &height,
                        U16 &width,
                        const U8 &inColorMode,
                        bool &bPassThru,
						HSC_STREAM_MULTIPIX& outImg)
{
    I16 y,x,k;
    I16 yOffset; //offset between streaming row and processing row
    I16 xOffset = 0;
    I16 loopHeight;
    I16 loopWidth = (width/HSC_SAMPLES_PER_CLOCK) + xOffset;
    I16 out_y, out_x;
    I16 ChromaLine;

    LINE_BUFFER_Y linebuf_y;
    LINE_BUFFER_C linebuf_c;

    YUV_MULTI_PIXEL pix;
    YUV_MULTI_PIXEL outpix;
    Y_MULTI_PIXEL mpix_y;
    C_MULTI_PIXEL mpix_c;
    //local storage for line buffer column (to avoid multiple read clients on BRAM)
    Y_MULTI_PIXEL pixbuf_y[LUMA_LINES+1];
    C_MULTI_PIXEL pixbuf_c[KERNEL_V_SIZE];

#if (USE_URAM == 1)
#pragma HLS RESOURCE variable=&linebuf_y core=XPM_MEMORY uram
#pragma HLS RESOURCE variable=&linebuf_c core=XPM_MEMORY uram
#endif

    if (bPassThru)
    {
        yOffset = 0;
    }
    else if (inColorMode == yuv420)
    {
        yOffset = 2;
    }
    else
    {
        yOffset = 1;
    }


    loopHeight = height + yOffset;

    for(y=0; y<loopHeight; ++y)
    {

        for(x=0; x<loopWidth; ++x)
        {

#pragma HLS LOOP_FLATTEN OFF
#pragma HLS PIPELINE II=1

            //processing (output) pixel coordinates
            out_y = y - yOffset;
            out_x = x; //x - xOffset;

            if (y < height)
            {
                srcImg >> pix;
                // create multi-pixel samples of luma only or chroma only
                for (I16 s=0; s<HSC_SAMPLES_PER_CLOCK; ++s)
                {
                    mpix_y.val[s] = pix.val[s*NUM_VIDEO_COMPONENTS];
                    mpix_c.val[s] = pix.val[s*NUM_VIDEO_COMPONENTS+1];
                }
            }

            //luma line buffer
#pragma HLS ARRAY_PARTITION variable=&pixbuf_y      complete dim=0
            // get column of pixels from the line buffer to local pixel array
            Y_MEM_PIXEL_TYPE InYPix;
            for (I16 i=0; i<LUMA_LINES; i++)
            {
                Y_MEM_PIXEL_TYPE LineBufVal = linebuf_y.getval(i,x);
                for (int l = 0; l < HSC_SAMPLES_PER_CLOCK; l++)
                {
                    int start = (l * HSC_BITS_PER_COMPONENT);
                    pixbuf_y[LUMA_LINES-1-i].val[l] = LineBufVal(start + HSC_BITS_PER_COMPONENT -1, start);
                    if(i==0)
                    InYPix(start + HSC_BITS_PER_COMPONENT -1, start) = mpix_y.val[l];
                }
            }

            // get new pixels from stream
           /* Y_MEM_PIXEL_TYPE InYPix;
            for (int l = 0; l < HSC_SAMPLES_PER_CLOCK; l++)
            {
                int start = (l * HSC_BITS_PER_COMPONENT);
                InYPix(start + HSC_BITS_PER_COMPONENT -1, start) = mpix_y.val[l];
            }*/
            linebuf_y.insert_bottom(InYPix, x); //pix does not change after final line - bottom edge padding
            pixbuf_y[LUMA_LINES] = mpix_y;

            for (I16 i=LUMA_LINES-1; i>0;  i--) // for circular buffer implementation
            {
                //on first line, fill line buffer with first pixel value - top edge padding
                Y_MEM_PIXEL_TYPE LineBufVal;
                Y_MULTI_PIXEL PixBufVal;
                if(y>0)
                	PixBufVal = pixbuf_y[LUMA_LINES-i];
                else
                	PixBufVal = pixbuf_y[LUMA_LINES];
                for (int l = 0; l < HSC_SAMPLES_PER_CLOCK; l++)
                {
                        int start = (l* HSC_BITS_PER_COMPONENT);
                        LineBufVal(start + HSC_BITS_PER_COMPONENT -1, start) = PixBufVal.val[l];
                }
                linebuf_y.val[i][x] = LineBufVal;
            }

            // chroma line buffer
#pragma HLS ARRAY_PARTITION variable=&pixbuf_c      complete dim=0
            ChromaLine = ((y&1) && (inColorMode == yuv420)) ? 0 : 1;
            // get column of pixels from the line buffer to local pixel array
            for (I16 i=0; i<CHROMA_LINES-1; i++)
            {
                C_MULTI_PIXEL CBufVal;
                C_MEM_PIXEL_TYPE LineBufVal = linebuf_c.getval(i,x);
                for (int l = 0; l < HSC_SAMPLES_PER_CLOCK; l++)
                {
                    int start = (l * HSC_BITS_PER_COMPONENT);
                    CBufVal.val[l] = LineBufVal(start + HSC_BITS_PER_COMPONENT -1, start);
                }
                if (ChromaLine == 1)
                {
                    pixbuf_c[CHROMA_LINES-1-i-1] = CBufVal;
                }
                else
                {
                    pixbuf_c[CHROMA_LINES-1-i] = CBufVal;
                }
            }

            if (ChromaLine == 1)
            {
                if (y < height)
                {
                    pixbuf_c[CHROMA_LINES-1] = mpix_c;
                    C_MEM_PIXEL_TYPE InCPix;
                    for (int l = 0; l < HSC_SAMPLES_PER_CLOCK; l++)
                     {
                     int start = (l * HSC_BITS_PER_COMPONENT);
                     InCPix(start + HSC_BITS_PER_COMPONENT -1, start) = mpix_c.val[l];
                     }
                     linebuf_c.insert_bottom(InCPix, x); //push read data to line buffer
                }
                else
                {
                    pixbuf_c[CHROMA_LINES-1] = pixbuf_c[CHROMA_LINES-2];
                }
            for (I16 i=CHROMA_LINES-2; i>0;  i--) // for circular buffer implementation
            {
            //on first line, fill line buffer with first pixel value - top edge padding
            C_MEM_PIXEL_TYPE LineBufVal;
            C_MULTI_PIXEL PixBufVal;
            if(y>0)
            	PixBufVal = pixbuf_c[CHROMA_LINES-i-1];
            else
            	PixBufVal = pixbuf_c[CHROMA_LINES-1];
            for (int l = 0; l < HSC_SAMPLES_PER_CLOCK; l++)
              {
            int start = (l* HSC_BITS_PER_COMPONENT);
            LineBufVal(start + HSC_BITS_PER_COMPONENT -1, start) = PixBufVal.val[l];
              }
             linebuf_c.val[i][x] = LineBufVal;
              }
              }
            else
            {
                C_MEM_PIXEL_TYPE LineBufCVal = linebuf_c.getval(CHROMA_LINES-1,x);
                for (int l = 0; l < HSC_SAMPLES_PER_CLOCK; l++)
                {
                    int start = (l * HSC_BITS_PER_COMPONENT);
                    pixbuf_c[0].val[l] = LineBufCVal(start + HSC_BITS_PER_COMPONENT -1, start);
                }
            }

           /* if (ChromaLine == 1)
            {
                if (y < height) //only insert new pixel if reading in new chroma line from image - bottom edge padding
                {
                    C_MEM_PIXEL_TYPE InCPix;
                    for (int l = 0; l < HSC_SAMPLES_PER_CLOCK; l++)
                    {
                            int start = (l * HSC_BITS_PER_COMPONENT);
                            InCPix(start + HSC_BITS_PER_COMPONENT -1, start) = mpix_c.val[l];
                    }
                    linebuf_c.insert_bottom(InCPix, x); //push read data to line buffer
                }
                for (I16 i=CHROMA_LINES-2; i>0;  i--) // for circular buffer implementation
                {
                    //on first line, fill line buffer with first pixel value - top edge padding
                    C_MEM_PIXEL_TYPE LineBufVal;
                    C_MULTI_PIXEL PixBufVal = (y>0) ? pixbuf_c[CHROMA_LINES-i-1] : pixbuf_c[CHROMA_LINES-1];
                    for (int l = 0; l < HSC_SAMPLES_PER_CLOCK; l++)
                    {
                            int start = (l* HSC_BITS_PER_COMPONENT);
                            LineBufVal(start + HSC_BITS_PER_COMPONENT -1, start) = PixBufVal.val[l];
                    }
                    linebuf_c.val[i][x] = LineBufVal;
                }
            }*/

            for(k=0; k<HSC_SAMPLES_PER_CLOCK; ++k)
            {
                {
                    if (inColorMode == yuv422)
                    {
                        // 422 to 420 fixed coef filtering [ 1/4 1/2 1/4 ]
                        // luma component
                        outpix.val[k*NUM_VIDEO_COMPONENTS] = pixbuf_y[1].val[k];
                        if (out_y & 1) // odd rows (1, 3, 5, etc)
                        {
                            // set rows with no chroma information to zero
                            outpix.val[k*NUM_VIDEO_COMPONENTS+1] = 0;
                        }
                        else // even rows (0, 2, 4, etc)
                        {
                            // interpolate co-sited pixel
                            outpix.val[k*NUM_VIDEO_COMPONENTS+1] = (pixbuf_c[0].val[k] + 2*pixbuf_c[1].val[k] + pixbuf_c[2].val[k] + 2) / 4;
                        }
                    }
                    else
                    {
                        // 420 to 422 fixed coef filtering [ 1/2 1/2 ]
                        // luma component
                        outpix.val[k*NUM_VIDEO_COMPONENTS] = pixbuf_y[0].val[k];
                        if (out_y & 1) // odd rows (1, 3, 5, etc)
                        {
                            //interpolate by averaging nearest neighbors
                            outpix.val[k*NUM_VIDEO_COMPONENTS+1] = (pixbuf_c[1].val[k] + pixbuf_c[2].val[k] + 1) / 2;
                        }
                        else // even rows (0, 2, 4, etc)
                        {
                            // passthru co-sited pixel
                            outpix.val[k*NUM_VIDEO_COMPONENTS+1] = pixbuf_c[1].val[k];
                        }
                    }
                }

#if (NUM_VIDEO_COMPONENTS == 3)
                // set unused third video component to zero
                outpix.val[k*NUM_VIDEO_COMPONENTS+2] = 0;
#endif

            } // for (k

            if((out_y>=0) && (out_x>=0))
            {
                if(bPassThru)
                	outImg << pix;
                else
                	outImg << outpix;
            }
        }
    }
}
#endif

#if (HSC_ENABLE_CSC==1)
static void v_csc_core(
	HSC_STREAM_MULTIPIX& srcImg,
    U16 &height,
    U16 &width,
    U8 &colorMode,
    bool &bPassThru,
	HSC_STREAM_MULTIPIX& outImg)
{
    U16 y,x,k;

    YUV_MULTI_PIXEL srcpix;
    YUV_MULTI_PIXEL dstpix;

    assert(width<=HSC_MAX_WIDTH);
    assert(height<=HSC_MAX_HEIGHT);

	for (y = 0; y < (height); ++y)
	{
		for (x = 0; x < (width)/HSC_SAMPLES_PER_CLOCK; ++x)
		{

#pragma HLS LOOP_FLATTEN OFF
#pragma HLS PIPELINE

			srcImg >> srcpix;

			for (k = 0; k < HSC_SAMPLES_PER_CLOCK; ++k)
			{
				PIXEL_TYPE r_y, g_u, b_v;

				r_y = srcpix.val[k*HSC_NR_COMPONENTS + 0];
				g_u = srcpix.val[k*HSC_NR_COMPONENTS + 1];
				b_v = srcpix.val[k*HSC_NR_COMPONENTS + 2];

				// yuv to rgb
				int Cr = b_v - (1<<(HSC_BITS_PER_COMPONENT-1));
				int Cb = g_u - (1<<(HSC_BITS_PER_COMPONENT-1));
				int r = (int)r_y + (((int)Cr * 1733) >> 10);
				int g = (int)r_y - (((int)Cb * 404 + (int)Cr * 595) >> 10);
				int b = (int)r_y + (((int)Cb * 2081) >> 10);

				// rgb to yuv
				int y = (306*(int)r_y + 601*(int)g_u + 117*(int)b_v)>>10;
				int u = (1<<(HSC_BITS_PER_COMPONENT-1)) + ((((int)b_v-(int)y)*504)>>10);
				int v = (1<<(HSC_BITS_PER_COMPONENT-1)) + ((((int)r_y-(int)y)*898)>>10);

				dstpix.val[k*HSC_NR_COMPONENTS + 0] = MAX(MIN((colorMode!=rgb) ? r : y, (1<<HSC_BITS_PER_COMPONENT)-1), 0);
				dstpix.val[k*HSC_NR_COMPONENTS + 1] = MAX(MIN((colorMode!=rgb) ? g : u, (1<<HSC_BITS_PER_COMPONENT)-1), 0);
				dstpix.val[k*HSC_NR_COMPONENTS + 2] = MAX(MIN((colorMode!=rgb) ? b : v, (1<<HSC_BITS_PER_COMPONENT)-1), 0);
			}
			if(bPassThru)
				outImg << srcpix;
			else
				outImg << dstpix;
		}
    }
}
#endif
