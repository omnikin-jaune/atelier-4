#ifndef __SYNTHESIS__
#include <stdio.h>
#endif
#include <assert.h>
#include "v_vscaler_config.h"
#include "v_vscaler.h"

#define MAX_COLS                VSC_MAX_WIDTH
#define SAMPLES_PER_CLOCK       VSC_SAMPLES_PER_CLOCK    // 1, 2, 4
#define NUM_VIDEO_COMPONENTS    VSC_NR_COMPONENTS


const U8 rgb = 0;
const U8 yuv444 = 1;
const U8 yuv422 = 2;
const U8 yuv420 = 3;

//Static functions
static int AXIvideo2MultiPixStream(VSC_AXI_STREAM_IN& AXI_video_strm,
                                   VSC_STREAM_MPIX& img,
                                   U16 &Height,
                                   U16 &WidthIn,
                                   U8 &ColorMode
                                   );

static int MultiPixStream2AXIvideo(VSC_STREAM_MPIX& StrmMPix,
                                   VSC_AXI_STREAM_OUT& AXI_video_strm,
                                   U16 &Height,
                                   U16 &Width,
                                   U8 &ColorMode
                                   );

void v_vcresampler_core(VSC_STREAM_MPIX& srcImg,
                        U16 &height,
                        U16 &width,
                        U8 &inColorMode,
                        U8 &outColorMode,
                        VSC_STREAM_MPIX& outImg);

#if (VSC_SCALE_MODE == VSC_BILINEAR)
static void vscale_core_bilinear(VSC_STREAM_MPIX& SrcImg,
                                 VSC_HW_STRUCT_REG *HwReg,
                                 VSC_STREAM_MPIX& OutImg
                                 );

static void vscale_bilinear (YUV_MULTI_PIXEL PixArray[VSC_TAPS],
                           U8 PhaseV,
                           YUV_MULTI_PIXEL *OutPix
                           );
#endif

#if  (VSC_SCALE_MODE == VSC_BICUBIC)
static void vscale_core_bicubic(VSC_STREAM_MPIX& SrcImg,
                                VSC_HW_STRUCT_REG *HwReg,
                                VSC_STREAM_MPIX& OutImg
                                );

static void vscale_bicubic (YUV_MULTI_PIXEL PixArray[VSC_TAPS],
                            U8 PhaseV,
                            YUV_MULTI_PIXEL *OutPix
                            );
#endif

#if  (VSC_SCALE_MODE == VSC_POLYPHASE)
static void vscale_core_polyphase(VSC_STREAM_MPIX& SrcImg,
                                  U16 &HeightIn,
								  U16 &Width,
								  U16 &HeightOut,
								  U32 &LineRate,
								  U8  &ColorMode,
								  I16 vfltCoeff[VSC_PHASES][VSC_TAPS],
                                  VSC_STREAM_MPIX& OutImg
                                  );

static void vscale_polyphase (I16 FiltCoeff[VSC_PHASES][VSC_TAPS],
                              YUV_MULTI_PIXEL PixArray[VSC_TAPS],
                              U8 PhaseV,
                              YUV_MULTI_PIXEL *OutPix
                              );
#endif

/*********************************************************************************
* Function:    vscale_top
* Parameters:  Stream of input/output pixels, image resolution, type of scaling etc
* Return:
* Description: Top level function to perform horizontal image resizing
* submodules - AXIvideo2MultiPixStream
*              hscale_core
*              MultiPixStream2AXIvideo
**********************************************************************************/
void v_vscaler (VSC_AXI_STREAM_IN& s_axis_video,
		        U16 &HeightIn,
				U16 &Width,
				U16 &HeightOut,
				U32 &LineRate,
				U8  &ColorMode,
				I16 vfltCoeff[VSC_PHASES][VSC_TAPS],
                VSC_AXI_STREAM_OUT& m_axis_video
                 )
{

__xilinx_ip_top(0);

#pragma HLS INTERFACE port=&s_axis_video AXIS register
#pragma HLS INTERFACE port=&m_axis_video AXIS register

#pragma HLS INTERFACE s_axilite port=return            bundle=CTRL
#if (VSC_SCALE_MODE == VSC_POLYPHASE)
#pragma HLS INTERFACE s_axilite port=&vfltCoeff   bundle=CTRL offset=0x800
#endif
#pragma HLS INTERFACE s_axilite port=&HeightIn        bundle=CTRL
#pragma HLS INTERFACE s_axilite port=&Width           bundle=CTRL
#pragma HLS INTERFACE s_axilite port=&HeightOut       bundle=CTRL
#pragma HLS INTERFACE s_axilite port=&LineRate        bundle=CTRL
#pragma HLS INTERFACE s_axilite port=&ColorMode       bundle=CTRL

#pragma HLS INTERFACE ap_stable port=&HeightIn
#pragma HLS INTERFACE ap_stable port=&Width
#pragma HLS INTERFACE ap_stable port=&HeightOut
#pragma HLS INTERFACE ap_stable port=&LineRate
#pragma HLS INTERFACE ap_stable port=&ColorMode

#if (VSC_SCALE_MODE == VSC_POLYPHASE)
#pragma HLS RESOURCE variable=&vfltCoeff core=RAM_1P_BRAM
#endif

VSC_HW_STRUCT_REG HwReg;
HwReg.ColorMode = ColorMode;
HwReg.HeightIn = HeightIn;
HwReg.HeightOut = HeightOut;
HwReg.LineRate =  LineRate;
HwReg.Width = Width;
U8 ColorMode_vcr;

// #pragma HLS INTERFACE ap_ctrl_hs port=&return
assert ((HwReg.HeightOut <= VSC_MAX_HEIGHT) && (HwReg.Width <= VSC_MAX_WIDTH));
assert (HwReg.HeightIn <= VSC_MAX_HEIGHT);

ColorMode_vcr = (HwReg.ColorMode==yuv420) ? yuv422 : HwReg.ColorMode;
VSC_STREAM_MPIX SrcYUV;
VSC_STREAM_MPIX SrcYUV422;
VSC_STREAM_MPIX OutYUV;
VSC_STREAM_MPIX OutYUV420;

#pragma HLS DATAFLOW
#pragma HLS stream variable=SrcYUV depth=16
#pragma HLS stream variable=SrcYUV422 depth=16
#pragma HLS stream variable=OutYUV depth=16
#pragma HLS stream variable=OutYUV420 depth=16

    AXIvideo2MultiPixStream(s_axis_video, SrcYUV, HwReg.HeightIn, HwReg.Width, HwReg.ColorMode);

#if (VSC_ENABLE_420==1)
    v_vcresampler_core(SrcYUV, HwReg.HeightIn, HwReg.Width, HwReg.ColorMode, ColorMode_vcr, SrcYUV422);
    #define STREAM_IN SrcYUV422
#else
    #define STREAM_IN SrcYUV
#endif

    #if  (VSC_SCALE_MODE == VSC_BILINEAR)
        vscale_core_bilinear(STREAM_IN, &HwReg, OutYUV);
	#endif

    #if  (VSC_SCALE_MODE == VSC_BICUBIC)
        vscale_core_bicubic(STREAM_IN, &HwReg, OutYUV);
    #endif

	#if (VSC_SCALE_MODE == VSC_POLYPHASE)
    	vscale_core_polyphase(STREAM_IN, HwReg.HeightIn, HwReg.Width, HwReg.HeightOut, HwReg.LineRate, HwReg.ColorMode, vfltCoeff, OutYUV);
    #endif

    MultiPixStream2AXIvideo(OutYUV, m_axis_video,  HwReg.HeightOut, HwReg.Width, ColorMode_vcr);

} // hscale_top

