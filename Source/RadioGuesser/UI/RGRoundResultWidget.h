// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Map/RGMapSubsystem.h"
#include "Match/RGMatchSubsystem.h"
#include "RGRoundResultWidget.generated.h"

/**
 * URGRoundResultWidget
 *
 * Shown after a round ends. Displays:
 *   - Distance from guess to actual station
 *   - Score for this round
 *   - Total match score
 *   - Station name and country (now safe to reveal)
 *   - NEXT ROUND / FINISH MATCH button
 *
 * Blueprint WBP_RoundResult subclasses this.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class RADIOGUESSER_API URGRoundResultWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /**
     * Populate the widget with result data.
     * Called automatically when OnRoundResultReady fires.
     */
    UFUNCTION(BlueprintCallable, Category = "Results")
    void ShowResult(const FRGGuessResult& Result, int32 InTotalScore,
                    int32 InRoundNumber, int32 InTotalRounds);

protected:
    /** Override in Blueprint to animate/display the result values */
    UFUNCTION(BlueprintImplementableEvent, Category = "Results")
    void OnResultDataReady(
        float  DistanceKm,
        int32  RoundScore,
        int32  TotalScore,
        int32  RoundNumber,
        int32  TotalRounds,
        const FRGGeoCoordinate& GuessLocation,
        const FRGGeoCoordinate& ActualLocation);

    /** Override in Blueprint to update the button label (NEXT ROUND vs FINISH) */
    UFUNCTION(BlueprintImplementableEvent, Category = "Results")
    void OnFinalRound(bool bIsFinal);

    UFUNCTION(BlueprintCallable, Category = "Results")
    void OnNextRoundClicked();

private:
    UFUNCTION() void HandleRoundResultReady(FRGGuessResult Result);
    UFUNCTION() void HandleMatchCompleted();

    int32 CachedTotalScore  = 0;
    int32 CachedRoundNumber = 0;
    int32 CachedTotalRounds = 0;
};
