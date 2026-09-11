// Copyright RadioGuesser. All Rights Reserved.

#include "Player/RGGlobePawn.h"
#include "RadioGuesser.h"
#include "Map/RGCesiumMapManager.h"
#include "Player/RGPlayerController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "EngineUtils.h"
#include "Engine/LocalPlayer.h"

ARGGlobePawn::ARGGlobePawn()
{
    PrimaryActorTick.bCanEverTick = true;

    GlobeRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GlobeRoot"));
    SetRootComponent(GlobeRoot);

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GlobeRoot);
    SpringArm->TargetArmLength  = 500000000.0f;  // ~5000 км
    SpringArm->bDoCollisionTest = false;
    SpringArm->bEnableCameraLag = false;  // отключён — lag мешает при быстром вращении
    SpringArm->bInheritPitch    = false;  // SpringArm управляет своим Pitch сам
    SpringArm->bInheritYaw      = false;  // SpringArm управляет своим Yaw сам
    SpringArm->bInheritRoll     = false;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw   = false;
    bUseControllerRotationRoll  = false;

    // Input assets created in constructor so SetupPlayerInputComponent can bind them
    MappingContext = CreateDefaultSubobject<UInputMappingContext>(TEXT("IMC_Globe"));

    IA_Drag    = CreateDefaultSubobject<UInputAction>(TEXT("IA_GlobeDrag"));
    IA_Drag->ValueType = EInputActionValueType::Boolean;

    IA_Zoom    = CreateDefaultSubobject<UInputAction>(TEXT("IA_GlobeZoom"));
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

    // Паун всегда в центре Земли — вращаем SpringArm, не перемещаем паун
    SetActorLocation(FVector::ZeroVector);

    // Начальная ориентация: смотрим на Европу немного сверху
    // Yaw=0 = смотрим с востока на запад, Pitch=-45 = 45° ниже горизонта
    CurrentYaw   = 0.0f;
    CurrentPitch = -45.0f;

    SpringArm->TargetArmLength = CurrentArmLength;
    SpringArm->SetRelativeRotation(FRotator(CurrentPitch, CurrentYaw, 0.0f));

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

    // ── Smooth zoom ────────────────────────────────────────────────────────────
    if (SpringArm)
    {
        SpringArm->TargetArmLength = FMath::FInterpTo(
            SpringArm->TargetArmLength, CurrentArmLength, DeltaTime, 8.0f);
    }

    // ── Globe rotation via drag ────────────────────────────────────────────────
    // Мышь тащит Землю: движение вправо = Yaw увеличивается (глобус поворачивается)
    // Движение вверх = смотрим более сверху (Pitch уменьшается)
    if (bIsDragging && !LastMouseDelta.IsNearlyZero())
    {
        // Скорость вращения в градусах масштабируется с высотой:
        // вблизи — медленнее (точнее), далеко — быстрее (охватываем больший угол)
        const float HeightFactor = FMath::Clamp(
            CurrentArmLength / 500000000.0f,  // нормировано к 5000 км
            0.1f, 10.0f);
        const float DragDeg = RotationSpeed * HeightFactor;

        // X мыши вправо → Yaw-- (Земля крутится влево под камерой)
        CurrentYaw   -= LastMouseDelta.X * DragDeg;
        // Y мыши вниз → Pitch++ (камера опускается к горизонту)
        CurrentPitch += LastMouseDelta.Y * DragDeg;

        // Ограничение Pitch: -85° = почти прямо вниз, -5° = почти горизонт
        CurrentPitch = FMath::Clamp(CurrentPitch, -85.0f, -5.0f);

        SpringArm->SetRelativeRotation(FRotator(CurrentPitch, CurrentYaw, 0.0f));

        LastMouseDelta = FVector2D::ZeroVector;
    }
}

// ─── Input handlers ───────────────────────────────────────────────────────────

void ARGGlobePawn::OnDragStarted(const FInputActionValue& /*Value*/)
{
    bIsDragging    = true;
    LastMouseDelta = FVector2D::ZeroVector;
}

void ARGGlobePawn::OnDragStopped(const FInputActionValue& /*Value*/)
{
    bIsDragging    = false;
    LastMouseDelta = FVector2D::ZeroVector;
}

void ARGGlobePawn::OnZoom(const FInputActionValue& Value)
{
    const float Axis = Value.Get<float>();
    // Логарифмический zoom: одно деление колёсика = 15% текущей высоты
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

// ─── WASD pan (вращение глобуса клавишами) ────────────────────────────────────

void ARGGlobePawn::MoveForward(float Value)
{
    if (FMath::Abs(Value) < KINDA_SMALL_NUMBER) return;

    // W = смотрим ниже (двигаемся к экватору/южному полюсу)
    // S = смотрим выше (двигаемся к северному полюсу)
    const float HeightFactor = FMath::Clamp(CurrentArmLength / 500000000.0f, 0.1f, 10.0f);
    CurrentPitch += Value * KeyboardPanSpeed * RotationSpeed * HeightFactor;
    CurrentPitch  = FMath::Clamp(CurrentPitch, -85.0f, -5.0f);

    SpringArm->SetRelativeRotation(FRotator(CurrentPitch, CurrentYaw, 0.0f));
}

void ARGGlobePawn::MoveRight(float Value)
{
    if (FMath::Abs(Value) < KINDA_SMALL_NUMBER) return;

    // D = поворот глобуса вправо (Yaw--)
    // A = поворот глобуса влево  (Yaw++)
    const float HeightFactor = FMath::Clamp(CurrentArmLength / 500000000.0f, 0.1f, 10.0f);
    CurrentYaw -= Value * KeyboardPanSpeed * RotationSpeed * HeightFactor;

    SpringArm->SetRelativeRotation(FRotator(CurrentPitch, CurrentYaw, 0.0f));
}
