// Copyright RadioGuesser. All Rights Reserved.

#include "UI/RGGameHUDWidget.h"
#include "RadioGuesser.h"
#include "Engine/GameInstance.h"
#include "Match/RGMatchSubsystem.h"
#include "Radio/RGRadioSubsystem.h"
#include "Map/RGMapSubsystem.h"
#include "UI/RGHUDSubsystem.h"

void URGGameHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    if (URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>())
    {
        Match->OnMatchStateChanged.AddDynamic(this, &URGGameHUDWidget::HandleMatchStateChanged);
        Match->OnRoundStarted.AddDynamic(this,      &URGGameHUDWidget::HandleRoundStarted);
    }

    if (URGHUDSubsystem* HUD = GI->GetSubsystem<URGHUDSubsystem>())
    {
        HUD->OnTimerTick.AddDynamic(this,    &URGGameHUDWidget::HandleTimerTick);
        HUD->OnTimerExpired.AddDynamic(this, &URGGameHUDWidget::HandleTimerExpired);
    }

    if (URGRadioSubsystem* Radio = GI->GetSubsystem<URGRadioSubsystem>())
    {
        Radio->OnPlaybackStateChanged.AddDynamic(this, &URGGameHUDWidget::HandlePlaybackStateChanged);
    }

    if (URGMapSubsystem* Map = GI->GetSubsystem<URGMapSubsystem>())
    {
        Map->OnGuessPlaced.AddDynamic(this, &URGGameHUDWidget::HandleGuessPlaced);
    }
}

void URGGameHUDWidget::NativeDestruct()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>())
        {
            Match->OnMatchStateChanged.RemoveAll(this);
            Match->OnRoundStarted.RemoveAll(this);
        }
        if (URGHUDSubsystem* HUD = GI->GetSubsystem<URGHUDSubsystem>())
        {
            HUD->OnTimerTick.RemoveAll(this);
            HUD->OnTimerExpired.RemoveAll(this);
        }
        if (URGRadioSubsystem* Radio = GI->GetSubsystem<URGRadioSubsystem>())
        {
            Radio->OnPlaybackStateChanged.RemoveAll(this);
        }
        if (URGMapSubsystem* Map = GI->GetSubsystem<URGMapSubsystem>())
        {
            Map->OnGuessPlaced.RemoveAll(this);
        }
    }
    Super::NativeDestruct();
}

// ─── Button callbacks ─────────────────────────────────────────────────────────

void URGGameHUDWidget::OnConfirmGuessClicked()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    URGMapSubsystem*   Map   = GI->GetSubsystem<URGMapSubsystem>();
    URGMatchSubsystem* Match = GI->GetSubsystem<URGMatchSubsystem>();

    if (!Map || !Match || !Map->HasPendingGuess()) return;

    Match->SubmitGuess(Map->GetPendingGuess());
    bHasPendingGuess = false;
    OnGuessPinUpdated(false);
}

void URGGameHUDWidget::OnVolumeChanged(float NewVolume)
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGRadioSubsystem* Radio = GI->GetSubsystem<URGRadioSubsystem>())
        {
            Radio->SetVolume(NewVolume);
        }
    }
}

void URGGameHUDWidget::OnMuteToggled()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGRadioSubsystem* Radio = GI->GetSubsystem<URGRadioSubsystem>())
        {
            Radio->SetMuted(!Radio->IsMuted());
        }
    }
}

// ─── Subsystem event handlers ─────────────────────────────────────────────────

void URGGameHUDWidget::HandleMatchStateChanged(ERGMatchState NewState)
{
    if (NewState == ERGMatchState::RoundActive)
    {
        bHasPendingGuess = false;
        OnGuessPinUpdated(false);
    }
}

void URGGameHUDWidget::HandleRoundStarted(FRGRoundData RoundData)
{
    bHasPendingGuess = false;
    OnRoundDataUpdated(RoundData);
    OnGuessPinUpdated(false);

    // Start the round countdown timer
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URGHUDSubsystem* HUD = GI->GetSubsystem<URGHUDSubsystem>())
        {
            HUD->StartRoundTimer(RoundData.DurationSeconds);
        }
    }
}

void URGGameHUDWidget::HandleTimerTick(float SecondsRemaining)
{
    OnTimerUpdated(SecondsRemaining);
}

void URGGameHUDWidget::HandleTimerExpired()
{
    UE_LOG(LogMatch, Log, TEXT("HUD: timer expired — auto-confirming guess"));
    // Auto-submit whatever is placed; if nothing placed, submit (0,0)
    OnConfirmGuessClicked();
}

void URGGameHUDWidget::HandlePlaybackStateChanged(ERGRadioPlaybackState NewState)
{
    OnRadioStateUpdated(NewState);
}

void URGGameHUDWidget::HandleGuessPlaced(FRGGeoCoordinate /*Coordinate*/)
{
    bHasPendingGuess = true;
    OnGuessPinUpdated(true);
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

FString URGGameHUDWidget::FormatTime(float Seconds) const
{
    const int32 Mins = FMath::FloorToInt(Seconds / 60.f);
    const int32 Secs = FMath::FloorToInt(Seconds) % 60;
    return FString::Printf(TEXT("%02d:%02d"), Mins, Secs);
}
