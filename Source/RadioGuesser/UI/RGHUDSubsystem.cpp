// Copyright RadioGuesser. All Rights Reserved.

#include "UI/RGHUDSubsystem.h"
#include "RadioGuesser.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

void URGHUDSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogMatch, Log, TEXT("RGHUDSubsystem initialised"));
}

void URGHUDSubsystem::Deinitialize()
{
    StopTimer();
    Super::Deinitialize();
}

void URGHUDSubsystem::StartRoundTimer(float DurationSeconds)
{
    StopTimer();
    TimeRemaining = DurationSeconds;
    bTimerRunning = true;

    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (World)
    {
        World->GetTimerManager().SetTimer(
            TimerHandle,
            this,
            &URGHUDSubsystem::TickTimer,
            1.0f,
            true
        );
    }
    OnTimerTick.Broadcast(TimeRemaining);
}

void URGHUDSubsystem::StopTimer()
{
    bTimerRunning = false;
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (World)
    {
        World->GetTimerManager().ClearTimer(TimerHandle);
    }
}

void URGHUDSubsystem::TickTimer()
{
    TimeRemaining -= 1.0f;
    if (TimeRemaining <= 0.0f)
    {
        TimeRemaining = 0.0f;
        StopTimer();
        OnTimerTick.Broadcast(0.0f);
        OnTimerExpired.Broadcast();
    }
    else
    {
        OnTimerTick.Broadcast(TimeRemaining);
    }
}
