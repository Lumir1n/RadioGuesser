// Copyright RadioGuesser. All Rights Reserved.

#include "Radio/RGRadioSubsystem.h"
#include "RadioGuesser.h"

void URGRadioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogRadio, Log, TEXT("RGRadioSubsystem initialised"));
}

void URGRadioSubsystem::Deinitialize()
{
    Stop();
    UE_LOG(LogRadio, Log, TEXT("RGRadioSubsystem deinitialised"));
    Super::Deinitialize();
}

void URGRadioSubsystem::OpenStream(const FRGRadioStreamInfo& StreamInfo)
{
    UE_LOG(LogRadio, Log, TEXT("OpenStream: DisplayName=%s"), *StreamInfo.DisplayName);
    ActiveStream = StreamInfo;
    SetPlaybackState(ERGRadioPlaybackState::Connecting);

    // TODO: Use Unreal Media Framework / Electra to open StreamInfo.StreamUrl
    // MediaPlayer->OpenUrl(StreamInfo.StreamUrl);
}

void URGRadioSubsystem::Play()
{
    UE_LOG(LogRadio, Log, TEXT("Radio Play requested"));
    // TODO: MediaPlayer->Play();
    SetPlaybackState(ERGRadioPlaybackState::Playing);
}

void URGRadioSubsystem::Pause()
{
    UE_LOG(LogRadio, Log, TEXT("Radio Pause requested"));
    // TODO: MediaPlayer->Pause();
    SetPlaybackState(ERGRadioPlaybackState::Paused);
}

void URGRadioSubsystem::Stop()
{
    UE_LOG(LogRadio, Log, TEXT("Radio Stop requested"));
    // TODO: MediaPlayer->Close();
    SetPlaybackState(ERGRadioPlaybackState::Stopped);
}

void URGRadioSubsystem::SetVolume(float Volume)
{
    CurrentVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
    UE_LOG(LogRadio, Verbose, TEXT("Radio volume set to %.2f"), CurrentVolume);
    // TODO: Apply volume to MediaSoundComponent
}

void URGRadioSubsystem::SetMuted(bool bMute)
{
    bMuted = bMute;
    UE_LOG(LogRadio, Verbose, TEXT("Radio muted: %s"), bMuted ? TEXT("true") : TEXT("false"));
    // TODO: Apply mute to MediaSoundComponent
}

void URGRadioSubsystem::SetPlaybackState(ERGRadioPlaybackState NewState)
{
    if (PlaybackState != NewState)
    {
        PlaybackState = NewState;
        OnPlaybackStateChanged.Broadcast(NewState);
    }
}