/*********************************************************************************
* Function:    vscale_core,
* Parameters:  Stream of input/output, image resolution, type of scaling etc
* Return:
* Description: Perform vertical image resizing
* Sub modules - vscale_linear
*               vscale_cubic
*               vscale_poly
**********************************************************************************/

#if  (VSC_SCALE_MODE == VSC_POLYPHASE)

void vscale_core_polyphase (VSC_STREAM_MPIX& SrcImg,
                            //VSC_HW_STRUCT_REG *HwReg,
		                    U16 &HeightIn,
						    U16 &Width,
						    U16 &HeightOut,
						    U32 &LineRate,
						    U8  &ColorMode,
						    I16 vfltCoeff[VSC_PHASES][VSC_TAPS],
                            VSC_STREAM_MPIX& OutImg
                            )
{
    U16 InLines  = HeightIn;
    U16 InPixels = Width;
    U16 OutLines = HeightOut;
    U32 Rate     = LineRate;

    bool GetNewLine;
    bool OutputWriteEn;

    U16  PixArrayLoc;
    U16  TotalLines, YLoopSize, XLoopSize;
    YUV_MULTI_PIXEL SrcPix, OutPix;
    YUV_MULTI_PIXEL PixArray[VSC_TAPS];
    I16 FiltCoeff[VSC_PHASES][VSC_TAPS];
    MEM_LINE_BUFFER LineBuf;
    MEM_PIXEL_TYPE InPix;

#if (USE_URAM == 1)
#pragma HLS RESOURCE variable=&LineBuf core=XPM_MEMORY uram
#endif

#pragma HLS RESOURCE variable=&FiltCoeff core=RAM_1P_LUTRAM


    assert(OutLines <= VSC_MAX_HEIGHT);

    // Loop over max of input and output resolution, add run-in/out for taps
    TotalLines   = (OutLines > InLines) ? OutLines : InLines;
    YLoopSize    = TotalLines + (VSC_TAPS>>1);
    XLoopSize =   (InPixels + (VSC_SAMPLES_PER_CLOCK-1)) / VSC_SAMPLES_PER_CLOCK;

    assert(YLoopSize < (VSC_MAX_HEIGHT+VSC_TAPS));
    assert(XLoopSize <= VSC_MAX_WIDTH);

#pragma HLS ARRAY_PARTITION variable=&SrcPix      complete dim=0
#pragma HLS ARRAY_PARTITION variable=&PixArray    complete dim=0
#pragma HLS ARRAY_PARTITION variable=&OutPix      complete dim=0
#pragma HLS ARRAY_PARTITION variable=&FiltCoeff   complete dim=2
//#pragma HLS RESOURCE variable=&FiltCoeff core=RAM_1P_LUTRAM

    GetNewLine      = 1;
    PixArrayLoc     = 0;

    int GetLine = 0;
    U8 PhaseV = 0;
    U32 offset = 0;
    int ReadLoc = 0;
    int WriteLoc = 0;
    int WriteLocNext = 0;

    // Get the coefficients to local array
loop_init_coeff_phase:
    for (int i = 0; i < VSC_PHASES; i++)
    {
loop_init_coeff_tap:
        for (int j = 0; j < VSC_TAPS; j++)
        {
            FiltCoeff[i][j] = vfltCoeff[i][j];
        }
    }

loop_height:
    for (U16 y = 0; y < YLoopSize; y++)
    {
        OutputWriteEn  = 0;

        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        //
        // Step and phase calculation
        //
        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        if (y >= (VSC_TAPS>>1))
        {

            PhaseV = ((offset>>(STEP_PRECISION_SHIFT-VSC_PHASE_SHIFT))) & (VSC_PHASES-1);
            WriteLoc = WriteLocNext;

            GetNewLine = 0;
            if ((offset >> STEP_PRECISION_SHIFT) != 0)
            {
                // Take a new sample from input
                ReadLoc = GetLine;
                GetNewLine = 1;
                GetLine++;
                PixArrayLoc++;
                offset = offset - (1<<STEP_PRECISION_SHIFT);
                OutputWriteEn = 0;
                WriteLocNext = WriteLoc;
            }

            if (((offset >> STEP_PRECISION_SHIFT) == 0) && (WriteLoc<(U32)OutLines))
            {
                // Produce a new output sample
                offset = offset + Rate;
                OutputWriteEn = 1;
                WriteLocNext = WriteLoc+1;
            }
//          printf("Readloc %d writeloc %d GetNewLine %d  OutputWriteEn %d PhaseV %d offset %d\n", ReadLoc, WriteLoc, GetNewLine, OutputWriteEn, PhaseV, offset);
        }

loop_width_for_procpix:
        for (U16 x = 0; x < XLoopSize; x++) // the loop runs for the max of (in,out)
        {
#pragma HLS LOOP_FLATTEN OFF
#pragma HLS PIPELINE

            /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            //
            // Line buffer control
            //
            /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

            for (I16 i = 0; i < VSC_TAPS; i++)// get cols of pixels from the line buffer to local pixel array
            {
                MEM_PIXEL_TYPE LineBufVal = LineBuf.getval(i,x);
                for (int l = 0; l < VSC_SAMPLES_PER_CLOCK; l++)
                {
                    for (int k= 0; k < VSC_NR_COMPONENTS; k++)
                    {
                        int start = (k + l * VSC_NR_COMPONENTS) * VSC_BITS_PER_COMPONENT;
                        PixArray[VSC_TAPS-1-i].val[k + l * VSC_NR_COMPONENTS] = LineBufVal(start + VSC_BITS_PER_COMPONENT -1, start);
                    }
                }
            }

            if ((GetNewLine == 1) || (y <= (VSC_TAPS>>1)))  // If new pixels to be filled in the line buffer
            {
                for (I16 i = 0; i < (VSC_TAPS-1);  i++)
                {
                    PixArray[i] =  PixArray[i+1];
                }
                if ((PixArrayLoc + (VSC_TAPS>>1)) < InLines) // get new pixels from stream
                {
                    SrcImg >> SrcPix;
                    for (int l = 0; l < VSC_SAMPLES_PER_CLOCK; l++)
                    {
                        for (int k= 0; k < VSC_NR_COMPONENTS; k++)
                        {
                            int start = (k + l * VSC_NR_COMPONENTS) * VSC_BITS_PER_COMPONENT;
                            InPix(start + VSC_BITS_PER_COMPONENT -1, start) = SrcPix.val[k + l * VSC_NR_COMPONENTS];
                        }
                    }
                    LineBuf.insert_bottom(InPix,x);
                    PixArray[VSC_TAPS-1] = SrcPix;
                } // get new pixels from stream

                for (int i = (VSC_TAPS-1); i > 0;  i--) // for circular buffer implementation
                {
                    MEM_PIXEL_TYPE LineBufVal;
                    YUV_MULTI_PIXEL PixArrayVal;
                    if(y > 0)
                    	PixArrayVal = PixArray[VSC_TAPS-1-i];
                    else
                    	PixArrayVal = PixArray[VSC_TAPS-1];
                    for (int l = 0; l < VSC_SAMPLES_PER_CLOCK; l++)
                    {
                        for (int k= 0; k < VSC_NR_COMPONENTS; k++)
                        {
                            int start = (k + l * VSC_NR_COMPONENTS) * VSC_BITS_PER_COMPONENT;
                            LineBufVal(start + VSC_BITS_PER_COMPONENT -1, start) = PixArrayVal.val[k + l * VSC_NR_COMPONENTS];
                        }
                    }
                    LineBuf.val[i][x] = LineBufVal;
                }
            }

            /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            //
            // Processing
            //
            /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            if (OutputWriteEn)
            {
                vscale_polyphase (FiltCoeff, PixArray, PhaseV, &OutPix);

                OutImg << OutPix;
            }
        }
    }
} // vscale_core_polyphase

