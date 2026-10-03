// Copyright RadioGuesser. All Rights Reserved.

#include "Match/RGMatchSubsystem.h"
#include "RadioGuesser.h"
#include "API/RGBackendSubsystem.h"
#include "Radio/RGRadioSubsystem.h"
#include "Map/RGMapSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void URGMatchSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogMatch, Log, TEXT("RGMatchSubsystem initialised"));
}

void URGMatchSubsystem::Deinitialize()
{
    UE_LOG(LogMatch, Log, TEXT("RGMatchSubsystem deinitialised"));
    Super::Deinitialize();
}

// ─── Match flow ───────────────────────────────────────────────────────────────

void URGMatchSubsystem::StartSoloMatch(int32 NumRounds)
{
    if (MatchState != ERGMatchState::Idle)
    {
        UE_LOG(LogMatch, Warning, TEXT("StartSoloMatch called while match already running (state=%d) — ignoring"),
            static_cast<int32>(MatchState));
        return;
    }

    TotalScore  = 0;
    RoundsDone  = 0;
    TotalRounds = NumRounds;
    bGuessLocked = false;
    RetryCount   = 0;

    UE_LOG(LogMatch, Log, TEXT("Starting solo match — %d rounds"), TotalRounds);
    SetMatchState(ERGMatchState::Loading);
    FetchNextRound();
}

void URGMatchSubsystem::FetchNextRound()
{
    URGBackendSubsystem* Backend = GetGameInstance()->GetSubsystem<URGBackendSubsystem>();
    if (!Backend)
    {
        UE_LOG(LogMatch, Error, TEXT("RGBackendSubsystem not available"));
        return;
    }

    SetMatchState(ERGMatchState::Loading);

    FOnHttpResponse Callback;
    Callback.BindUObject(this, &URGMatchSubsystem::OnRoundDataReceived);
    Backend->PostAsync(
        TEXT("/api/v1/games/solo/round"),
        FString::Printf(TEXT("{\"roundNumber\":%d,\"totalRounds\":%d}"), RoundsDone + 1, TotalRounds),
        Callback
    );
}

void URGMatchSubsystem::OnRoundDataReceived(bool bSuccess, const FString& ResponseBody)
{
    if (!bSuccess)
    {
        RetryCount++;
        UE_LOG(LogMatch, Warning, TEXT("Failed to fetch round data (attempt %d/%d): %s"),
            RetryCount, MaxRetries, *ResponseBody);

        if (RetryCount < MaxRetries)
        {
            // Retry after RetryDelaySeconds — stay in Loading state
            if (UWorld* World = GetGameInstance()->GetWorld())
            {
                FTimerDelegate Del;
                Del.BindUObject(this, &URGMatchSubsystem::FetchNextRound);
                World->GetTimerManager().SetTimer(RetryTimerHandle, Del, RetryDelaySeconds, false);
                UE_LOG(LogMatch, Log, TEXT("Retrying in %.1f s..."), RetryDelaySeconds);
            }
        }
        else
        {
            UE_LOG(LogMatch, Error, TEXT("All %d retries exhausted — match cannot start"), MaxRetries);
            SetMatchState(ERGMatchState::Idle);
            RetryCount = 0;
        }
        return;
    }

    // Successful response — reset retry counter
    RetryCount = 0;

    // Parse JSON: { roundToken, streamUrl, displayName, roundNumber, totalRounds, durationSeconds }
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
    {
        UE_LOG(LogMatch, Error, TEXT("Invalid round JSON: %s"), *ResponseBody);
        // Treat malformed JSON like a network failure and retry
        OnRoundDataReceived(false, TEXT("Malformed JSON"));
        return;
    }

    FRGRoundData RoundData;
    RoundData.RoundToken      = JsonObj->GetStringField(TEXT("roundToken"));
    RoundData.StreamUrl       = JsonObj->GetStringField(TEXT("streamUrl"));
    RoundData.DisplayName     = JsonObj->GetStringField(TEXT("displayName"));
    RoundData.RoundNumber     = JsonObj->GetIntegerField(TEXT("roundNumber"));
    RoundData.TotalRounds     = JsonObj->GetIntegerField(TEXT("totalRounds"));
    RoundData.DurationSeconds = static_cast<float>(JsonObj->GetNumberField(TEXT("durationSeconds")));

    CurrentRound = RoundData;
    bGuessLocked = false;
    RoundsDone++;

    UE_LOG(LogMatch, Log, TEXT("Round %d/%d ready — token=%s"),
        RoundData.RoundNumber, RoundData.TotalRounds, *RoundData.RoundToken);

    // Clear previous guess from map
    if (URGMapSubsystem* MapSub = GetGameInstance()->GetSubsystem<URGMapSubsystem>())
    {
        MapSub->ClearGuess();
    }

    // Start radio playback
    if (URGRadioSubsystem* RadioSub = GetGameInstance()->GetSubsystem<URGRadioSubsystem>())
    {
        FRGRadioStreamInfo StreamInfo;
        StreamInfo.RoundToken  = RoundData.RoundToken;
        StreamInfo.StreamUrl   = RoundData.StreamUrl;
        StreamInfo.DisplayName = RoundData.DisplayName;
        RadioSub->OpenStream(StreamInfo);
        RadioSub->Play();
    }

    SetMatchState(ERGMatchState::RoundActive);
    OnRoundStarted.Broadcast(RoundData);
}

