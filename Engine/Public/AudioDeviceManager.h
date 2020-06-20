// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioDefines.h"
#include "AudioThread.h"
#include "CoreMinimal.h"


#if ENABLE_AUDIO_DEBUG
class FAudioDebugger;
#endif // ENABLE_AUDIO_DEBUG

// Set this to one if you'd like to check who owns
// handles to an audio device.
#ifndef INSTRUMENT_AUDIODEVICE_HANDLES
#define INSTRUMENT_AUDIODEVICE_HANDLES 0
#endif

class FReferenceCollector;
class FSoundBuffer;
class IAudioDeviceModule;
class UAudioComponent;
class USoundClass;
class USoundMix;
class USoundSubmixBase;
class USoundWave;
class UWorld;
class FAudioDevice;
class FAudioDebugger;
struct FSourceEffectChainEntry;

namespace Audio
{
	/**
	 * Typed identifier for Audio Device Id
	 */
	using FDeviceId = uint32;
}

enum class ESoundType : uint8
{
	Class,
	Cue,
	Wave
};

// This enum is used in FAudioDeviceManager::RequestAudioDevice to map a given UWorld to an audio device.
enum class EAudioDeviceScope : uint8
{
	// Default to the behavior specified by the editor preferences.
	Default,
	// Use an audio device that can be shared by multiple worlds.
	Shared,
	// Create a new audio device specifically for this handle.
	Unique
};

// Parameters passed into FAudioDeviceManager::RequestAudioDevice.
struct FAudioDeviceParams
{
	// Optional world parameter. This allows tools to surface information about which worlds are being rendered through which audio devices.
	UWorld* AssociatedWorld;
	// This should be set to EAudioDeviceScope::Unique if you'd like to force a new device to be created from scratch, or use EAudioDeviceScope::Shared to use an existing device if possible.
	EAudioDeviceScope Scope;
	// Set this to true to get a handle to a non realtime audio renderer.
	bool bIsNonRealtime;
	// Use this to force this audio device to use a specific audio module. If nullptr, uses the default audio module.
	IAudioDeviceModule* AudioModule;

	FAudioDeviceParams()
		: AssociatedWorld(nullptr)
		, Scope(EAudioDeviceScope::Default)
		, bIsNonRealtime(false)
		, AudioModule(nullptr)
	{}
};

// Strong handle to an audio device. Guarantees that the audio device it references will stay alive while it is in scope.
class ENGINE_API FAudioDeviceHandle
{
public:
	FAudioDeviceHandle();
	FAudioDeviceHandle(const FAudioDeviceHandle& Other);
	FAudioDeviceHandle(FAudioDeviceHandle&& Other);

	FAudioDeviceHandle& operator=(const FAudioDeviceHandle& Other);
	FAudioDeviceHandle& operator=(FAudioDeviceHandle&& Other);

	
	~FAudioDeviceHandle();

	// gets a pointer to the compressed chunk.
	FAudioDevice* GetAudioDevice() const;

	// Returns the device ID for the audio device referenced by this handle.
	Audio::FDeviceId GetDeviceID() const;

	// Checks whether this points to a valid compressed chunk.
	bool IsValid() const;

	void Reset();

private:
	// This constructor should only be called by FAudioDeviceManager.
	FAudioDeviceHandle(FAudioDevice* InDevice, Audio::FDeviceId InID, UWorld* InWorld);

	// The world that this FAudioDeviceHandle was requested with.
	// Null if this device handle wasn't generated by RequestAudioDevice.
	UWorld* World;

	FAudioDevice* Device;
	Audio::FDeviceId DeviceId;

#if INSTRUMENT_AUDIODEVICE_HANDLES
	uint32 StackWalkID;
	void AddStackDumpToAudioDeviceContainer();
#endif

	friend class FAudioDeviceManager;

public:
	// These are convenience operators to use an FAudioDeviceHandle like an FAudioDevice* or an Audio::FDeviceId.
	// For safety, we still require explicit casting to an FAudioDevice* to ensure ownership isn't lost due to programmer error.
	const FAudioDevice& operator*() const
	{
		check(IsValid());
		return *Device;
	}

	FAudioDevice& operator*()
	{
		check(IsValid());
		return *Device;
	}

	const FAudioDevice* operator->() const
	{
		check(IsValid());
		return Device;
	}

	FAudioDevice* operator->()
	{
		check(IsValid());
		return Device;
	}

	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	FORCEINLINE bool operator==(const FAudioDeviceHandle& Other) const
	{
		return DeviceId == Other.DeviceId;
	}

	FORCEINLINE bool operator==(const FAudioDevice*& Other) const
	{
		return Device == Other;
	}