void vscale_polyphase (I16 FiltCoeff[VSC_PHASES][VSC_TAPS],
                       YUV_MULTI_PIXEL PixArray[VSC_TAPS],
                       U8 PhaseV,
                       YUV_MULTI_PIXEL *OutPix
                       )
{
#pragma HLS inline
    U8 Phase = PhaseV;
    for (int s = 0; s < VSC_SAMPLES_PER_CLOCK; s++) // samples
    {
        for (int c = 0; c < VSC_NR_COMPONENTS; c++)
        {
#pragma HLS expression_balance off
            I32 sum = (COEFF_PRECISION>>1);  //MP todo 32 bits might not be enough
            I32 norm = 0;

            // Center tap is tap with index (VSC_TAPS>>1)-1, 0 is top most, VSC_TAPS-1 is bottom most pixel
            for (int t = 0; t < VSC_TAPS; t++)
            {
                sum += PixArray[t].val[VSC_NR_COMPONENTS*s+c] * FiltCoeff[Phase][t];
            }
            norm = sum >> COEFF_PRECISION_SHIFT;
            norm = CLAMP(norm, 0, (1<<VSC_BITS_PER_COMPONENT)-1);

            OutPix->val[VSC_NR_COMPONENTS*s+c] = (ap_uint<VSC_BITS_PER_COMPONENT>) norm;
        }
    }
}

#endif

#if  (VSC_SCALE_MODE == VSC_BILINEAR)

