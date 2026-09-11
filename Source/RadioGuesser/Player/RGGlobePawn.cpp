// Copyright RadioGuesser. All Rights Reserved.

#include "Player/RGGlobePawn.h"
#include "RadioGuesser.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"
#include "Engine/EngineTypes.h"
#include "CesiumGeoreference.h"

namespace
{
    constexpr double MetersPerDegreeLat = 111320.0;

    double WrapLongitude(double Longitude)
    {
        Longitude = FMath::Fmod(Longitude + 180.0, 360.0);
        if (Longitude < 0.0)
        {
            Longitude += 360.0;
        }
        return Longitude - 180.0;
    }
}

ARGGlobePawn::ARGGlobePawn()
{
    PrimaryActorTick.bCanEverTick = true;

    GlobeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GlobeRoot"));
    SetRootComponent(GlobeRoot);

    // SpringArm is kept at zero length so existing Blueprints still compile.
    // Pose comes from lat/lon/height, not from orbiting the actor origin.
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GlobeRoot);
    SpringArm->TargetArmLength       = 0.0f;
    SpringArm->bDoCollisionTest      = false;
    SpringArm->bEnableCameraLag      = false;
    SpringArm->bUsePawnControlRotation = false;
    SpringArm->bInheritPitch         = true;
    SpringArm->bInheritYaw           = true;
    SpringArm->bInheritRoll          = true;
    SpringArm->SetRelativeRotation(FRotator::ZeroRotator);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;
    Camera->SetFieldOfView(60.0f);

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw   = false;
    bUseControllerRotationRoll  = false;

    MappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_Globe"));

    IA_Drag = CreateDefaultSubobject<UInputAction>(TEXT("IA_GlobeDrag"));
    IA_Drag->ValueType = EInputActionValueType::Boolean;

    IA_Zoom = CreateDefaultSubobject<UInputAction>(TEXT("IA_GlobeZoom"));
    IA_Zoom->ValueType = EInputActionValueType::Axis1D;

    IA_MouseXY = CreateDefaultSubobject<UInputAction>(TEXT("IA_GlobeMouseXY"));
    IA_MouseXY->ValueType = EInputActionValueType::Axis2D;

    MappingContext->MapKey(IA_Drag,    EKeys::LeftMouseButton);
    MappingContext->MapKey(IA_Zoom,    EKeys::MouseWheelAxis);
    MappingContext->MapKey(IA_MouseXY, EKeys::Mouse2D);
}

void ARGGlobePawn::BeginPlay()
{
    Super::BeginPlay();

    CachedGeoreference = FindGeoreference();
    CurrentArmLength = FMath::Clamp(CurrentArmLength, MinArmLength, MaxArmLength);
    ApplyCameraToGlobe();

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (auto* Sys = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
            {
                Sys->AddMappingContext(MappingContext, 0);
            }
        }
    }
}

void ARGGlobePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EIC->BindAction(IA_Drag,    ETriggerEvent::Started,   this, &ARGGlobePawn::OnDragStarted);
        EIC->BindAction(IA_Drag,    ETriggerEvent::Completed, this, &ARGGlobePawn::OnDragStopped);
        EIC->BindAction(IA_Drag,    ETriggerEvent::Canceled,  this, &ARGGlobePawn::OnDragStopped);
        EIC->BindAction(IA_Zoom,    ETriggerEvent::Triggered, this, &ARGGlobePawn::OnZoom);
        EIC->BindAction(IA_MouseXY, ETriggerEvent::Triggered, this, &ARGGlobePawn::OnMouseXY);
    }

    PlayerInputComponent->BindAxis("MoveForward", this, &ARGGlobePawn::MoveForward);
    PlayerInputComponent->BindAxis("MoveRight",   this, &ARGGlobePawn::MoveRight);
}

void ARGGlobePawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!CachedGeoreference)
    {
        CachedGeoreference = FindGeoreference();
    }

    if (bIsDragging)
    {
        ApplyDragPan();
    }

    ApplyCameraToGlobe();
}

ACesiumGeoreference* ARGGlobePawn::FindGeoreference() const
{
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
        {
            return *It;
        }
    }
    return nullptr;
}