	FORCEINLINE bool operator==(FAudioDevice*& Other) const
	{
		return Device == Other;
	}

	FORCEINLINE bool operator==(const Audio::FDeviceId& Other) const
	{
		return DeviceId == Other;
	}

	explicit operator FAudioDevice*() const
	{
		check(IsValid());
		return Device;
	}

	explicit operator Audio::FDeviceId() const
	{
		return DeviceId;
	}
};

// List of delegates for the audio device manager.
class ENGINE_API FAudioDeviceManagerDelegates
{
public:
	// This delegate is called whenever an entirely new audio device is created.
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAudioDeviceCreated, Audio::FDeviceId /* AudioDeviceId*/);
	static FOnAudioDeviceCreated OnAudioDeviceCreated;

	// This delegate is called whenever an audio device is destroyed.
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAudioDeviceDestroyed, Audio::FDeviceId /* AudioDeviceId*/);
	static FOnAudioDeviceDestroyed OnAudioDeviceDestroyed;

	// This delegate is called whenever a world is registered to an audio device. Please note that UWorlds are not guaranteed to
	// be registered to the same audio device throughout their lifecycle,
	// and there is no guarantee on the lifespan of both the UWorld and the Audio Device registered in this callback.
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWorldRegisteredToAudioDevice, const UWorld* /*InWorld */, Audio::FDeviceId /* AudioDeviceId*/);
	static FOnWorldRegisteredToAudioDevice OnWorldRegisteredToAudioDevice;
};

/**
* Class for managing multiple audio devices.
*/
class ENGINE_API FAudioDeviceManager
{
public:

	/**
	* Constructor
	*/
	FAudioDeviceManager();

	/**
	* Destructor
	*/
	~FAudioDeviceManager();

	/**
	* Initialize the audio device manager.
	* Return true if successfully initialized.
	**/
	bool Initialize();

	/** Returns the handle to the main audio device. */
	FAudioDeviceHandle GetMainAudioDeviceHandle() const { return MainAudioDeviceHandle; }
	Audio::FDeviceId GetMainAudioDeviceID() const { return MainAudioDeviceHandle.GetDeviceID(); }

	/** Returns true if we're currently using the audio mixer. */
	bool IsUsingAudioMixer() const;

	/**
	 * returns the currently used audio device module for this platform.
	 * returns nullptr if Initialize() has not been called yet.
	 */
	IAudioDeviceModule* GetAudioDeviceModule();

	FAudioDeviceParams GetDefaultParamsForNewWorld();

	/**
	* Creates or requests an audio device instance internally and returns a
	* handle to the audio device.
	* This audio device is guaranteed to be alive as long as the returned handle is in scope.
	*/
	FAudioDeviceHandle RequestAudioDevice(const FAudioDeviceParams& InParams);

	/**
	* Returns whether the audio device handle is valid (i.e. points to
	* an actual audio device instance)
	*/
	bool IsValidAudioDevice(Audio::FDeviceId DeviceID) const;

	/**
	 * Returns a strong handle to the audio device associated with the given device ID.
	 * if the device ID is invalid we return an invalid, zeroed handle.
	 */
	FAudioDeviceHandle GetAudioDevice(Audio::FDeviceId DeviceID);

	/**
	* Returns a raw ptr to the audio device associated with the handle. If the
	* handle is invalid then a NULL device ptr will be returned.
	*/
	FAudioDevice* GetAudioDeviceRaw(Audio::FDeviceId DeviceID);

	static FAudioDeviceManager* Get();

	/**
	* Returns a ptr to the active audio device. If there is no active
	* device then it will return the main audio device.
	*/
	FAudioDeviceHandle GetActiveAudioDevice();

	/** Returns the current number of active audio devices. */
	uint8 GetNumActiveAudioDevices() const;

	/** Returns the number of worlds (e.g. PIE viewports) using the main audio device. */
	uint8 GetNumMainAudioDeviceWorlds() const;

	/** Updates all active audio devices */
	void UpdateActiveAudioDevices(bool bGameTicking);

	void IterateOverAllDevices(TFunction<void(Audio::FDeviceId, FAudioDevice*)> ForEachDevice);
	void IterateOverAllDevices(TFunction<void(Audio::FDeviceId, const FAudioDevice*)> ForEachDevice) const;

	/** Tracks objects in the active audio devices. */
	void AddReferencedObjects(FReferenceCollector& Collector);

	/** Stops sounds using the given resource on all audio devices. */
	void StopSoundsUsingResource(class USoundWave* InSoundWave, TArray<UAudioComponent*>* StoppedComponents = nullptr);

	/** Registers the Sound Class for all active devices. */
	void RegisterSoundClass(USoundClass* SoundClass);