void vscale_core_bilinear (VSC_STREAM_MPIX& SrcImg,
                           VSC_HW_STRUCT_REG *HwReg,
                           VSC_STREAM_MPIX& OutImg
                          )
{
    U16 InLines  = HwReg->HeightIn;
    U16 InPixels = HwReg->Width;
    U16 OutLines = HwReg->HeightOut;
    U32 Rate     = HwReg->LineRate;

    bool GetNewLine;
    bool OutputWriteEn;

    U16  PixArrayLoc;
    U16  TotalLines, YLoopSize, XLoopSize;
    YUV_MULTI_PIXEL SrcPix, OutPix;
    YUV_MULTI_PIXEL PixArray[VSC_TAPS];
    MEM_LINE_BUFFER LineBuf;
    MEM_PIXEL_TYPE InPix;

#if (USE_URAM == 1)
#pragma HLS RESOURCE variable=&LineBuf core=XPM_MEMORY uram
#endif

    assert(OutLines <= VSC_MAX_HEIGHT);

    // Loop over max of input and output resolution, add run-in/out for taps
    TotalLines   = (OutLines > InLines) ? OutLines : InLines;
    YLoopSize    = TotalLines + VSC_TAPS;
    XLoopSize =   (InPixels + (VSC_SAMPLES_PER_CLOCK-1)) / VSC_SAMPLES_PER_CLOCK;

    assert(YLoopSize < (VSC_MAX_HEIGHT+VSC_TAPS));
    assert(XLoopSize <= VSC_MAX_WIDTH);

#pragma HLS ARRAY_PARTITION variable=&SrcPix      complete dim=0
#pragma HLS ARRAY_PARTITION variable=&PixArray    complete dim=0
#pragma HLS ARRAY_PARTITION variable=&OutPix      complete dim=0

    GetNewLine      = 1;
    PixArrayLoc     = 0;

    int GetLine = 0;
    U8 PhaseV = 0;
    U32 offset = 0;
    int ReadLoc = 0;
    int WriteLoc = 0;
    int WriteLocNext = 0;

loop_height:
    for (U16 y = 0; y < YLoopSize; y++)
    {
        OutputWriteEn  = 0;

        if (y >= (VSC_TAPS-1))
        {
            PhaseV = ((offset>>(STEP_PRECISION_SHIFT-VSC_PHASE_SHIFT))) & (VSC_PHASES-1);
            WriteLoc = WriteLocNext;

            GetNewLine = 0;
            if ((offset >> STEP_PRECISION_SHIFT) != 0)
            {
                // take a new sample from input, but don't process anything
                ReadLoc = GetLine;
                GetNewLine = 1;
                GetLine++;
                PixArrayLoc++;
                offset = offset - (1<<STEP_PRECISION_SHIFT);
                OutputWriteEn = 0;
                WriteLocNext = WriteLoc;
            }

            if (((offset >> STEP_PRECISION_SHIFT) == 0) && (WriteLoc<(U32)OutLines))
            {
                // produce a new output sample
                offset = offset + Rate;
                OutputWriteEn = 1;
                WriteLocNext = WriteLoc+1;
            }
        } // compute per line

loop_width_for_procpix:
        for (U16 x = 0; x < XLoopSize; x++) // the loop runs for the max of (in,out)
        {
#pragma HLS LOOP_FLATTEN OFF
#pragma HLS PIPELINE

loop_shift_pix:
            for (I16 i = 0; i < VSC_TAPS; i++)// get cols of pixels from the line buffer to local pixel array
            {
                MEM_PIXEL_TYPE LineBufVal = LineBuf.getval(i,x);
                for (int l = 0; l < VSC_SAMPLES_PER_CLOCK; l++)
                {
                    for (int k= 0; k < VSC_NR_COMPONENTS; k++)
                    {
                        int start = (k + l * VSC_NR_COMPONENTS) * VSC_BITS_PER_COMPONENT;
                        PixArray[VSC_TAPS-1-i].val[k + l * VSC_NR_COMPONENTS] = LineBufVal(start + VSC_BITS_PER_COMPONENT -1, start);
                    }
                }
            }

            if ((GetNewLine == 1) || (y < VSC_TAPS))  // If new pixels to be filled in the line buffer
            {
                for (I16 i = 0; i < (VSC_TAPS-1);  i++)
                {
                    PixArray[i] =  PixArray[i+1];
                }
                if ((PixArrayLoc + VSC_TAPS -1) < InLines) // get new pixels from stream
                {
                    SrcImg >> SrcPix;
                    for (int l = 0; l < VSC_SAMPLES_PER_CLOCK; l++)
                    {
                        for (int k= 0; k < VSC_NR_COMPONENTS; k++)
                        {
                            int start = (k + l * VSC_NR_COMPONENTS) * VSC_BITS_PER_COMPONENT;
                            InPix(start + VSC_BITS_PER_COMPONENT -1, start) = SrcPix.val[k + l * VSC_NR_COMPONENTS];
                        }
                    }
                    LineBuf.insert_bottom(InPix,x);
                    PixArray[VSC_TAPS-1] = SrcPix;
                } // get new pixels from stream

                for (I16 i = (VSC_TAPS-1); i > 0;  i--) // for circular buffer implementation
                {
                    MEM_PIXEL_TYPE LineBufVal;
                    YUV_MULTI_PIXEL PixArrayVal = (y >0) ? PixArray[VSC_TAPS-1-i] : PixArray[VSC_TAPS-1];
                    for (int l = 0; l < VSC_SAMPLES_PER_CLOCK; l++)
                    {
                        for (int k= 0; k < VSC_NR_COMPONENTS; k++)
                        {
                            int start = (k + l * VSC_NR_COMPONENTS) * VSC_BITS_PER_COMPONENT;
                            LineBufVal(start + VSC_BITS_PER_COMPONENT -1, start) = PixArrayVal.val[k + l * VSC_NR_COMPONENTS];
                        }
                    }
                    LineBuf.val[i][x] = LineBufVal;
                }
            } // GetNewLine

            if (y >= (VSC_TAPS-1))
            {

                vscale_bilinear (PixArray, PhaseV, &OutPix);

                if (OutputWriteEn) // line based write enable - high for a line or low for a line
                {
                    OutImg << OutPix;
                }
            }
        }
    }
} // hscale_core_bilinear

