// Copyright RadioGuesser. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Map/RGMapSubsystem.h"   // FRGGeoCoordinate, FRGGuessResult
#include "RGRoundResultWidget.generated.h"

/**
 * URGRoundResultWidget
 *
 * C++ base for the round results screen (WBP_RoundResult).
 * Shows distance, score, and a Next Round / Finish button.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class RADIOGUESSER_API URGRoundResultWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /** Called by C++ when a result arrives — forwards to Blueprint event */
    UFUNCTION(BlueprintCallable, Category = "Results")
    void ShowResult(const FRGGuessResult& Result,
                    int32 InTotalScore, int32 InRoundNumber, int32 InTotalRounds);

protected:
    /** Override in Blueprint to animate/display values */
    UFUNCTION(BlueprintImplementableEvent, Category = "Results")
    void OnResultDataReady(float DistanceKm, int32 RoundScore, int32 TotalScore,
                           int32 RoundNumber, int32 TotalRounds,
                           const FRGGeoCoordinate& GuessLocation,
                           const FRGGeoCoordinate& ActualLocation);

    /** Override to update the action button label */
    UFUNCTION(BlueprintImplementableEvent, Category = "Results")
    void OnFinalRound(bool bIsFinal);

    /** Bind to "NEXT ROUND" / "FINISH" button in Blueprint */
    UFUNCTION(BlueprintCallable, Category = "Results")
    void OnNextRoundClicked();

private:
    UFUNCTION() void HandleRoundResultReady(FRGGuessResult Result);
    UFUNCTION() void HandleMatchCompleted();
};
