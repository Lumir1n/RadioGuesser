// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
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

protected:
    virtual void BeginPlay() override;
    void SetGamePhase(ERGGamePhase NewPhase);

    UPROPERTY(BlueprintReadOnly) ERGGamePhase CurrentPhase  = ERGGamePhase::WaitingToStart;
    UPROPERTY(BlueprintReadOnly) int32        CurrentRound  = 0;
};
