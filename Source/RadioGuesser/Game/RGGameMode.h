// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UI/RGGameHUDWidget.h"
#include "UI/RGRoundResultWidget.h"
#include "Match/RGMatchSubsystem.h"
#include "Player/RGGlobePawn.h"
#include "RGGameMode.generated.h"

UENUM(BlueprintType)
enum class ERGGamePhase : uint8
{
    WaitingToStart UMETA(DisplayName = "Waiting to Start"),
    RoundActive    UMETA(DisplayName = "Round Active"),
    RoundResult    UMETA(DisplayName = "Round Result"),
    MatchOver      UMETA(DisplayName = "Match Over"),
};

/**
 * ARGGameMode
 *
 * Base game mode for all RadioGuesser modes.
 * Subclassed for Solo, Multiplayer, Ranked, Explorer, etc.
 * The authoritative game server runs a subclass of this.
 */
UCLASS()
class RADIOGUESSER_API ARGGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ARGGameMode();

    // ── Phase management ──────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category = "Game")
    virtual void StartRound();

    UFUNCTION(BlueprintCallable, Category = "Game")
    virtual void EndRound();

    UFUNCTION(BlueprintCallable, Category = "Game")
    virtual void EndMatch();

    UFUNCTION(BlueprintPure, Category = "Game")
    ERGGamePhase GetGamePhase() const { return CurrentPhase; }

    // ── Config ────────────────────────────────────────────────────────────────

    /** Total rounds in a match (configurable per mode) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Config")
    int32 TotalRounds = 5;

    /** Seconds per round (0 = no time limit for solo mode) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Config")
    float RoundDurationSeconds = 120.0f;

    /**
     * Widget class to use for the in-game HUD.
     * Set this in BP_RGGameMode defaults to WBP_GameHUD.
     * If not set, the game mode will try to find WBP_GameHUD automatically.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<URGGameHUDWidget> HUDWidgetClass;

    /**
     * Widget class for the round result screen.
     * Auto-loaded from WBP_RoundResult if not set.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<URGRoundResultWidget> ResultWidgetClass;

protected:
    virtual void BeginPlay() override;
    void SetGamePhase(ERGGamePhase NewPhase);
    void CreateHUDWidget();
    void CreateResultWidget();

    UFUNCTION() void HandleRoundResultReady(FRGGuessResult Result);
    UFUNCTION() void HandleMatchCompleted();
    UFUNCTION() void HandleMatchStateChanged(ERGMatchState NewState);

    UPROPERTY(BlueprintReadOnly) ERGGamePhase      CurrentPhase  = ERGGamePhase::WaitingToStart;
    UPROPERTY(BlueprintReadOnly) int32             CurrentRound  = 0;
    UPROPERTY(BlueprintReadOnly) URGGameHUDWidget*    HUDWidget    = nullptr;
    UPROPERTY(BlueprintReadOnly) URGRoundResultWidget* ResultWidget = nullptr;
};
