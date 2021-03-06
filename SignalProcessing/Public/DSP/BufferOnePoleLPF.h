// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Dsp.h"
#include "DSP/BufferVectorOperations.h"

namespace Audio
{

	// Simple 1-pole lowpass filter
	class SIGNALPROCESSING_API FBufferOnePoleLPF
	{
	public:

		// Constructor 
		FBufferOnePoleLPF(float InG = 0.0f);

		// Set the LPF gain coefficient
		void SetG(float InG);

		// Resets the sample delay to 0
		void Reset();

		// Sets the filter frequency using normalized frequency (between 0.0 and 1.0f or 0.0 hz and Nyquist Frequency in Hz) 
		void SetFrequency(const float InFrequency);

		// Process InSamples and place filtered audio in OutSamples
		void ProcessAudio(const AlignedFloatBuffer& InSamples, AlignedFloatBuffer& OutSamples);

	protected:
		float CutoffFrequency;

		// Filter coefficients
		float B1;
		float A0;

		// 1-sample delay
		float Z1;
	};

	

}