	/** Unregisters the Sound Class for all active devices. */
	void UnregisterSoundClass(USoundClass* SoundClass);

	/** Initializes the sound class for all active devices. */
	void InitSoundClasses();

	/** Registers the Sound Mix for all active devices. */
	void RegisterSoundSubmix(const USoundSubmixBase* SoundSubmix);

	/** Registers the Sound Mix for all active devices. */
	void UnregisterSoundSubmix(const USoundSubmixBase* SoundSubmix);

	/** Initializes the sound mixes for all active devices. */
	void InitSoundSubmixes();

	/** Initialize all sound effect presets. */
	void InitSoundEffectPresets();

	/** Updates source effect chain on all sources currently using the source effect chain. */
	void UpdateSourceEffectChain(const uint32 SourceEffectChainId, const TArray<FSourceEffectChainEntry>& SourceEffectChain, const bool bPlayEffectChainTails);

	/** Updates this submix for any changes made. Broadcasts to all submix instances. */
	void UpdateSubmix(USoundSubmixBase* SoundSubmix);

	/** Sets which audio device is the active audio device. */
	void SetActiveDevice(uint32 InAudioDeviceHandle);

	/** Sets an audio device to be solo'd */
	void SetSoloDevice(Audio::FDeviceId InAudioDeviceHandle);

	/** Links up the resource data indices for looking up and cleaning up. */
	void TrackResource(USoundWave* SoundWave, FSoundBuffer* Buffer);

	/** Frees the given sound wave resource from the device manager */
	void FreeResource(USoundWave* SoundWave);

	/** Frees the sound buffer from the device manager. */
	void FreeBufferResource(FSoundBuffer* SoundBuffer);

	/** Stops using the given sound buffer. Called before freeing the buffer */
	void StopSourcesUsingBuffer(FSoundBuffer* Buffer);

	/** Retrieves the sound buffer for the given resource id */
	FSoundBuffer* GetSoundBufferForResourceID(uint32 ResourceID);

	/** Removes the sound buffer for the given resource id */
	void RemoveSoundBufferForResourceID(uint32 ResourceID);

	/** Removes sound mix from all audio devices */
	void RemoveSoundMix(USoundMix* SoundMix);

	/** Toggles playing audio for all active PIE sessions (and all devices). */
	void TogglePlayAllDeviceAudio();

	/** Gets whether or not all devices should play their audio. */
	bool IsPlayAllDeviceAudio() const { return bPlayAllDeviceAudio; }

	/** Is debug visualization of 3d sounds enabled */
	bool IsVisualizeDebug3dEnabled() const;

	/** Toggles 3d visualization of 3d sounds on/off */
	void ToggleVisualize3dDebug();

	/** Reset all sound cue trims */
	void ResetAllDynamicSoundVolumes();

	/** Get, reset, or set a sound cue trim */
	float GetDynamicSoundVolume(ESoundType SoundType,  const FName& SoundName) const;
	void ResetDynamicSoundVolume(ESoundType SoundType, const FName& SoundName);
	void SetDynamicSoundVolume(ESoundType SoundType, const FName& SoundName, float Volume);

#if ENABLE_AUDIO_DEBUG
	/** Get the audio debugger instance */
	FAudioDebugger& GetDebugger();
	const FAudioDebugger& GetDebugger() const;
#endif // ENABLE_AUDIO_DEBUG

public:

	/** Array of all created buffers */
	TArray<FSoundBuffer*>			Buffers;

	/** Look up associating a USoundWave's resource ID with sound buffers	*/
	TMap<int32, FSoundBuffer*>	WaveBufferMap;

	/** Returns all the audio devices managed by device manager. */
	TArray<FAudioDevice*> GetAudioDevices();

	TArray<UWorld*> GetWorldsUsingAudioDevice(const Audio::FDeviceId& InID);

#if INSTRUMENT_AUDIODEVICE_HANDLES
	void AddStackWalkForContainer(Audio::FDeviceId InId, uint32 StackWalkID, FString&& InStackWalk);
	void RemoveStackWalkForContainer(Audio::FDeviceId InId, uint32 StackWalkID);
#endif

	void LogListOfAudioDevices();

private:

#if ENABLE_AUDIO_DEBUG
	/** Instance of audio debugger shared across audio devices */
	TUniquePtr<FAudioDebugger> AudioDebugger;
#endif // ENABLE_AUDIO_DEBUG


	/** Creates a handle given the index and a generation value. */
	uint32 GetNewDeviceID();

	/** Toggles between audio mixer and non-audio mixer audio engine. */
	void ToggleAudioMixer();

