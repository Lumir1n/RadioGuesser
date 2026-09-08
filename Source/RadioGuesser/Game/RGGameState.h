// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Game/RGGameMode.h"
#include "RGGameState.generated.h"  // must be last

/**
 * ARGGameState
 *
 * Replicated match state visible to all connected clients.
 * Never holds the actual station location during active guessing.
 */
UCLASS()
class RADIOGUESSER_API ARGGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    // ── Replicated fields ─────────────────────────────────────────────────────

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match State")
    ERGGamePhase GamePhase = ERGGamePhase::WaitingToStart;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match State")
    int32 CurrentRound = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match State")
    int32 TotalRounds = 5;

    /** Seconds remaining in current round (updated by server) */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match State")
    float RoundTimeRemaining = 0.0f;

    /**
     * Public round token — opaque identifier sent to clients.
     * Does NOT contain station coordinates or country.
     */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match State")
    FString RoundToken;

    /** Display name shown in UI: e.g. "LIVE RADIO" */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match State")
    FString StationDisplayName;

    /** Stream URL clients use to connect directly to the radio station */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match State")
    FString StreamUrl;

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
