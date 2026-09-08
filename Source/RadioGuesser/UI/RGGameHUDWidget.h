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
 * C++ base for the in-game HUD Blueprint widget (WBP_GameHUD).
 *
 * In Blueprint, bind:
 *   Btn_ConfirmGuess.OnClicked → OnConfirmGuessClicked()
 *   Slider_Volume.OnValueChanged → OnVolumeChanged(float)
 *
 * Override these Blueprint events to update visuals:
 *   OnRoundDataUpdated  — update round number text
 *   OnTimerUpdated      — update countdown text
 *   OnRadioStateUpdated — update radio status label
 *   OnGuessPinUpdated   — enable/disable confirm button
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class RADIOGUESSER_API URGGameHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

protected:
    // ── Button callbacks (call from Blueprint) ────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void OnConfirmGuessClicked();

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void OnVolumeChanged(float NewVolume);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void OnMuteToggled();

    // ── Subsystem event handlers (bound in NativeConstruct) ───────────────────

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

    /** Format seconds as "MM:SS" */
    UFUNCTION(BlueprintPure, Category = "HUD")
    FString FormatTime(float Seconds) const;

private:
    bool bHasPendingGuess = false;
};
