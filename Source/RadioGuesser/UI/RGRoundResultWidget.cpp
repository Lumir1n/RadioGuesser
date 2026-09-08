// Copyright RadioGuesser. All Rights Reserved.

#include "UI/RGRoundResultWidget.h"
#include "RadioGuesser.h"
#include "Engine/GameInstance.h"
#include "Match/RGMatchSubsystem.h"

void URGRoundResultWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>())
        {
            Match->OnRoundResultReady.AddDynamic(this, &URGRoundResultWidget::HandleRoundResultReady);
            Match->OnMatchCompleted.AddDynamic(this,   &URGRoundResultWidget::HandleMatchCompleted);
        }
    }
}

void URGRoundResultWidget::NativeDestruct()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>())
        {
            Match->OnRoundResultReady.RemoveAll(this);
            Match->OnMatchCompleted.RemoveAll(this);
        }
    }
    Super::NativeDestruct();
}

void URGRoundResultWidget::ShowResult(const FRGGuessResult& Result,
                                       int32 InTotalScore,
                                       int32 InRoundNumber,
                                       int32 InTotalRounds)
{
    CachedTotalScore  = InTotalScore;
    CachedRoundNumber = InRoundNumber;
    CachedTotalRounds = InTotalRounds;

    const bool bIsFinal = (InRoundNumber >= InTotalRounds);
    OnFinalRound(bIsFinal);

    OnResultDataReady(
        Result.DistanceKm,
        Result.Score,
        InTotalScore,
        InRoundNumber,
        InTotalRounds,
        Result.PlayerGuess,
        Result.ActualLocation
    );
}

void URGRoundResultWidget::OnNextRoundClicked()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>())
        {
            Match->ProceedToNextRound();
        }
    }
}

void URGRoundResultWidget::HandleRoundResultReady(FRGGuessResult Result)
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>())
        {
            ShowResult(Result,
                Match->GetTotalScore(),
                Match->GetCurrentRound().RoundNumber,
                Match->GetCurrentRound().TotalRounds);
        }
    }

    // Make this widget visible
    SetVisibility(ESlateVisibility::Visible);
}

void URGRoundResultWidget::HandleMatchCompleted()
{
    UE_LOG(LogMatch, Log, TEXT("Match completed — showing final result"));
    OnFinalRound(true);
}
