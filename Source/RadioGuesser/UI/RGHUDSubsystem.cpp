// Copyright RadioGuesser. All Rights Reserved.

#include "UI/RGHUDSubsystem.h"
#include "RadioGuesser.h"
#include "TimerManager.h"
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

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UWorld* World = GI->GetWorld())
        {
            FTimerDelegate Del;
            Del.BindUObject(this, &URGHUDSubsystem::TickTimer);
            World->GetTimerManager().SetTimer(TimerHandle, Del, 1.0f, true);
        }
    }
    OnTimerTick.Broadcast(TimeRemaining);
}

void URGHUDSubsystem::StopTimer()
{
    bTimerRunning = false;

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UWorld* World = GI->GetWorld())
        {
            World->GetTimerManager().ClearTimer(TimerHandle);
        }
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
        UE_LOG(LogMatch, Log, TEXT("Round timer expired"));
    }
    else
    {
        OnTimerTick.Broadcast(TimeRemaining);
    }
}