static void vscale_bilinear (YUV_MULTI_PIXEL PixArray[VSC_TAPS], U8 Phase, YUV_MULTI_PIXEL *OutPix)
{
#pragma HLS inline
    U32 sum, norm;

    for (int s = 0; s < VSC_SAMPLES_PER_CLOCK; s++) // samples
    {
        for (int c = 0; c < VSC_NR_COMPONENTS; c++)
        {
            // Instead of using 2 multpliers following expression: sum = (PixArray[0].p[s].val[c]* PhaseInv) + (PixArray[1].p[s].val[c] * Phase);
            // is rewritten using just 1 multiplier
            sum = (PixArray[0].val[VSC_NR_COMPONENTS*s+c]*(VSC_PHASES)) - (PixArray[0].val[VSC_NR_COMPONENTS*s+c] - PixArray[1].val[VSC_NR_COMPONENTS*s+c]) * Phase;

            norm = (sum + (VSC_PHASES>>1)) >> VSC_PHASE_SHIFT;
            norm = MIN(norm, (U32)((1<<VSC_BITS_PER_COMPONENT)-1));

            OutPix->val[VSC_NR_COMPONENTS*s+c] = (ap_uint<VSC_BITS_PER_COMPONENT>) norm;
        }
    } // samples
} // linear

#endif

#if  (VSC_SCALE_MODE == VSC_BICUBIC)

void vscale_core_bicubic (VSC_STREAM_MPIX& SrcImg,
                          VSC_HW_STRUCT_REG *HwReg,
                          VSC_STREAM_MPIX& OutImg
                          )
{
    U16 InLines  = HwReg->HeightIn;
    U16 InPixels = HwReg->Width;
    U16 OutLines = HwReg->HeightOut;
    U32 Rate     = HwReg->LineRate;

    bool GetNewLine;
    bool OutputWriteEn;

    U16  PixArrayLoc;
    U16  TotalLines, YLoopSize, XLoopSize;
    YUV_MULTI_PIXEL SrcPix, OutPix;
    YUV_MULTI_PIXEL PixArray[VSC_TAPS];
    MEM_LINE_BUFFER LineBuf;
    MEM_PIXEL_TYPE InPix;

#if (USE_URAM == 1)
#pragma HLS RESOURCE variable=&LineBuf core=XPM_MEMORY uram
#endif

    assert(OutLines <= VSC_MAX_HEIGHT);

    // Loop over max of input and output resolution, add run-in/out for taps
    TotalLines   = (OutLines > InLines) ? OutLines : InLines;
    YLoopSize    = TotalLines + VSC_TAPS;
    XLoopSize =   (InPixels + (VSC_SAMPLES_PER_CLOCK-1)) / VSC_SAMPLES_PER_CLOCK;

    assert(YLoopSize < (VSC_MAX_HEIGHT+VSC_TAPS));
    assert(XLoopSize <= VSC_MAX_WIDTH);

#pragma HLS ARRAY_PARTITION variable=&SrcPix      complete dim=0
#pragma HLS ARRAY_PARTITION variable=&PixArray    complete dim=0
#pragma HLS ARRAY_PARTITION variable=&OutPix      complete dim=0

    GetNewLine      = 1;
    PixArrayLoc     = 0;

    int GetLine = 0;
    U8 PhaseV = 0;
    U32 offset = 0;
    int ReadLoc = 0;
    int WriteLoc = 0;
    int WriteLocNext = 0;

loop_height:
    for (U16 y = 0; y < YLoopSize; y++)
    {
        OutputWriteEn  = 0;

        if (y >= (VSC_TAPS-1-1))
        {
            PhaseV = ((offset>>(STEP_PRECISION_SHIFT-VSC_PHASE_SHIFT))) & (VSC_PHASES-1);
            WriteLoc = WriteLocNext;

            GetNewLine = 0;
            if ((offset >> STEP_PRECISION_SHIFT) != 0)
            {
                // take a new sample from input, but don't process anything
                ReadLoc = GetLine;
                GetNewLine = 1;
                GetLine++;
                PixArrayLoc++;
                offset = offset - (1<<STEP_PRECISION_SHIFT);
                OutputWriteEn = 0;
                WriteLocNext = WriteLoc;
            }

            if (((offset >> STEP_PRECISION_SHIFT) == 0) && (WriteLoc<(U32)OutLines))
            {
                // produce a new output sample
                offset = offset + Rate;
                OutputWriteEn = 1;
                WriteLocNext = WriteLoc+1;
            }
        } // compute per line

loop_width_for_procpix:
        for (U16 x = 0; x < XLoopSize; x++) // the loop runs for the max of (in,out)
        {
#pragma HLS LOOP_FLATTEN OFF
#pragma HLS PIPELINE

loop_shift_pix:
            for (I16 i = 0; i < VSC_TAPS; i++)// get cols of pixels from the line buffer to local pixel array
            {
                MEM_PIXEL_TYPE LineBufVal = LineBuf.getval(i,x);
                for (int l = 0; l < VSC_SAMPLES_PER_CLOCK; l++)
                {
                    for (int k= 0; k < VSC_NR_COMPONENTS; k++)
                    {
                        int start = (k + l * VSC_NR_COMPONENTS) * VSC_BITS_PER_COMPONENT;
                        PixArray[VSC_TAPS-1-i].val[k + l * VSC_NR_COMPONENTS] = LineBufVal(start + VSC_BITS_PER_COMPONENT -1, start);
                    }
                }
            }

            if ((GetNewLine == 1) || (y < VSC_TAPS-1))  // If new pixels to be filled in the line buffer
            {
                for (I16 i = 0; i < (VSC_TAPS-1);  i++)
                {
                    PixArray[i] =  PixArray[i+1];
                }
                if ((PixArrayLoc + VSC_TAPS - 2) < InLines) // get new pixels from stream
                {
                    SrcImg >> SrcPix;
                    for (int l = 0; l < VSC_SAMPLES_PER_CLOCK; l++)
                    {
                        for (int k= 0; k < VSC_NR_COMPONENTS; k++)
                        {
                            int start = (k + l * VSC_NR_COMPONENTS) * VSC_BITS_PER_COMPONENT;
                            InPix(start + VSC_BITS_PER_COMPONENT -1, start) = SrcPix.val[k + l * VSC_NR_COMPONENTS];
                        }
                    }
                    LineBuf.insert_bottom(InPix,x);
                    PixArray[VSC_TAPS-1] = SrcPix;
                } // get new pixels from stream

                for (I16 i = (VSC_TAPS-1); i > 0;  i--) // for circular buffer implementation
                {
                    MEM_PIXEL_TYPE LineBufVal;
                    YUV_MULTI_PIXEL PixArrayVal = (y >0) ? PixArray[VSC_TAPS-1-i] : PixArray[VSC_TAPS-1];
                    for (int l = 0; l < VSC_SAMPLES_PER_CLOCK; l++)
                    {
                        for (int k= 0; k < VSC_NR_COMPONENTS; k++)
                        {
                            int start = (k + l * VSC_NR_COMPONENTS) * VSC_BITS_PER_COMPONENT;
                            LineBufVal(start + VSC_BITS_PER_COMPONENT -1, start) = PixArrayVal.val[k + l * VSC_NR_COMPONENTS];
                        }
                    }
                    LineBuf.val[i][x] = LineBufVal;
                }
            } // GetNewLine

            if (y >= (VSC_TAPS-1)-1)
            {
                vscale_bicubic(PixArray, PhaseV, &OutPix);

                if (OutputWriteEn) // line based write enable - high for a line or low for a line
                {
                    OutImg << OutPix;
                }
            }
        }
    }
}

