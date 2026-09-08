// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RGRadioSubsystem.generated.h"

class UMediaPlayer;
class UMediaSource;
class UStreamMediaSource;
class UMediaSoundComponent;

UENUM(BlueprintType)
enum class ERGRadioPlaybackState : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Connecting  UMETA(DisplayName = "Connecting"),
    Buffering   UMETA(DisplayName = "Buffering"),
    Playing     UMETA(DisplayName = "Playing"),
    Paused      UMETA(DisplayName = "Paused"),
    Error       UMETA(DisplayName = "Error"),
    Stopped     UMETA(DisplayName = "Stopped"),
};

USTRUCT(BlueprintType)
struct RADIOGUESSER_API FRGRadioStreamInfo
{
    GENERATED_BODY()

    /** Opaque round token — does NOT reveal station location */
    UPROPERTY(BlueprintReadOnly) FString RoundToken;
    /** Direct stream URL received from server */
    UPROPERTY(BlueprintReadOnly) FString StreamUrl;
    /** Display name shown in UI (e.g. "LIVE RADIO") */
    UPROPERTY(BlueprintReadOnly) FString DisplayName;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlaybackStateChanged, ERGRadioPlaybackState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRadioError, ERGRadioPlaybackState, State, const FString&, Message);

/**
 * URGRadioSubsystem
 *
 * Manages internet radio stream playback via Unreal's MediaPlayer framework.
 * Opens a UUrlMediaSource pointing to the stream URL from the server.
 * Never exposes station identity or geographic data.
 */
UCLASS()
class RADIOGUESSER_API URGRadioSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Playback ──────────────────────────────────────────────────────────────

    /** Open and begin streaming the given radio stream */
    UFUNCTION(BlueprintCallable, Category = "Radio")
    void OpenStream(const FRGRadioStreamInfo& StreamInfo);

    UFUNCTION(BlueprintCallable, Category = "Radio") void Play();
    UFUNCTION(BlueprintCallable, Category = "Radio") void Pause();
    UFUNCTION(BlueprintCallable, Category = "Radio") void Stop();

    UFUNCTION(BlueprintCallable, Category = "Radio")
    void SetVolume(float InVolume);

    UFUNCTION(BlueprintCallable, Category = "Radio")
    void SetMuted(bool bInMuted);

    // ── State ─────────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Radio")
    ERGRadioPlaybackState GetPlaybackState() const { return PlaybackState; }

    UFUNCTION(BlueprintPure, Category = "Radio")
    float GetVolume() const { return CurrentVolume; }

    UFUNCTION(BlueprintPure, Category = "Radio")
    bool IsMuted() const { return bMuted; }

    UFUNCTION(BlueprintPure, Category = "Radio")
    FString GetDisplayName() const { return ActiveStream.DisplayName; }

    /** Direct access for BP/UI to bind to the media player */
    UFUNCTION(BlueprintPure, Category = "Radio")
    UMediaPlayer* GetMediaPlayer() const { return MediaPlayer; }

    // ── Events ────────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "Radio")
    FOnPlaybackStateChanged OnPlaybackStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Radio")
    FOnRadioError OnRadioError;

private:
    UFUNCTION() void HandleMediaOpened(FString OpenedUrl);
    UFUNCTION() void HandleMediaOpenFailed(FString FailedUrl);
    UFUNCTION() void HandleMediaEndReached();

    void SetPlaybackState(ERGRadioPlaybackState NewState);
    void BindMediaPlayerDelegates();

    UPROPERTY() TObjectPtr<UMediaPlayer>       MediaPlayer;
    UPROPERTY() TObjectPtr<UStreamMediaSource> StreamMediaSource;

    FRGRadioStreamInfo          ActiveStream;
    ERGRadioPlaybackState       PlaybackState = ERGRadioPlaybackState::Idle;
    float                       CurrentVolume = 1.0f;
    bool                        bMuted        = false;

    static constexpr int32      MaxRetries    = 3;
    int32                       RetryCount    = 0;
};