void URGMatchSubsystem::SubmitGuess(FRGGeoCoordinate Coordinate)
{
    if (bGuessLocked)
    {
        UE_LOG(LogMatch, Warning, TEXT("Guess already locked for this round"));
        return;
    }
    if (CurrentRound.RoundToken.IsEmpty())
    {
        UE_LOG(LogMatch, Error, TEXT("No active round token"));
        return;
    }

    bGuessLocked = true;

    // Stop radio so player focuses on result
    if (URGRadioSubsystem* RadioSub = GetGameInstance()->GetSubsystem<URGRadioSubsystem>())
    {
        RadioSub->Stop();
    }

    URGBackendSubsystem* Backend = GetGameInstance()->GetSubsystem<URGBackendSubsystem>();
    if (!Backend) return;

    const FString Body = FString::Printf(
        TEXT("{\"roundToken\":\"%s\",\"latitude\":%.6f,\"longitude\":%.6f}"),
        *CurrentRound.RoundToken,
        Coordinate.Latitude,
        Coordinate.Longitude
    );

    FOnHttpResponse Callback;
    Callback.BindUObject(this, &URGMatchSubsystem::OnGuessResultReceived);
    Backend->PostAsync(TEXT("/api/v1/games/solo/guess"), Body, Callback);
}

void URGMatchSubsystem::OnGuessResultReceived(bool bSuccess, const FString& ResponseBody)
{
    if (!bSuccess)
    {
        UE_LOG(LogMatch, Error, TEXT("Guess submission failed — showing fallback result: %s"), *ResponseBody);
        // Don't leave the player stuck in RoundActive. Build a minimal fallback result
        // so the result screen appears and ProceedToNextRound works.
        FRGGuessResult Fallback;
        if (URGMapSubsystem* MapSub = GetGameInstance()->GetSubsystem<URGMapSubsystem>())
        {
            Fallback.PlayerGuess = MapSub->GetPendingGuess();
        }
        Fallback.ActualLocation = Fallback.PlayerGuess; // unknown — show same pin
        Fallback.DistanceKm     = -1.f;                 // sentinel: backend unavailable
        Fallback.Score          = 0;
        Fallback.StationName    = TEXT("Unknown (backend offline)");
        Fallback.Country        = TEXT("");

        if (URGMapSubsystem* MapSub = GetGameInstance()->GetSubsystem<URGMapSubsystem>())
        {
            MapSub->ShowResult(Fallback);
        }
        SetMatchState(ERGMatchState::RoundResult);
        OnRoundResultReady.Broadcast(Fallback);
        return;
    }

    // Parse JSON: { guessLat, guessLon, actualLat, actualLon, distanceKm, score }
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
    {
        UE_LOG(LogMatch, Error, TEXT("Invalid guess result JSON: %s"), *ResponseBody);
        // Treat as failure
        OnGuessResultReceived(false, TEXT("Malformed JSON"));
        return;
    }

    FRGGuessResult Result;
    Result.PlayerGuess.Latitude   = JsonObj->GetNumberField(TEXT("guessLat"));
    Result.PlayerGuess.Longitude  = JsonObj->GetNumberField(TEXT("guessLon"));
    Result.ActualLocation.Latitude  = JsonObj->GetNumberField(TEXT("actualLat"));
    Result.ActualLocation.Longitude = JsonObj->GetNumberField(TEXT("actualLon"));
    Result.DistanceKm = static_cast<float>(JsonObj->GetNumberField(TEXT("distanceKm")));
    Result.Score      = JsonObj->GetIntegerField(TEXT("score"));
    JsonObj->TryGetStringField(TEXT("stationName"), Result.StationName);
    JsonObj->TryGetStringField(TEXT("country"), Result.Country);

    TotalScore += Result.Score;
    UE_LOG(LogMatch, Log, TEXT("Round result: dist=%.1f km score=%d total=%d"),
        Result.DistanceKm, Result.Score, TotalScore);

    // Show result on map
    if (URGMapSubsystem* MapSub = GetGameInstance()->GetSubsystem<URGMapSubsystem>())
    {
        MapSub->ShowResult(Result);
    }

    SetMatchState(ERGMatchState::RoundResult);
    OnRoundResultReady.Broadcast(Result);
}

void URGMatchSubsystem::ProceedToNextRound()
{
    if (RoundsDone >= TotalRounds)
    {
        UE_LOG(LogMatch, Log, TEXT("Match complete — total score: %d"), TotalScore);
        SetMatchState(ERGMatchState::MatchOver);
        OnMatchCompleted.Broadcast();
        return;
    }

    FetchNextRound();
}

void URGMatchSubsystem::SetMatchState(ERGMatchState NewState)
{
    if (MatchState != NewState)
    {
        MatchState = NewState;
        OnMatchStateChanged.Broadcast(NewState);
    }
}
