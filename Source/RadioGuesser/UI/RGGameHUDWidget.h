// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Match/RGMatchSubsystem.h"   // FRGRoundData, ERGMatchState
#include "Radio/RGRadioSubsystem.h"   // ERGRadioPlaybackState
#include "Map/RGMapSubsystem.h"       // FRGGeoCoordinate
#include "RGGameHUDWidget.generated.h"

/**
 * URGGameHUDWidget
 *
 * C++ base for the in-game HUD widget.
 * Blueprint WBP_GameHUD subclasses this.
 *
 * Button bindings:
 *   Btn_ConfirmGuess → OnConfirmGuessClicked()
 *   Slider_Volume    → OnVolumeChanged(float)
 *   Btn_Mute         → OnMuteToggled()
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class RADIOGUESSER_API URGGameHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

protected:
    // ── Called by Blueprint button bindings ───────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void OnConfirmGuessClicked();

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void OnVolumeChanged(float NewVolume);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void OnMuteToggled();

    // ── Subsystem event handlers ──────────────────────────────────────────────

    UFUNCTION() void HandleMatchStateChanged(ERGMatchState NewState);
    UFUNCTION() void HandleRoundStarted(FRGRoundData RoundData);
    UFUNCTION() void HandleTimerTick(float SecondsRemaining);
    UFUNCTION() void HandleTimerExpired();
    UFUNCTION() void HandlePlaybackStateChanged(ERGRadioPlaybackState NewState);
    UFUNCTION() void HandleGuessPlaced(FRGGeoCoordinate Coordinate);

    // ── Blueprint-implementable events ────────────────────────────────────────

    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void OnRoundDataUpdated(const FRGRoundData& RoundData);

    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void OnTimerUpdated(float SecondsRemaining);

    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void OnRadioStateUpdated(ERGRadioPlaybackState State);

    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void OnGuessPinUpdated(bool bHasGuess);

    // ── Helpers ───────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "HUD")
    bool HasPendingGuess() const { return bHasPendingGuess; }

    UFUNCTION(BlueprintPure, Category = "HUD")
    FString FormatTime(float Seconds) const;

private:
    bool bHasPendingGuess = false;
};
