/************************************************************************
COPYRIGHT (C) SGS-THOMSON Microelectronics 2006

Source file name : frame_parser_video_divx.h
Author :           Chris

Definition of the DivX frame parser video class implementation for player 2.


Date        Modification                                    Name
----        ------------                                    --------
18-Jun-07   Created                                         Chris

************************************************************************/

#ifndef H_FRAME_PARSER_VIDEO_DIVX
#define H_FRAME_PARSER_VIDEO_DIVX

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "frame_parser_video.h"
#include "mpeg4.h"

#define MAX_REFERENCE_FRAMES_FORWARD_PLAY MPEG4_VIDEO_MAX_REFERENCE_FRAMES_IN_DECODE

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

// /////////////////////////////////////////////////////////////////////////
//
// The class definition
//

class FrameParser_VideoDivx_c : public FrameParser_Video_c
{
protected:

	// Data

	unsigned int                 DivXVersion;

	Mpeg4VideoFrameParameters_t  *FrameParameters;
	Mpeg4VideoStreamParameters_t *StreamParameters;

	bool                          DroppedFrame;
	bool                          StreamParametersSet;
	bool                          SentStreamParameter;
	unsigned int                  TimeIncrementBits;
	unsigned int                  QuantPrecision;
	unsigned int                  Interlaced;       
	unsigned int                  CurrentMicroSecondsPerFrame;

	unsigned int                  LastPredictionType;     // for fake NVOP detection
	unsigned int                  LastTimeIncrement;      // for fake NVOP detection

	// Functions

	FrameParserStatus_t  ReadVolHeader( Mpeg4VolHeader_t *Vol );
	FrameParserStatus_t  ReadVopHeader( Mpeg4VopHeader_t *Vop );
	unsigned int ReadVosHeader( void );

	FrameParserStatus_t  CommitFrameForDecode(void);

public:

    //
    // Constructor/Destructor methods
    //

    FrameParser_VideoDivx_c ( void );
    ~FrameParser_VideoDivx_c( void );

    //
    // Overrides for component base class functions
    //

    FrameParserStatus_t   Reset( void );

    //
    // FrameParser class functions
    //

    FrameParserStatus_t   RegisterOutputBufferRing( Ring_t Ring );

    //
    // Extensions to the class overriding the base implementations
    // NOTE in order to keep the names reasonably short, in the following
    // functions specifically for forward playback will be prefixed with 
    // ForPlay and functions specific to reverse playback will be prefixed with 
    // RevPlay.
    //

    FrameParserStatus_t   ForPlayProcessFrame( void );
    FrameParserStatus_t   ForPlayQueueFrameForDecode( void );
/*
    FrameParserStatus_t   RevPlayProcessFrame( void );
    FrameParserStatus_t   RevPlayQueueFrameForDecode( void );
    FrameParserStatus_t   RevPlayPurgeDecodeStacks( void );
    FrameParserStatus_t   RevPlayProcessDecodeStacks( void );
    FrameParserStatus_t   RevPlayGeneratePostDecodeParameterSettings(           void );
    FrameParserStatus_t   RevPlayAppendToReferenceFrameList(                    void );
    FrameParserStatus_t   RevPlayRemoveReferenceFrameFromList(                  void );
    FrameParserStatus_t   RevPlayJunkReferenceFrameList(                        void );
*/      


    //
    // Extensions to the class to be fulfilled by my inheritors, 
    // these are required to support the process buffer override
    //

    FrameParserStatus_t   ReadHeaders( void );

    FrameParserStatus_t   PrepareReferenceFrameList( void );
    FrameParserStatus_t   ForPlayUpdateReferenceFrameList( void );

};

#endif // H_FRAME_PARSER_VIDEO_DIVX
