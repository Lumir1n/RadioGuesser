// Copyright RadioGuesser. All Rights Reserved.

#include "Radio/RGRadioSubsystem.h"
#include "RadioGuesser.h"
#include "MediaPlayer.h"
#include "StreamMediaSource.h"

void URGRadioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Create MediaPlayer at runtime — no asset needed in Content Browser
    MediaPlayer = NewObject<UMediaPlayer>(this, TEXT("RGMediaPlayer"));
    MediaPlayer->SetLooping(false);
    MediaPlayer->PlayOnOpen = true;

    // Create a StreamMediaSource we can reuse by changing the URL each round
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
    MediaPlayer->OnMediaOpened.AddDynamic(this, &URGRadioSubsystem::HandleMediaOpened);
    MediaPlayer->OnMediaOpenFailed.AddDynamic(this, &URGRadioSubsystem::HandleMediaOpenFailed);
    MediaPlayer->OnEndReached.AddDynamic(this, &URGRadioSubsystem::HandleMediaEndReached);
}

// ─── Playback ─────────────────────────────────────────────────────────────────

void URGRadioSubsystem::OpenStream(const FRGRadioStreamInfo& StreamInfo)
{
    if (StreamInfo.StreamUrl.IsEmpty())
    {
        UE_LOG(LogRadio, Error, TEXT("OpenStream: empty stream URL"));
        SetPlaybackState(ERGRadioPlaybackState::Error);
        return;
    }

    ActiveStream = StreamInfo;
    RetryCount   = 0;

    UE_LOG(LogRadio, Log, TEXT("OpenStream: %s"), *StreamInfo.DisplayName);
    SetPlaybackState(ERGRadioPlaybackState::Connecting);

    // Set the URL on the media source and open it
    StreamMediaSource->StreamUrl = StreamInfo.StreamUrl;
    MediaPlayer->OpenSource(StreamMediaSource);
}

void URGRadioSubsystem::Play()
{
    if (MediaPlayer && MediaPlayer->CanPlay())
    {
        MediaPlayer->Play();
        SetPlaybackState(ERGRadioPlaybackState::Playing);
    }
    else
    {
        UE_LOG(LogRadio, Warning, TEXT("Play called but media not ready"));
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
    UE_LOG(LogRadio, Verbose, TEXT("Volume: %.2f"), CurrentVolume);
    // Volume is applied via UMediaSoundComponent in the level Blueprint
    // The Blueprint listens to OnPlaybackStateChanged and reads GetVolume()
}

void URGRadioSubsystem::SetMuted(bool bInMuted)
{
    bMuted = bInMuted;
    UE_LOG(LogRadio, Verbose, TEXT("Muted: %s"), bMuted ? TEXT("true") : TEXT("false"));
}

// ─── MediaPlayer callbacks ────────────────────────────────────────────────────

void URGRadioSubsystem::HandleMediaOpened(FString OpenedUrl)
{
    UE_LOG(LogRadio, Log, TEXT("Stream opened: %s"), *OpenedUrl);
    RetryCount = 0;
    SetPlaybackState(ERGRadioPlaybackState::Playing);
}

void URGRadioSubsystem::HandleMediaOpenFailed(FString FailedUrl)
{
    UE_LOG(LogRadio, Warning, TEXT("Stream open failed: %s (retry %d/%d)"),
        *FailedUrl, RetryCount + 1, MaxRetries);

    if (RetryCount < MaxRetries)
    {
        RetryCount++;
        // Retry after a short delay by reopening the same source
        SetPlaybackState(ERGRadioPlaybackState::Connecting);
        MediaPlayer->OpenSource(StreamMediaSource);
    }
    else
    {
        UE_LOG(LogRadio, Error, TEXT("Stream failed after %d retries: %s"), MaxRetries, *FailedUrl);
        SetPlaybackState(ERGRadioPlaybackState::Error);
        OnRadioError.Broadcast(ERGRadioPlaybackState::Error,
            FString::Printf(TEXT("Stream unavailable after %d retries"), MaxRetries));
    }
}

void URGRadioSubsystem::HandleMediaEndReached()
{
    // Internet radio streams are continuous — "end reached" means stream disconnected
    UE_LOG(LogRadio, Warning, TEXT("Stream disconnected — attempting reconnect"));
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
