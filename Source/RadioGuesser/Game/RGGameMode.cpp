// Copyright RadioGuesser. All Rights Reserved.

#include "Game/RGGameMode.h"
#include "RadioGuesser.h"

ARGGameMode::ARGGameMode()
{
    // Defaults — subclasses override via their constructors or BP defaults
}

void ARGGameMode::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogMatch, Log, TEXT("ARGGameMode BeginPlay — TotalRounds=%d"), TotalRounds);
}

void ARGGameMode::SetGamePhase(ERGGamePhase NewPhase)
{
    if (CurrentPhase != NewPhase)
    {
        CurrentPhase = NewPhase;
        UE_LOG(LogMatch, Log, TEXT("Game phase changed to: %d"), static_cast<int32>(NewPhase));
    }
}

void ARGGameMode::StartRound()
{
    CurrentRound++;
    UE_LOG(LogMatch, Log, TEXT("StartRound %d / %d"), CurrentRound, TotalRounds);
    SetGamePhase(ERGGamePhase::RoundActive);
}

void ARGGameMode::EndRound()
{
    UE_LOG(LogMatch, Log, TEXT("EndRound %d"), CurrentRound);
    SetGamePhase(ERGGamePhase::RoundResult);
}

void ARGGameMode::EndMatch()
{
    UE_LOG(LogMatch, Log, TEXT("Match over after %d rounds"), CurrentRound);
    SetGamePhase(ERGGamePhase::MatchOver);
}