	/** Load audio device module. */
	bool LoadDefaultAudioDeviceModule();

	FAudioDeviceHandle CreateNewDevice(const FAudioDeviceParams& InParams);

	/**
	* Shutsdown the audio device associated with the handle. The handle
	* will become invalid after the audio device is shut down.
	*/
	bool ShutdownAudioDevice(Audio::FDeviceId DeviceID);

	// Called exclusively by the FAudioDeviceHandle copy constructor and assignment operators:
	void IncrementDevice(Audio::FDeviceId DeviceID);

	// Called exclusively by the FAudioDeviceHandle dtor.
	void DecrementDevice(Audio::FDeviceId DeviceID, UWorld* InWorld);

	/**
	* Shuts down all active audio devices
	*/
	bool ShutdownAllAudioDevices();

	/** Application enters background handler */
	void AppWillEnterBackground();

	/** Audio device module which creates (old backend) audio devices. */
	IAudioDeviceModule* AudioDeviceModule;

	/** Audio device module name. This is the "old" audio engine module name to use. E.g. XAudio2 */
	FString AudioDeviceModuleName;

	/** The audio mixer module name. This is the audio mixer module name to use. E.g. AudioMixerXAudio2 */
	FString AudioMixerModuleName;

	/** Handle to the main audio device. */
	FAudioDeviceHandle MainAudioDeviceHandle;

	struct FAudioDeviceContainer
	{
		// Singularly owned device.
		// Could be a TUniquePtr if FAudioDevice was not an incomplete type here.
		FAudioDevice* Device;

		// Ref count of FAudioDeviceHandles referencing this device.
		int32 NumberOfHandlesToThisDevice;

		/** Worlds that have been registered to this device. */
		TArray<UWorld*> WorldsUsingThisDevice;

		/** Whether this device can be shared. */
		EAudioDeviceScope Scope;

		/** Whether this audio device is realtime or not. */
		bool bIsNonRealtime;

		/** Module this was created with. If nullptr, this device was created with the default module. */
		IAudioDeviceModule* SpecifiedModule;

		FAudioDeviceContainer(const FAudioDeviceParams& InParams, Audio::FDeviceId InDeviceID, FAudioDeviceManager* DeviceManager);
		~FAudioDeviceContainer();

		FAudioDeviceContainer(const FAudioDeviceContainer& Other)
		{
			// We explicitly enforce that we invoke the move instructor.
			// If this was hit, you likely need to call either Devices.Emplace(args) or Devices.Add(DeviceID, MoveTemp(Container));
			checkNoEntry();
		}

		FAudioDeviceContainer(FAudioDeviceContainer&& Other);

#if INSTRUMENT_AUDIODEVICE_HANDLES
		TMap<uint32, FString> HandleCreationStackWalks;
#endif

	private:
		FAudioDeviceContainer();
	};

	FAudioDeviceHandle BuildNewHandle(FAudioDeviceContainer&Container, Audio::FDeviceId DeviceID, const FAudioDeviceParams &InParams);

	/**
	 * This function is used to check if we can use an existing audio device.
	 */
	static bool CanUseAudioDevice(const FAudioDeviceParams& InParams, const FAudioDeviceContainer& InContainer);

#if INSTRUMENT_AUDIODEVICE_HANDLES
	static uint32 CreateUniqueStackWalkID();
#endif

	/**
	* Bank of audio devices. Will increase in size as we create new audio devices,
	*/
	TMap<Audio::FDeviceId, FAudioDeviceContainer> Devices;
	FCriticalSection DeviceMapCriticalSection;

	/* Counter used by GetNewDeviceID() to generate a unique ID for a given audio device. */
	uint32 DeviceIDCounter;

	/** Next resource ID to assign out to a wave/buffer */
	int32 NextResourceID;

	/** Which audio device is solo'd */
	Audio::FDeviceId SoloDeviceHandle;

	/** Which audio device is currently active */
	Audio::FDeviceId ActiveAudioDeviceID;

	/** Dynamic volume map */
	TMap<TTuple<ESoundType, FName>, float> DynamicSoundVolumes;

	/** Whether we're currently using the audio mixer or not. Used to toggle between old/new audio engines. */
	bool bUsingAudioMixer;

	/** Whether or not to play all audio in all active audio devices. */
	bool bPlayAllDeviceAudio;

	/** Whether or not we check to toggle audio mixer once. */
	bool bOnlyToggleAudioMixerOnce;

	/** Whether or not we've toggled the audio mixer. */
	bool bToggledAudioMixer;

	/** Audio Fence to ensure that we don't allow the audio thread to drift never endingly behind. */
	FAudioCommandFence SyncFence;

	friend class FAudioDeviceHandle;
};