void ARGGlobePawn::ApplyCameraToGlobe()
{
    if (!CachedGeoreference)
    {
        return;
    }

    // Keep the Unreal origin roughly under the look-at point to prevent
    // world-bounds / far-clip issues at extreme longitude/latitude values.
    //
    // IMPORTANT: SetOriginLongitudeLatitudeHeight triggers a full Cesium tile
    // tree rebuild which causes the visible seam / "map splits in two" artefact.
    // We therefore rebase ONLY when:
    //   (a) the view has drifted more than 2° from the last applied origin, AND
    //   (b) the camera is high enough that the seam won't be obvious (> 300 km).
    // At close range floating-point error is negligible and rebasing does more
    // harm than good (tears between LOD levels).
    const double DeltaLon = FMath::Abs(ViewLongitude - AppliedOriginLongitude);
    const double DeltaLat = FMath::Abs(ViewLatitude  - AppliedOriginLatitude);
    const bool bFarFromOrigin  = (DeltaLon > 2.0 || DeltaLat > 2.0);
    const bool bHighEnough     = (CurrentArmLength > 30000000.0f); // > 300 km
    const bool bNeverApplied   = (AppliedOriginLongitude > 1.0e30);

    if (bNeverApplied || (bFarFromOrigin && bHighEnough))
    {
        CachedGeoreference->SetOriginLongitudeLatitudeHeight(
            FVector(ViewLongitude, ViewLatitude, 0.0));
        AppliedOriginLongitude = ViewLongitude;
        AppliedOriginLatitude  = ViewLatitude;
    }

    SetActorLocation(FVector(0.0, 0.0, CurrentArmLength));

    // At a Cesium cartographic origin: +X east, +Y south, +Z up.
    // Look straight down with north at the top of the screen.
    SetActorRotation(
        FRotationMatrix::MakeFromXZ(FVector::DownVector, FVector(0.0f, -1.0f, 0.0f)).Rotator());
}

void ARGGlobePawn::ApplyDragPan()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !CachedGeoreference)
    {
        LastMouseDelta = FVector2D::ZeroVector;
        return;
    }

    FVector2D CursorPos = LastCursorPos;
    const bool bGotCursor = PC->GetMousePosition(CursorPos.X, CursorPos.Y);

    FVector2D Delta = LastMouseDelta;
    LastMouseDelta = FVector2D::ZeroVector;

    if (bGotCursor)
    {
        if (bHaveCursorPos)
        {
            Delta = CursorPos - LastCursorPos;
        }
        LastCursorPos  = CursorPos;
        bHaveCursorPos = true;
    }

    TotalDragPixels += Delta.Size();
    if (TotalDragPixels >= ClickDragThreshold)
    {
        bDragExceededThreshold = true;
    }

    const float NsScale = GetNorthSouthPanScale();

    // Grab-to-cursor: the geographic point picked on press stays under the cursor.
    if (bHasGrabPoint)
    {
        FHitResult Hit;
        const bool bHit = PC->GetHitResultUnderCursorByChannel(
            UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit);

        if (bHit && Hit.bBlockingHit)
        {
            const FVector HitLLH =
                CachedGeoreference->TransformUnrealPositionToLongitudeLatitudeHeight(
                    Hit.ImpactPoint);

            ViewLongitude = WrapLongitude(ViewLongitude + (GrabLongitude - HitLLH.X));
            ViewLatitude  = FMath::Clamp(
                ViewLatitude + (HitLLH.Y - GrabLatitude) * static_cast<double>(NsScale),
                -static_cast<double>(MaxAbsLatitude),
                static_cast<double>(MaxAbsLatitude));
            return;
        }
    }

    if (Delta.IsNearlyZero())
    {
        return;
    }

    int32 SizeX = 1920;
    int32 SizeY = 1080;
    PC->GetViewportSize(SizeX, SizeY);
    (void)SizeX;
    SizeY = FMath::Max(SizeY, 1);

    const float VerticalFovDeg = Camera ? Camera->FieldOfView : 90.0f;
    const double HalfFovRad = FMath::DegreesToRadians(VerticalFovDeg * 0.5);
    const double MetersPerPixel =
        (2.0 * GetViewHeightMeters() * FMath::Tan(HalfFovRad)) / static_cast<double>(SizeY);

    // Visible cursor: dragging the globe moves it with the mouse (Google Maps).
    const double EastMeters  = -static_cast<double>(Delta.X) * MetersPerPixel;
    const double NorthMeters = static_cast<double>(Delta.Y) * MetersPerPixel * NsScale;
    PanByMeters(EastMeters, NorthMeters);
}

