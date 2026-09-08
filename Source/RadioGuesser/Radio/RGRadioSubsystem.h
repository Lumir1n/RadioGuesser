// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RGRadioSubsystem.generated.h"

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

    /** Direct stream URL received from the server for this round */
    UPROPERTY(BlueprintReadOnly) FString StreamUrl;

    /** Display name shown in the UI (e.g. "LIVE RADIO") */
    UPROPERTY(BlueprintReadOnly) FString DisplayName;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlaybackStateChanged, ERGRadioPlaybackState, NewState);

/**
 * URGRadioSubsystem
 *
 * Manages radio stream discovery, playback lifecycle, and state.
 * Does NOT expose station coordinates or identity to the client.
 */
UCLASS()
class RADIOGUESSER_API URGRadioSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── USubsystem ────────────────────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Playback control ──────────────────────────────────────────────────────

    /** Begin streaming the station described by StreamInfo */
    UFUNCTION(BlueprintCallable, Category = "Radio")
    void OpenStream(const FRGRadioStreamInfo& StreamInfo);

    UFUNCTION(BlueprintCallable, Category = "Radio") void Play();
    UFUNCTION(BlueprintCallable, Category = "Radio") void Pause();
    UFUNCTION(BlueprintCallable, Category = "Radio") void Stop();

    UFUNCTION(BlueprintCallable, Category = "Radio")
    void SetVolume(float Volume);

    UFUNCTION(BlueprintCallable, Category = "Radio")
    void SetMuted(bool bMute);

    // ── State ─────────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Radio")
    ERGRadioPlaybackState GetPlaybackState() const { return PlaybackState; }

    UFUNCTION(BlueprintPure, Category = "Radio")
    float GetVolume() const { return CurrentVolume; }

    UFUNCTION(BlueprintPure, Category = "Radio")
    bool IsMuted() const { return bMuted; }

    // ── Events ────────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "Radio")
    FOnPlaybackStateChanged OnPlaybackStateChanged;

private:
    void SetPlaybackState(ERGRadioPlaybackState NewState);

    UPROPERTY() ERGRadioPlaybackState PlaybackState = ERGRadioPlaybackState::Idle;
    UPROPERTY() float CurrentVolume = 1.0f;
    UPROPERTY() bool  bMuted        = false;

    FRGRadioStreamInfo ActiveStream;
};