void vscale_bicubic (YUV_MULTI_PIXEL PixArray[VSC_TAPS],
                     U8 Phase,
                     YUV_MULTI_PIXEL *OutPix
                     )
{
#pragma HLS inline
    I64 sum;
    I32 norm;
    I32 a, b, c;
    ap_uint<28> d;
    ap_int<52> ax3;
    ap_int<40> bx2;
    ap_int<28> cx;

    for (int s = 0; s < VSC_SAMPLES_PER_CLOCK; s++) // samples
    {
        const int ArrayLocM1 = 0;
        const int ArrayLoc = 1;
        const int ArrayLocP1 = 2;
        const int ArrayLocP2 = 3;

        for (int i = 0; i < VSC_NR_COMPONENTS; i++)
        {
            a = ((PixArray[ArrayLoc].val[VSC_NR_COMPONENTS*s+i] * 3) - (PixArray[ArrayLocP1].val[VSC_NR_COMPONENTS*s+i] * 3) - (PixArray[ArrayLocM1].val[VSC_NR_COMPONENTS*s+i]* 1) + (PixArray[ArrayLocP2].val[VSC_NR_COMPONENTS*s+i] * 1)) >> 1;
            b = ((PixArray[ArrayLocP1].val[VSC_NR_COMPONENTS*s+i] * 4) - (PixArray[ArrayLoc].val[VSC_NR_COMPONENTS*s+i] * 5) + (PixArray[ArrayLocM1].val[VSC_NR_COMPONENTS*s+i] * 2) - (PixArray[ArrayLocP2].val[VSC_NR_COMPONENTS*s+i] * 1)) >> 1;
            c = ((PixArray[ArrayLocP1].val[VSC_NR_COMPONENTS*s+i] * 1) - (PixArray[ArrayLocM1].val[VSC_NR_COMPONENTS*s+i] * 1)) >> 1;
            d = PixArray[ArrayLoc].val[VSC_NR_COMPONENTS*s+i] * VSC_PHASES;

            ax3 =  (((ap_int<52>) a * Phase * Phase * Phase) + VSC_PHASES) >> (VSC_PHASE_SHIFT +  VSC_PHASE_SHIFT);
            bx2 =  (((ap_int<40>) b * Phase * Phase) + (VSC_PHASES >> 1)) >> (VSC_PHASE_SHIFT);
            cx  =  ((ap_int<28>) c * Phase);

            sum = (I64) ((ax3 + bx2 + cx)  + (I32) d);
            norm = (I32) (sum + (VSC_PHASES >> 1)) >> VSC_PHASE_SHIFT;
            norm = CLAMP(norm, 0, (I32) ((1<<VSC_BITS_PER_COMPONENT)-1));
            OutPix->val[VSC_NR_COMPONENTS*s+i]   = (ap_uint<VSC_BITS_PER_COMPONENT>) norm;
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
int AXIvideo2MultiPixStream(VSC_AXI_STREAM_IN& AXI_video_strm,
                            VSC_STREAM_MPIX& img,
                            U16 &Height, U16 &WidthIn,
                            U8 &ColorMode
                            )
{
    int res = 0;
    //int res = 0;
    ap_axiu<(VSC_BITS_PER_CLOCK),1,1,1> axi;
    YUV_MULTI_PIXEL pix;
    int rows         = reg(Height);
    int cols         = reg(WidthIn);
    const int depth  = VSC_BITS_PER_COMPONENT;
    bool sof = 0;

    assert(rows <= VSC_MAX_HEIGHT);
    assert(cols <= VSC_MAX_WIDTH);
    assert(cols % VSC_SAMPLES_PER_CLOCK == 0);

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
#pragma HLS loop_tripcount max=2160
        bool eol = 0;
loop_width:
        for (int j = 0; j < cols/VSC_SAMPLES_PER_CLOCK; j++)
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
            if (eol && (j != cols/VSC_SAMPLES_PER_CLOCK-1))
            {
                res |= ERROR_IO_EOL_EARLY;
            }
            for (int l = 0; l < VSC_SAMPLES_PER_CLOCK; l++)
            {
                for (HLS_CHANNEL_T k = 0; k < VSC_NR_COMPONENTS; k++)
                {
                    ap_uint<VSC_BITS_PER_COMPONENT> pix_rgb, pix_444, pix_422;
                    const int map[3] = {2, 0, 1};
#pragma HLS ARRAY_PARTITION variable=&map complete dim=0
                    switch(ColorMode)
                    {
                    case 0x0:
                    	hls::AXIGetBitFields(axi, (map[k] + l * 3) * depth, depth, pix_rgb);
                    	pix.val[k + l * VSC_NR_COMPONENTS] = pix_rgb;
                        break;
                    case 0x1:
                    	hls::AXIGetBitFields(axi, (k + l * 3) * depth, depth, pix_444);
                        pix.val[k + l * VSC_NR_COMPONENTS] = pix_444;
                        break;
                    default:
                    	hls::AXIGetBitFields(axi, (k + l * 2) * depth, depth, pix_422);
                    	pix.val[k + l * VSC_NR_COMPONENTS] = pix_422;
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
int MultiPixStream2AXIvideo(VSC_STREAM_MPIX& StrmPix,
                            VSC_AXI_STREAM_OUT& AXI_video_strm,
                            U16 &Height,
                            U16 &Width,
                            U8 &ColorMode
                            )
{
    int res = 0;
    YUV_MULTI_PIXEL pix;
    ap_uint<VSC_BITS_PER_COMPONENT> pix_rgb, pix_444, pix_422;
    ap_axiu<(VSC_BITS_PER_CLOCK), 1, 1, 1> axi;
    int depth = VSC_BITS_PER_COMPONENT;
    int rows = reg(Height);
    int cols = reg(Width);
    assert(rows <= VSC_MAX_HEIGHT);
    assert(cols <= VSC_MAX_WIDTH);

#if (SAMPLES_PER_CLOCK == 1)
    const ap_uint<5> mapComp[4][3] =  {
                                        {1,  2,  0},     //RGB
                                        {0,  1,  2},     //4:4:4
                                        {0,  1,  2},     //4:2:2
                                        {0,  1,  2}     //4:2:0
                                        };
#endif
#if (SAMPLES_PER_CLOCK == 2)
    const ap_uint<5> mapComp[4][6] =  {
                                        {1,  2,  0,  4,  5,  3},     //RGB
                                        {0,  1,  2,  3,  4,  5},     //4:4:4
                                        {0,  1,  3,  4,  5,  2},     //4:2:2
                                        {0,  1,  3,  4,  5,  2}     //4:2:0
                                        };
#endif
#if (SAMPLES_PER_CLOCK == 4)
    const ap_uint<5> mapComp[4][12] =  {
                                        {1,  2,  0,  4,  5,  3,  7,  8,  6, 10, 11,  9},     //RGB
                                        {0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11},     //4:4:4
                                        {0,  1,  3,  4,  6,  7,  9, 10, 11,  8,  5,  2},     //4:2:2
                                        {0,  1,  3,  4,  6,  7,  9, 10, 11,  8,  5,  2}     //4:2:0
                                        };
#endif
#if (SAMPLES_PER_CLOCK == 8)
    const ap_uint<5> mapComp[4][24] =  {
                                        {1,  2,  0,  4,  5,  3,  7,  8,  6, 10, 11,  9, 13, 14, 12, 16, 17, 15, 19, 20, 18, 22, 23, 21},     //RGB
                                        {0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23},     //4:4:4
                                        {0,  1,  3,  4,  6,  7,  9, 10, 12, 13, 15, 16, 18, 19, 21, 22, 23, 20, 17, 14, 11,  8,  5,  2},     //4:2:2
                                        {0,  1,  3,  4,  6,  7,  9, 10, 12, 13, 15, 16, 18, 19, 21, 22, 23, 20, 17, 14, 11,  8,  5,  2}     //4:2:0
                                        };
#endif

    bool sof = 1;
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols/SAMPLES_PER_CLOCK; j++)
        {
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
            if (j == (cols/SAMPLES_PER_CLOCK-1))
            {
                axi.last = 1;
            }
            else
            {
                axi.last = 0;
            }
            StrmPix >> pix;
            axi.data = -1;

            for (int l = 0; l < SAMPLES_PER_CLOCK; l++)
            {
                for (int k = 0; k < VSC_NR_COMPONENTS; k++)
                {
                    int start = (k + l * VSC_NR_COMPONENTS) * depth;
                    //axi.data(start + depth - 1, start) = pix.val[kMap];
                    pix_rgb = pix.val[mapComp[0][k + l * VSC_NR_COMPONENTS]];
                    pix_444 = pix.val[mapComp[1][k + l * VSC_NR_COMPONENTS]];
                    pix_422 = pix.val[mapComp[2][k + l * VSC_NR_COMPONENTS]];
                    switch(ColorMode)
                    {
                    case 0x0:
                      axi.data(start + depth - 1, start) = pix_rgb;
                      break;
                    case 0x1:
                      axi.data(start + depth - 1, start) = pix_444;
                      break;
                    default:
                      axi.data(start + depth - 1, start) = pix_422;
                    }
                }
            }
            axi.keep = -1;

            AXI_video_strm << axi;
        }
    }
    return res;
}

//#define DBG_PRINT

#define KERNEL_V_SIZE   3
#define CHROMA_LINES    3
#define LUMA_LINES      2

//typedef hls::Scalar<1, PIXEL_TYPE>                  Y_PIXEL;
typedef hls::Scalar<SAMPLES_PER_CLOCK, PIXEL_TYPE>  Y_MULTI_PIXEL;
//typedef hls::Scalar<1, PIXEL_TYPE>                  C_PIXEL;
typedef hls::Scalar<SAMPLES_PER_CLOCK, PIXEL_TYPE>  C_MULTI_PIXEL;

typedef ap_uint<VSC_SAMPLES_PER_CLOCK*VSC_BITS_PER_COMPONENT>   Y_MEM_PIXEL_TYPE;
typedef ap_uint<VSC_SAMPLES_PER_CLOCK*VSC_BITS_PER_COMPONENT>   C_MEM_PIXEL_TYPE;

typedef hls::LineBuffer<LUMA_LINES,   (MAX_COLS/SAMPLES_PER_CLOCK), Y_MEM_PIXEL_TYPE>    LINE_BUFFER_Y;
typedef hls::LineBuffer<CHROMA_LINES, (MAX_COLS/SAMPLES_PER_CLOCK), C_MEM_PIXEL_TYPE>    LINE_BUFFER_C;
//typedef hls::LineBuffer<1, (MAX_COLS/SAMPLES_PER_CLOCK), YUV_MULTI_PIXEL>             LINE_BUFFER_YCC;
//typedef hls::LineBuffer<1, (MAX_COLS/SAMPLES_PER_CLOCK), C_MULTI_PIXEL>               LINE_BUFFER_Cr;

void v_vcresampler_core(VSC_STREAM_MPIX& srcImg,
                        U16 &height,
                        U16 &width,
                        U8 &inColorMode,
                        U8 &outColorMode,
                        VSC_STREAM_MPIX& outImg)
{
    I16 y,x,k;
    I16 yOffset; //offset between streaming row and processing row
    I16 xOffset = 0;
    I16 loopHeight;
    I16 loopWidth = (width/SAMPLES_PER_CLOCK) + xOffset;
    I16 out_y, out_x;
    I16 ChromaLine;


    YUV_MULTI_PIXEL pix;
    YUV_MULTI_PIXEL outpix;
    Y_MULTI_PIXEL mpix_y;
    C_MULTI_PIXEL mpix_c;
    //local storage for line buffer column (to avoid multiple read clients on BRAM)
    Y_MULTI_PIXEL pixbuf_y[LUMA_LINES+1];
    C_MULTI_PIXEL pixbuf_c[KERNEL_V_SIZE];
    Y_MEM_PIXEL_TYPE InYPix;
    C_MEM_PIXEL_TYPE InCPix;
    LINE_BUFFER_Y linebuf_y;
    LINE_BUFFER_C linebuf_c;

#if (USE_URAM == 1)
#pragma HLS RESOURCE variable=&linebuf_y core=XPM_MEMORY uram
#pragma HLS RESOURCE variable=&linebuf_c core=XPM_MEMORY uram
#endif

    if (inColorMode==outColorMode)
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
                for (I16 s=0; s<SAMPLES_PER_CLOCK; ++s)
                {
                    mpix_y.val[s] = pix.val[s*NUM_VIDEO_COMPONENTS];
                    mpix_c.val[s] = pix.val[s*NUM_VIDEO_COMPONENTS+1];
                }
            }

            //luma line buffer
#pragma HLS ARRAY_PARTITION variable=&pixbuf_y      complete dim=0
            // get column of pixels from the line buffer to local pixel array
            for (I16 i=0; i<LUMA_LINES; i++)
            {
                Y_MEM_PIXEL_TYPE LineBufVal = linebuf_y.getval(i,x);
                for (int l = 0; l < SAMPLES_PER_CLOCK; l++)
                {
                    int start = (l * VSC_BITS_PER_COMPONENT);
                    pixbuf_y[LUMA_LINES-1-i].val[l] = LineBufVal(start + VSC_BITS_PER_COMPONENT -1, start);
		    if(i==0)
                    InYPix(start + VSC_BITS_PER_COMPONENT -1, start) = mpix_y.val[l];
                }
            }

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
                for (int l = 0; l < SAMPLES_PER_CLOCK; l++)
                {
                        int start = (l* VSC_BITS_PER_COMPONENT);
                        LineBufVal(start + VSC_BITS_PER_COMPONENT -1, start) = PixBufVal.val[l];
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
                for (int l = 0; l < SAMPLES_PER_CLOCK; l++)
                {
                    int start = (l * VSC_BITS_PER_COMPONENT);
                    CBufVal.val[l] = LineBufVal(start + VSC_BITS_PER_COMPONENT -1, start);
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
                    for (int l = 0; l < SAMPLES_PER_CLOCK; l++)
                    {
                    int start = (l * VSC_BITS_PER_COMPONENT);
                    InCPix(start + VSC_BITS_PER_COMPONENT -1, start) = mpix_c.val[l];
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
                for (int l = 0; l < SAMPLES_PER_CLOCK; l++)
                {
                int start = (l* VSC_BITS_PER_COMPONENT);
                LineBufVal(start + VSC_BITS_PER_COMPONENT -1, start) = PixBufVal.val[l];
                }
                linebuf_c.val[i][x] = LineBufVal;
                }
            }
            else
            {
                C_MEM_PIXEL_TYPE LineBufCVal = linebuf_c.getval(CHROMA_LINES-1,x);
                for (int l = 0; l < SAMPLES_PER_CLOCK; l++)
                {
                    int start = (l * VSC_BITS_PER_COMPONENT);
                    pixbuf_c[0].val[l] = LineBufCVal(start + VSC_BITS_PER_COMPONENT -1, start);
                }
           }

            for(k=0; k<SAMPLES_PER_CLOCK; ++k)
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
                if(inColorMode==outColorMode)
                	outImg << pix;
                else
                	outImg << outpix;
            }
        }
    }
}