void ARGGlobePawn::PanByMeters(double EastMeters, double NorthMeters)
{
    const double CosLat = FMath::Max(
        0.05,
        FMath::Abs(FMath::Cos(FMath::DegreesToRadians(ViewLatitude))));

    ViewLatitude = FMath::Clamp(
        ViewLatitude + NorthMeters / MetersPerDegreeLat,
        -static_cast<double>(MaxAbsLatitude),
        static_cast<double>(MaxAbsLatitude));

    ViewLongitude = WrapLongitude(ViewLongitude + EastMeters / (MetersPerDegreeLat * CosLat));
}

float ARGGlobePawn::GetNorthSouthPanScale() const
{
    const float Span = FMath::Max(1.0f, PlanetViewFullHeight - PlanetViewStartHeight);
    const float Alpha = FMath::Clamp(
        (CurrentArmLength - PlanetViewStartHeight) / Span,
        0.0f,
        1.0f);
    return 1.0f - Alpha;
}

void ARGGlobePawn::OnDragStarted(const FInputActionValue& /*Value*/)
{
    bIsDragging            = true;
    bDragExceededThreshold = false;
    bHasGrabPoint          = false;
    TotalDragPixels        = 0.0f;
    LastMouseDelta         = FVector2D::ZeroVector;
    bHaveCursorPos         = false;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !CachedGeoreference)
    {
        return;
    }

    if (PC->GetMousePosition(LastCursorPos.X, LastCursorPos.Y))
    {
        bHaveCursorPos = true;
    }

    FHitResult Hit;
    if (PC->GetHitResultUnderCursorByChannel(
            UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit) && Hit.bBlockingHit)
    {
        const FVector GrabLLH =
            CachedGeoreference->TransformUnrealPositionToLongitudeLatitudeHeight(
                Hit.ImpactPoint);
        GrabLongitude = GrabLLH.X;
        GrabLatitude  = GrabLLH.Y;
        bHasGrabPoint = true;
    }
}

void ARGGlobePawn::OnDragStopped(const FInputActionValue& /*Value*/)
{
    bIsDragging    = false;
    bHasGrabPoint  = false;
    bHaveCursorPos = false;
    LastMouseDelta = FVector2D::ZeroVector;
}

void ARGGlobePawn::OnZoom(const FInputActionValue& Value)
{
    const float Axis = Value.Get<float>();
    const float ZoomDelta = CurrentArmLength * 0.15f * Axis;
    CurrentArmLength = FMath::Clamp(
        CurrentArmLength - ZoomDelta,
        MinArmLength,
        MaxArmLength);
}

void ARGGlobePawn::OnMouseXY(const FInputActionValue& Value)
{
    LastMouseDelta = Value.Get<FVector2D>();
}

void ARGGlobePawn::MoveForward(float Value)
{
    if (FMath::Abs(Value) < KINDA_SMALL_NUMBER)
    {
        return;
    }

    const float NsScale = GetNorthSouthPanScale();
    if (NsScale <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
    const double NorthMeters =
        static_cast<double>(Value) * KeyboardPanSpeed * GetViewHeightMeters() * Dt * NsScale;
    PanByMeters(0.0, NorthMeters);
}

void ARGGlobePawn::MoveRight(float Value)
{
    if (FMath::Abs(Value) < KINDA_SMALL_NUMBER)
    {
        return;
    }

    const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
    const double EastMeters =
        static_cast<double>(Value) * KeyboardPanSpeed * GetViewHeightMeters() * Dt;
    PanByMeters(EastMeters, 0.0);
}

void ARGGlobePawn::FocusOnGuessResult(FRGGeoCoordinate Guess, FRGGeoCoordinate Actual, float DistanceKm)
{
    ViewLatitude = FMath::Clamp(
        (Guess.Latitude + Actual.Latitude) * 0.5,
        -static_cast<double>(MaxAbsLatitude),
        static_cast<double>(MaxAbsLatitude));

    double LonA = Guess.Longitude;
    double LonB = Actual.Longitude;
    double DLon = LonB - LonA;
    if (DLon > 180.0)
    {
        LonB -= 360.0;
    }
    else if (DLon < -180.0)
    {
        LonB += 360.0;
    }
    ViewLongitude = WrapLongitude((LonA + LonB) * 0.5);

    const float HeightKm = FMath::Clamp(FMath::Max(DistanceKm * 2.0f, 400.0f), 400.0f, 20000.0f);
    CurrentArmLength = FMath::Clamp(HeightKm * 100000.0f, MinArmLength, MaxArmLength);
}
