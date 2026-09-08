// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Radio/RGRadioSubsystem.h"   // FRGRadioStreamInfo
#include "Map/RGMapSubsystem.h"       // FRGGeoCoordinate, FRGGuessResult
#include "RGMatchSubsystem.generated.h"

UENUM(BlueprintType)
enum class ERGMatchState : uint8
{
    Idle          UMETA(DisplayName = "Idle"),
    Loading       UMETA(DisplayName = "Loading"),
    RoundActive   UMETA(DisplayName = "Round Active"),
    RoundResult   UMETA(DisplayName = "Round Result"),
    MatchOver     UMETA(DisplayName = "Match Over"),
};

USTRUCT(BlueprintType)
struct RADIOGUESSER_API FRGRoundData
{
    GENERATED_BODY()

    /** Opaque token from server — no location info */
    UPROPERTY(BlueprintReadOnly) FString RoundToken;
    UPROPERTY(BlueprintReadOnly) FString StreamUrl;
    UPROPERTY(BlueprintReadOnly) FString DisplayName = TEXT("LIVE RADIO");
    UPROPERTY(BlueprintReadOnly) int32   RoundNumber = 1;
    UPROPERTY(BlueprintReadOnly) int32   TotalRounds = 5;
    UPROPERTY(BlueprintReadOnly) float   DurationSeconds = 120.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchStateChanged, ERGMatchState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundStarted,      FRGRoundData,  RoundData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundResultReady,  FRGGuessResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMatchCompleted);

/**
 * URGMatchSubsystem
 *
 * Orchestrates a solo or multiplayer match:
 *  - requests round data from the backend
 *  - starts radio playback
 *  - submits guess to backend
 *  - receives and displays result
 *
 * In multiplayer, the game server replicates state instead of
 * this subsystem fetching it directly.
 */
UCLASS()
class RADIOGUESSER_API URGMatchSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Solo match flow ───────────────────────────────────────────────────────

    /** Start a new solo match. Fetches first round from backend. */
    UFUNCTION(BlueprintCallable, Category = "Match")
    void StartSoloMatch(int32 NumRounds = 5);

    /** Called by PlayerController after player clicks CONFIRM GUESS */
    UFUNCTION(BlueprintCallable, Category = "Match")
    void SubmitGuess(FRGGeoCoordinate Coordinate);

    /** Proceed to the next round (called from Results screen) */
    UFUNCTION(BlueprintCallable, Category = "Match")
    void ProceedToNextRound();

    // ── State ─────────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintPure, Category = "Match")
    ERGMatchState GetMatchState() const { return MatchState; }

    UFUNCTION(BlueprintPure, Category = "Match")
    FRGRoundData GetCurrentRound() const { return CurrentRound; }

    UFUNCTION(BlueprintPure, Category = "Match")
    int32 GetTotalScore() const { return TotalScore; }

    // ── Events ────────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "Match")
    FOnMatchStateChanged OnMatchStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Match")
    FOnRoundStarted OnRoundStarted;

    UPROPERTY(BlueprintAssignable, Category = "Match")
    FOnRoundResultReady OnRoundResultReady;

    UPROPERTY(BlueprintAssignable, Category = "Match")
    FOnMatchCompleted OnMatchCompleted;

private:
    void SetMatchState(ERGMatchState NewState);
    void FetchNextRound();
    void OnRoundDataReceived(bool bSuccess, const FString& ResponseBody);
    void OnGuessResultReceived(bool bSuccess, const FString& ResponseBody);

    UPROPERTY() ERGMatchState MatchState    = ERGMatchState::Idle;
    UPROPERTY() FRGRoundData  CurrentRound;
    UPROPERTY() int32         TotalScore   = 0;
    UPROPERTY() int32         RoundsDone   = 0;
    UPROPERTY() int32         TotalRounds  = 5;
    UPROPERTY() bool          bGuessLocked = false;
};
