// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Match/RGMatchSubsystem.h"
#include "Radio/RGRadioSubsystem.h"
#include "UI/RGHUDSubsystem.h"
#include "RGGameHUDWidget.generated.h"

/**
 * URGGameHUDWidget
 *
 * C++ base for the in-game HUD widget.
 * Blueprint WBP_GameHUD subclasses this and binds:
 *   - Txt_RoundNumber       (text: "ROUND 1 / 5")
 *   - Txt_Timer             (text: "01:52")
 *   - Txt_RadioName         (text: "LIVE RADIO")
 *   - Txt_PlaybackState     (text: "● PLAYING" / "◌ CONNECTING...")
 *   - Slider_Volume         (float 0-1)
 *   - Btn_ConfirmGuess      (enabled only when guess is placed)
 *   - Img_GuessPinIndicator (visible after guess placed)
 *
 * All game logic lives in subsystems — this widget only reads state
 * and forwards button clicks.
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

    // ── Subsystem event handlers (bound in NativeConstruct) ───────────────────

    UFUNCTION()
    void HandleMatchStateChanged(ERGMatchState NewState);

    UFUNCTION()
    void HandleRoundStarted(FRGRoundData RoundData);

    UFUNCTION()
    void HandleTimerTick(float SecondsRemaining);

    UFUNCTION()
    void HandleTimerExpired();

    UFUNCTION()
    void HandlePlaybackStateChanged(ERGRadioPlaybackState NewState);

    UFUNCTION()
    void HandleGuessPlaced(FRGGeoCoordinate Coordinate);

    // ── Blueprint-implementable events ────────────────────────────────────────

    /** Override in Blueprint to update text/visibility based on current round */
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void OnRoundDataUpdated(const FRGRoundData& RoundData);

    /** Override in Blueprint to update timer display */
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void OnTimerUpdated(float SecondsRemaining);

    /** Override in Blueprint to update radio status display */
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void OnRadioStateUpdated(ERGRadioPlaybackState State);

    /** Override in Blueprint to enable/disable CONFIRM GUESS button */
    UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
    void OnGuessPinUpdated(bool bHasGuess);

    // ── State readable from Blueprint ─────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "HUD")
    bool HasPendingGuess() const { return bHasPendingGuess; }

    UFUNCTION(BlueprintPure, Category = "HUD")
    FString FormatTime(float Seconds) const;

private:
    UPROPERTY() bool bHasPendingGuess = false;
};
