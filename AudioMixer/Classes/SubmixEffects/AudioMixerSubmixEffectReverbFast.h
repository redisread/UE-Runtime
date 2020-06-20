// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioEffect.h"
#include "Curves/RichCurve.h"
#include "DSP/Amp.h"
#include "DSP/ReverbFast.h"
#include "DSP/BufferVectorOperations.h"
#include "Sound/SoundEffectSubmix.h"

#include "AudioMixerSubmixEffectReverbFast.generated.h"

struct FAudioEffectParameters;

USTRUCT(BlueprintType)
struct AUDIOMIXER_API FSubmixEffectReverbFastSettings
{
	GENERATED_USTRUCT_BODY()

	/** Bypasses reverb */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General)
	bool bBypass;

	/** Density - 0.0 < 0.85 < 1.0 - Coloration of the late reverb - lower value is more grainy */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General, meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "!bBypass"))
	float Density;

	/** Diffusion - 0.0 < 0.85 < 1.0 - Echo density in the reverberation decay - lower is more grainy */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General, meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "!bBypass"))
	float Diffusion;

	/** Reverb Gain - 0.0 < 0.32 < 1.0 - overall reverb gain - master volume control */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General, meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "!bBypass"))
	float Gain;

	/** Reverb Gain High Frequency - 0.0 < 0.89 < 1.0 - attenuates the high frequency reflected sound */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General, meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "!bBypass"))
	float GainHF;

	/** Decay Time - 0.1 < 1.49 < 20.0 Seconds - larger is more reverb */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Decay, meta = (ClampMin = "0.1", ClampMax = "20.0", EditCondition = "!bBypass"))
	float DecayTime;

	/** Decay High Frequency Ratio - 0.1 < 0.83 < 2.0 - how much the quicker or slower the high frequencies decay relative to the lower frequencies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Decay, meta = (ClampMin = "0.1", ClampMax = "2.0", EditCondition = "!bBypass"))
	float DecayHFRatio;

	/** Reflections Gain - 0.0 < 0.05 < 3.16 - controls the amount of initial reflections */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EarlyReflections, meta = (ClampMin = "0.0", ClampMax = "3.16", EditCondition = "!bBypass"))
	float ReflectionsGain;

	/** Reflections Delay - 0.0 < 0.007 < 0.3 Seconds - the time between the listener receiving the direct path sound and the first reflection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = EarlyReflections, meta = (ClampMin = "0.0", ClampMax = "0.3", EditCondition = "!bBypass"))
	float ReflectionsDelay;

	/** Late Reverb Gain - 0.0 < 1.26 < 10.0 - gain of the late reverb */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LateReflections, meta = (ClampMin = "0.0", ClampMax = "10.0", EditCondition = "!bBypass"))
	float LateGain;

	/** Late Reverb Delay - 0.0 < 0.011 < 0.1 Seconds - time difference between late reverb and first reflections */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LateReflections, meta = (ClampMin = "0.0", ClampMax = "0.1", EditCondition = "!bBypass"))
	float LateDelay;

	/** Air Absorption - 0.0 < 0.994 < 1.0 - lower value means more absorption */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = General, meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "!bBypass"))
	float AirAbsorptionGainHF;

	// Overall wet level of the reverb effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Routing, meta = (EditCondition = "!bBypass", UIMin = "0.0", UIMax = "1.0", ClampMin = "0.0", ClampMax = "10.0", EditCondition = "!bBypass"))
	float WetLevel;

	// Overall dry level of the reverb effect
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Routing, meta = (EditCondition = "!bBypass", ClampMin = "0.0", ClampMax = "1.0", EditCondition = "!bBypass"))
	float DryLevel;

	FSubmixEffectReverbFastSettings()
		: bBypass(false)
		, Density(0.85f)
		, Diffusion(0.85f)
		, Gain(0.0f)
		, GainHF(0.89f)
		, DecayTime(1.49f)
		, DecayHFRatio(0.83f)
		, ReflectionsGain(0.05f)
		, ReflectionsDelay(0.007f)
		, LateGain(1.26f)
		, LateDelay(0.1f)
		, AirAbsorptionGainHF(0.994f)
		, WetLevel(0.3f)
		, DryLevel(0.0f)
	{
	}
};

class AUDIOMIXER_API FSubmixEffectReverbFast : public FSoundEffectSubmix
{
public:
	FSubmixEffectReverbFast();

	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSubmixInitData& InSampleRate) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// We want to receive downmixed submix audio to stereo input for the reverb effect
	virtual uint32 GetDesiredInputChannelCountOverride() const override { return 2; }

	// Process the input block of audio. Called on audio thread.
	virtual void OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData) override;

	// Sets the reverb effect parameters based from audio thread code
	virtual bool SetParameters(const FAudioEffectParameters& InParameters) override;

	// Whether this effect supports the default reverb system
	virtual bool SupportsDefaultReverb() const override
	{
		return true;
	}

	// Returns the drylevel of the effect
	virtual float GetDryLevel() const override { return CurrentWetDry.DryLevel; }

private:

	static const float MinWetness;
	static const float MaxWetness;

	void UpdateParameters();

	// The fast reverb effect
	TUniquePtr<Audio::FPlateReverbFast> PlateReverb;

	// The reverb effect params
	Audio::TParams<Audio::FPlateReverbFastSettings> ReverbParams;

	// Settings for wet and dry signal to be consumed on next buffer
	Audio::TParams<Audio::FWetDry> WetDryParams;

	// Level of wet/dry signal on current buffer
	Audio::FWetDry CurrentWetDry;

	Audio::AlignedFloatBuffer WetInputBuffer;

	// Curve which maps old reverb times to new decay value
	FRichCurve DecayCurve;
};

UCLASS()
class AUDIOMIXER_API USubmixEffectReverbFastPreset : public USoundEffectSubmixPreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SubmixEffectReverbFast)

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSubmixEffectReverbFastSettings& InSettings);

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettingsWithReverbEffect(const UReverbEffect* InReverbEffect, const float WetLevel, const float DryLevel = 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixEffectPreset)
	FSubmixEffectReverbFastSettings Settings;
};
