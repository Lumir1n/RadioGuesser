// Copyright RadioGuesser. All Rights Reserved.

#include "Game/RGGameMode.h"
#include "RadioGuesser.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Player/RGGlobePawn.h"

ARGGameMode::ARGGameMode()
{
    // Set Globe Pawn as the default pawn for all players
    DefaultPawnClass = ARGGlobePawn::StaticClass();
}

void ARGGameMode::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogMatch, Log, TEXT("ARGGameMode BeginPlay — TotalRounds=%d"), TotalRounds);

    // Create UI widgets
    CreateHUDWidget();
    CreateResultWidget();

    // Subscribe to match events
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>())
        {
            Match->OnRoundResultReady.AddDynamic(this, &ARGGameMode::HandleRoundResultReady);
            Match->OnMatchCompleted.AddDynamic(this,   &ARGGameMode::HandleMatchCompleted);
            Match->OnMatchStateChanged.AddDynamic(this, &ARGGameMode::HandleMatchStateChanged);
        }
    }
}

void ARGGameMode::CreateHUDWidget()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC) return;

    if (!HUDWidgetClass)
    {
        const FSoftClassPath HUDPath(TEXT("/Game/WBP_GameHUD.WBP_GameHUD_C"));
        HUDWidgetClass = TSoftClassPtr<URGGameHUDWidget>(HUDPath).LoadSynchronous();
    }

    if (!HUDWidgetClass)
    {
        UE_LOG(LogMatch, Warning, TEXT("ARGGameMode: HUDWidgetClass not set and WBP_GameHUD not found."));
        return;
    }

    HUDWidget = CreateWidget<URGGameHUDWidget>(PC, HUDWidgetClass);
    if (HUDWidget)
    {
        HUDWidget->AddToViewport(0);
        UE_LOG(LogMatch, Log, TEXT("ARGGameMode: HUD widget created and added to viewport."));
    }
}

void ARGGameMode::CreateResultWidget()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC) return;

    if (!ResultWidgetClass)
    {
        const FSoftClassPath ResultPath(TEXT("/Game/WBP_RoundResult.WBP_RoundResult_C"));
        ResultWidgetClass = TSoftClassPtr<URGRoundResultWidget>(ResultPath).LoadSynchronous();
    }

    if (!ResultWidgetClass)
    {
        UE_LOG(LogMatch, Warning, TEXT("ARGGameMode: WBP_RoundResult not found — result screen disabled."));
        return;
    }

    ResultWidget = CreateWidget<URGRoundResultWidget>(PC, ResultWidgetClass);
    if (ResultWidget)
    {
        // Start hidden — shown when a result arrives
        ResultWidget->SetVisibility(ESlateVisibility::Collapsed);
        ResultWidget->AddToViewport(1); // Higher Z-order than HUD
        UE_LOG(LogMatch, Log, TEXT("ARGGameMode: Result widget created (hidden)."));
    }
}

// ─── Match event handlers ─────────────────────────────────────────────────────

void ARGGameMode::HandleRoundResultReady(FRGGuessResult Result)
{
    // ResultWidget handles its own visibility via NativeConstruct bindings.
    // HUD should be hidden during result screen.
    if (HUDWidget)
    {
        HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void ARGGameMode::HandleMatchCompleted()
{
    UE_LOG(LogMatch, Log, TEXT("ARGGameMode: Match completed."));
}

void ARGGameMode::HandleMatchStateChanged(ERGMatchState NewState)
{
    if (NewState == ERGMatchState::RoundActive)
    {
        // New round started — show HUD, hide result screen
        if (HUDWidget)
        {
            HUDWidget->SetVisibility(ESlateVisibility::Visible);
        }
        if (ResultWidget)
        {
            ResultWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

// ─── Phase management ─────────────────────────────────────────────────────────

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
