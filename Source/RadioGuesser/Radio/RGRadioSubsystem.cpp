// Copyright RadioGuesser. All Rights Reserved.

#include "Radio/RGRadioSubsystem.h"
#include "RadioGuesser.h"
#include "MediaPlayer.h"
#include "StreamMediaSource.h"

void URGRadioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Create UMediaPlayer at runtime — no Content Browser asset needed
    MediaPlayer = NewObject<UMediaPlayer>(this, TEXT("RGMediaPlayer"));
    MediaPlayer->SetLooping(false);
    // Note: PlayOnOpen is handled by OpenSource triggering OnMediaOpened,
    // which sets state to Playing. We do not access PlayOnOpen directly
    // to avoid version-specific field access issues.

    StreamMediaSource = NewObject<UStreamMediaSource>(this, TEXT("RGStreamMediaSource"));

    BindMediaPlayerDelegates();

    UE_LOG(LogRadio, Log, TEXT("RGRadioSubsystem initialised"));
}

void URGRadioSubsystem::Deinitialize()
{
    Stop();
    if (MediaPlayer)
    {
        MediaPlayer->OnMediaOpened.RemoveAll(this);
        MediaPlayer->OnMediaOpenFailed.RemoveAll(this);
        MediaPlayer->OnEndReached.RemoveAll(this);
    }
    UE_LOG(LogRadio, Log, TEXT("RGRadioSubsystem deinitialised"));
    Super::Deinitialize();
}

void URGRadioSubsystem::BindMediaPlayerDelegates()
{
    if (!MediaPlayer) return;
    MediaPlayer->OnMediaOpened.AddDynamic(this,     &URGRadioSubsystem::HandleMediaOpened);
    MediaPlayer->OnMediaOpenFailed.AddDynamic(this,  &URGRadioSubsystem::HandleMediaOpenFailed);
    MediaPlayer->OnEndReached.AddDynamic(this,       &URGRadioSubsystem::HandleMediaEndReached);
}

// ─── Playback ─────────────────────────────────────────────────────────────────

void URGRadioSubsystem::OpenStream(const FRGRadioStreamInfo& StreamInfo)
{
    if (StreamInfo.StreamUrl.IsEmpty())
    {
        UE_LOG(LogRadio, Error, TEXT("OpenStream: empty URL"));
        SetPlaybackState(ERGRadioPlaybackState::Error);
        return;
    }

    ActiveStream = StreamInfo;
    RetryCount   = 0;

    UE_LOG(LogRadio, Log, TEXT("OpenStream: %s"), *StreamInfo.DisplayName);
    SetPlaybackState(ERGRadioPlaybackState::Connecting);

    StreamMediaSource->StreamUrl = StreamInfo.StreamUrl;
    MediaPlayer->OpenSource(StreamMediaSource);
    // MediaPlayer will fire OnMediaOpened → HandleMediaOpened → state = Playing
}

void URGRadioSubsystem::Play()
{
    // Only needed to resume from Pause. On first open, OnMediaOpened handles it.
    if (MediaPlayer && MediaPlayer->IsPaused())
    {
        MediaPlayer->Play();
        SetPlaybackState(ERGRadioPlaybackState::Playing);
    }
}

void URGRadioSubsystem::Pause()
{
    if (MediaPlayer)
    {
        MediaPlayer->Pause();
        SetPlaybackState(ERGRadioPlaybackState::Paused);
    }
}

void URGRadioSubsystem::Stop()
{
    if (MediaPlayer)
    {
        MediaPlayer->Close();
    }
    SetPlaybackState(ERGRadioPlaybackState::Stopped);
}

void URGRadioSubsystem::SetVolume(float InVolume)
{
    CurrentVolume = FMath::Clamp(InVolume, 0.0f, 1.0f);
    // Volume is applied to the UMediaSoundComponent in the level Blueprint
}

void URGRadioSubsystem::SetMuted(bool bInMuted)
{
    bMuted = bInMuted;
}

// ─── MediaPlayer callbacks ────────────────────────────────────────────────────

void URGRadioSubsystem::HandleMediaOpened(FString OpenedUrl)
{
    UE_LOG(LogRadio, Log, TEXT("Stream opened: %s"), *OpenedUrl);
    RetryCount = 0;
    // Start playback — call Play() since we removed PlayOnOpen = true
    if (MediaPlayer)
    {
        MediaPlayer->Play();
    }
    SetPlaybackState(ERGRadioPlaybackState::Playing);
}

void URGRadioSubsystem::HandleMediaOpenFailed(FString FailedUrl)
{
    UE_LOG(LogRadio, Warning, TEXT("Stream failed: %s (retry %d/%d)"),
        *FailedUrl, RetryCount + 1, MaxRetries);

    if (RetryCount < MaxRetries)
    {
        RetryCount++;
        SetPlaybackState(ERGRadioPlaybackState::Connecting);
        MediaPlayer->OpenSource(StreamMediaSource);
    }
    else
    {
        UE_LOG(LogRadio, Error, TEXT("Stream gave up after %d retries"), MaxRetries);
        SetPlaybackState(ERGRadioPlaybackState::Error);
        OnRadioError.Broadcast(ERGRadioPlaybackState::Error,
            FString::Printf(TEXT("Stream unavailable after %d retries"), MaxRetries));
    }
}

void URGRadioSubsystem::HandleMediaEndReached()
{
    // Internet radio is continuous — reaching end = disconnect
    UE_LOG(LogRadio, Warning, TEXT("Stream disconnected — reconnecting"));
    if (RetryCount < MaxRetries && !ActiveStream.StreamUrl.IsEmpty())
    {
        RetryCount++;
        SetPlaybackState(ERGRadioPlaybackState::Connecting);
        MediaPlayer->OpenSource(StreamMediaSource);
    }
    else
    {
        SetPlaybackState(ERGRadioPlaybackState::Error);
        OnRadioError.Broadcast(ERGRadioPlaybackState::Error, TEXT("Stream disconnected"));
    }
}

void URGRadioSubsystem::SetPlaybackState(ERGRadioPlaybackState NewState)
{
    if (PlaybackState != NewState)
    {
        PlaybackState = NewState;
        OnPlaybackStateChanged.Broadcast(NewState);
    }
}
