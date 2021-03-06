//Copyright bHaptics Inc. 2017-2019

#include "BhapticsLibrary.h"
#include "Interfaces/IPluginManager.h"

#include "Engine.h"

#if PLATFORM_ANDROID
#include "AndroidHapticLibrary.h"
#else
#include "HapticLibrary/HapticLibrary.h"
#endif // PLATFORM_ANDROID


bool BhapticsLibrary::IsInitialised = false;
bool BhapticsLibrary::IsLoaded = false;
FProcHandle BhapticsLibrary::Handle;
bool BhapticsLibrary::Success = false;

#if PLATFORM_ANDROID
#else
static bhaptics::PositionType PositionEnumToDeviceType(EPosition Position);
static bhaptics::PositionType PositionEnumToPositionType(EPosition Position);
#endif

static FString PositionEnumToString(EPosition Position);
static FString PositionEnumToDeviceString(EPosition Position);

BhapticsLibrary::BhapticsLibrary()
{

}

BhapticsLibrary::~BhapticsLibrary()
{

}

void BhapticsLibrary::SetLibraryLoaded()
{
	IsLoaded = true;
}

bool BhapticsLibrary::Initialize()
{
#if PLATFORM_ANDROID
	UAndroidHapticLibrary::AndroidThunkCpp_StartScanning();
#else
	if (!IsLoaded)
	{
		return false;
	}

	if (IsInitialised)
	{
		return Success;
	}

	bool bLaunch = true;
	if (GConfig)
	{
		GConfig->GetBool(
			TEXT("/Script/HapticsManager.HapticSettings"),
			TEXT("bShouldLaunch"),
			bLaunch,
			GGameIni
		);
	}

	IsInitialised = true;

	FString ConfigPath = *FPaths::ProjectContentDir();
	ConfigPath.Append("/ConfigFiles/HapticPlayer.txt");
	if (FPaths::FileExists(ConfigPath)) {
		FString ExeLocation;
		FFileHelper::LoadFileToString(ExeLocation, *ConfigPath);
		if (FPaths::FileExists(ExeLocation)) {
			FString ExeName = FPaths::GetBaseFilename(*ExeLocation);

			if (!FPlatformProcess::IsApplicationRunning(*ExeName) && bLaunch)
			{
				Handle = FPlatformProcess::CreateProc(*ExeLocation, nullptr, true, true, false, nullptr, 0, nullptr, nullptr);
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Could not launch: %s."), *ExeLocation);
		}
	}
	else
	{
		char Path[1000];
		int Size = 0;
		bool Result = TryGetExePath(Path, Size);
		FString ExePath(Path);
		if (Result)
		{
			if (!ExePath.IsEmpty() && FPaths::FileExists(ExePath))
			{
				FString ExeName = FPaths::GetBaseFilename(ExePath);
				UE_LOG(LogTemp, Log, TEXT("Exelocated: %s."), *ExePath);
				UE_LOG(LogTemp, Log, TEXT("Player is installed"));

				if (!FPlatformProcess::IsApplicationRunning(*ExeName) && bLaunch)
				{
					UE_LOG(LogTemp, Log, TEXT("Player is not running - launching"));

					Handle = FPlatformProcess::CreateProc(*ExePath, nullptr, true, true, false, nullptr, 0, nullptr, nullptr);

				}
				else
				{
					UE_LOG(LogTemp, Log, TEXT("Player is running"));
				}
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("Player is not Installed"));
				return false;
			}

		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Registry check failed - Initialising"));
		}

	}
	FString ProjectName;
	if (GConfig) {
		GConfig->GetString(
			TEXT("/Script/EngineSettings.GeneralProjectSettings"),
			TEXT("ProjectName"),
			ProjectName,
			GGameIni
		);
	}

	const std::string standardName(TCHAR_TO_UTF8(*ProjectName));
	Initialise(standardName.c_str(), standardName.c_str());
	Success = true;
#endif 
	return true;
}

void BhapticsLibrary::Free()
{
#if PLATFORM_ANDROID

	UAndroidHapticLibrary::AndroidThunkCpp_StopScanning();
#else
	if (!IsLoaded)
	{
		return;
	}

	Destroy();

	if (Handle.IsValid())
	{
		FPlatformProcess::TerminateProc(Handle);
		FPlatformProcess::CloseProc(Handle);
	}

#endif // PLATFORM_ANDROID
}
void BhapticsLibrary::Lib_RegisterFeedback(FString Key, FString ProjectJson)
{
	InitializeCheck();
	
#if PLATFORM_ANDROID
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ProjectJson);
	if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("Deserialisation failed."));
		return;
	}
	FRegisterRequest RegisterRequest = FRegisterRequest();
	RegisterRequest.Key = Key;
	RegisterRequest.Project = JsonObject;
	UAndroidHapticLibrary::SubmitRequestToPlayer(RegisterRequest);
#else
	if (!IsLoaded)
	{
		return;
	}
	std::string StandardKey(TCHAR_TO_UTF8(*Key));
	std::string ProjectString = (TCHAR_TO_UTF8(*ProjectJson));
	RegisterFeedback(StandardKey.c_str(), ProjectString.c_str());
#endif
}

void BhapticsLibrary::Lib_SubmitRegistered(FString Key)
{
	InitializeCheck();
	
#if PLATFORM_ANDROID
	FSubmitRequest Request = FSubmitRequest();
	Request.Key = Key;
	Request.Type = "key";
	UAndroidHapticLibrary::SubmitRequestToPlayer(Request);
#else
	if (!IsLoaded)
	{
		return;
	}
	const std::string stdKey(TCHAR_TO_UTF8(*Key));
	SubmitRegistered(stdKey.c_str());
#endif // PLATFORM_ANDROID

}

void BhapticsLibrary::Lib_SubmitRegistered(FString Key, FString AltKey, FScaleOption ScaleOpt, FRotationOption RotOption)
{

	InitializeCheck();
	
#if PLATFORM_ANDROID
	FSubmitRequest Request = FSubmitRequest();
	Request.Key = Key;
	Request.Type = "key";
	Request.Parameters.Add("rotationOption", RotOption.ToString());
	Request.Parameters.Add("scaleOption", ScaleOpt.ToString());
	Request.Parameters.Add("altKey", AltKey);
	UAndroidHapticLibrary::SubmitRequestToPlayer(Request);
#else
	if (!IsLoaded)
	{
		return;
	}
	std::string StandardKey(TCHAR_TO_UTF8(*Key));
	std::string StandardAltKey(TCHAR_TO_UTF8(*AltKey));
	bhaptics::RotationOption RotateOption;
	bhaptics::ScaleOption Option;
	RotateOption.OffsetAngleX = RotOption.OffsetAngleX;
	RotateOption.OffsetY = RotOption.OffsetY;

	Option.Intensity = ScaleOpt.Intensity;
	Option.Duration = ScaleOpt.Duration;
	SubmitRegisteredAlt(StandardKey.c_str(), StandardAltKey.c_str(), Option, RotateOption);
#endif // PLATFORM_ANDROID
}

void BhapticsLibrary::Lib_Submit(FString Key, EPosition Pos, TArray<uint8> MotorBytes, int DurationMillis)
{
	InitializeCheck();
	
#if PLATFORM_ANDROID
	TArray<FDotPoint> Points = TArray<FDotPoint>();
	for (int i = 0; i < MotorBytes.Num(); i++)
	{
		if (MotorBytes[i] > 0)
		{
			Points.Add(FDotPoint(i, MotorBytes[i]));
		}
	}
	Lib_Submit(Key, Pos, Points, DurationMillis);
#else
	if (!IsLoaded)
	{
		return;
	}

	bhaptics::PositionType HapticPosition = PositionEnumToPositionType(Pos);
	std::string StandardKey(TCHAR_TO_UTF8(*Key));

	if (MotorBytes.Num() != 20)
	{
		printf("Invalid Point Array\n");
		return;
	}

	std::vector<uint8_t> SubmittedDots(20, 0);

	for (int32 i = 0; i < MotorBytes.Num(); i++)
	{
		SubmittedDots[i] = MotorBytes[i];
	}

	Submit(StandardKey.c_str(), HapticPosition, SubmittedDots, DurationMillis);
#endif // PLATFORM_ANDROID
}

void BhapticsLibrary::Lib_Submit(FString Key, EPosition Pos, TArray<FDotPoint> Points, int DurationMillis)
{

	InitializeCheck();
	
#if PLATFORM_ANDROID
	FHapticFrame SubmissionFrame = FHapticFrame();
	SubmissionFrame.DotPoints = Points;
	SubmissionFrame.Position = PositionEnumToString(Pos);
	SubmissionFrame.PathPoints = TArray<FPathPoint>();
	SubmissionFrame.DurationMillis = DurationMillis;
	UAndroidHapticLibrary::SubmitFrame(Key, SubmissionFrame);
#else
	if (!IsLoaded)
	{
		return;
	}
	std::string StandardKey(TCHAR_TO_UTF8(*Key));
	bhaptics::PositionType HapticPosition = PositionEnumToPositionType(Pos);

	std::vector<bhaptics::DotPoint> SubmittedDots;

	for (int32 i = 0; i < Points.Num(); i++)
	{
		SubmittedDots.push_back(bhaptics::DotPoint(Points[i].Index, Points[i].Intensity));
	}

	SubmitDot(StandardKey.c_str(), HapticPosition, SubmittedDots, DurationMillis);
#endif // PLATFORM_ANDROID
}

void BhapticsLibrary::Lib_Submit(FString Key, EPosition Pos, TArray<FPathPoint> Points, int DurationMillis)
{
	InitializeCheck();
#if PLATFORM_ANDROID
	FHapticFrame SubmissionFrame = FHapticFrame();
	SubmissionFrame.DotPoints = TArray<FDotPoint>();
	SubmissionFrame.Position = PositionEnumToString(Pos);
	SubmissionFrame.PathPoints = Points;
	SubmissionFrame.DurationMillis = DurationMillis;
	UAndroidHapticLibrary::SubmitFrame(Key, SubmissionFrame);
#else
	if (!IsLoaded)
	{
		return;
	}
	std::string StandardKey(TCHAR_TO_UTF8(*Key));
	bhaptics::PositionType HapticPosition = PositionEnumToPositionType(Pos);

	std::vector<bhaptics::PathPoint> PathVector;

	for (int32 i = 0; i < Points.Num(); i++)
	{
		int XVal = Points[i].X * 1000;
		int YVal = Points[i].Y * 1000;
		bhaptics::PathPoint Point(XVal, YVal, Points[i].Intensity, Points[i].MotorCount);
		PathVector.push_back(Point);
	}

	SubmitPath(StandardKey.c_str(), HapticPosition, PathVector, DurationMillis);
#endif
}

bool BhapticsLibrary::Lib_IsFeedbackRegistered(FString Key)
{
	InitializeCheck();
	bool Value = false;
#if PLATFORM_ANDROID
	FPlayerResponse Response = UAndroidHapticLibrary::GetCurrentResponse();
	Value = Response.RegisteredKeys.Find(Key) != INDEX_NONE;
#else
	if (!IsLoaded)
	{
		return false;
	}
	std::string StandardKey(TCHAR_TO_UTF8(*Key));
	Value = IsFeedbackRegistered(StandardKey.c_str());
#endif
	return Value;
}

bool BhapticsLibrary::Lib_IsPlaying()
{
	InitializeCheck();
	
	bool Value = false;
#if PLATFORM_ANDROID
	FPlayerResponse Response = UAndroidHapticLibrary::GetCurrentResponse();
	Value = Response.ActiveKeys.Num() > 0;
#else
	if (!IsLoaded)
	{
		return false;
	}
	Value = IsPlaying();
#endif
	return Value;
}

bool BhapticsLibrary::Lib_IsPlaying(FString Key)
{
	InitializeCheck();
	
	bool Value = false;
#if PLATFORM_ANDROID
	FPlayerResponse Response = UAndroidHapticLibrary::GetCurrentResponse();
	Value = Response.ActiveKeys.Find(Key) != INDEX_NONE;
#else
	if (!IsLoaded)
	{
		return false;
	}
	std::string StandardKey(TCHAR_TO_UTF8(*Key));
	Value = IsPlayingKey(StandardKey.c_str());
#endif
	return Value;
}

void BhapticsLibrary::Lib_TurnOff()
{
	InitializeCheck();
#if PLATFORM_ANDROID
	FSubmitRequest Request = FSubmitRequest();
	Request.Key = "";
	Request.Type = "turnOffAll";
	UAndroidHapticLibrary::SubmitRequestToPlayer(Request);
#else
	if (!IsLoaded)
	{
		return;
	}
	TurnOff();
#endif // PLATFORM_ANDROID
}

void BhapticsLibrary::Lib_TurnOff(FString Key)
{
	InitializeCheck();
#if PLATFORM_ANDROID
	FSubmitRequest Request = FSubmitRequest();
	Request.Key = Key;
	Request.Type = "turnOff";
	UAndroidHapticLibrary::SubmitRequestToPlayer(Request);
#else
	if (!IsLoaded)
	{
		return;
	}
	std::string StandardKey(TCHAR_TO_UTF8(*Key));
	TurnOffKey(StandardKey.c_str());
#endif
}

void BhapticsLibrary::Lib_EnableFeedback()
{
	InitializeCheck();
#if PLATFORM_ANDROID
#else
	if (!IsLoaded)
	{
		return;
	}
	EnableFeedback();
#endif
}

void BhapticsLibrary::Lib_DisableFeedback()
{
	InitializeCheck();
#if PLATFORM_ANDROID
#else
	if (!IsLoaded)
	{
		return;
	}
	DisableFeedback();
#endif
}

void BhapticsLibrary::Lib_ToggleFeedback()
{
	InitializeCheck();
#if PLATFORM_ANDROID
#else
	if (!IsLoaded)
	{
		return;
	}
	ToggleFeedback();
#endif
}

bool BhapticsLibrary::Lib_IsDevicePlaying(EPosition Pos)
{
	InitializeCheck();
	bool Value = false;
#if PLATFORM_ANDROID
	FString DeviceString = PositionEnumToDeviceString(Pos);
	FPlayerResponse Response = UAndroidHapticLibrary::GetCurrentResponse();
	Value = Response.ConnectedPositions.Find(DeviceString) != INDEX_NONE;
#else
	if (!IsLoaded)
	{
		return false;
	}
	bhaptics::PositionType device = PositionEnumToDeviceType(Pos);
	Value = IsDevicePlaying(device);
#endif
	return Value;
}

TArray<FHapticFeedback> BhapticsLibrary::Lib_GetResponseStatus()
{
	InitializeCheck();
	TArray<FHapticFeedback> ChangedFeedback;
#if PLATFORM_ANDROID
	FPlayerResponse Response = UAndroidHapticLibrary::GetCurrentResponse();
	ChangedFeedback = Response.Status;
#else
	if (!IsLoaded)
	{
		return ChangedFeedback;
	}
	TArray<bhaptics::PositionType> Positions =
	{ bhaptics::PositionType::ForearmL,bhaptics::PositionType::ForearmR,bhaptics::PositionType::Head, bhaptics::PositionType::VestFront,bhaptics::PositionType::VestBack,
	bhaptics::PositionType::HandL, bhaptics::PositionType::HandR, bhaptics::PositionType::FootL, bhaptics::PositionType::FootR };
	TArray<EPosition> PositionEnum =
	{ EPosition::ForearmL,EPosition::ForearmR,EPosition::Head, EPosition::VestFront,EPosition::VestBack,EPosition::HandL, EPosition::HandR, EPosition::FootL, EPosition::FootR };

	for (int posIndex = 0; posIndex < Positions.Num(); posIndex++)
	{
		status stat;
		bool res = TryGetResponseForPosition(Positions[posIndex], stat);
		TArray<uint8> val;
		val.Init(0, 20);
		if (res)
		{
			for (int motorIndex = 0; motorIndex < val.Num(); motorIndex++)
			{
				val[motorIndex] = stat.values[motorIndex];
			}
		}

		FHapticFeedback Feedback = FHapticFeedback(PositionEnum[posIndex], val, EFeedbackMode::DOT_MODE);
		ChangedFeedback.Add(Feedback);
	}
#endif
	return ChangedFeedback;
}

void BhapticsLibrary::InitializeCheck()
{
	if (!IsInitialised)
	{
		Initialize();
	}
}

#if !PLATFORM_ANDROID


static bhaptics::PositionType PositionEnumToDeviceType(EPosition Position)
{
	bhaptics::PositionType Device = bhaptics::PositionType::All;

	switch (Position)
	{
	case EPosition::Left:
		Device = bhaptics::PositionType::Left;
		break;
	case EPosition::Right:
		Device = bhaptics::PositionType::Right;
		break;
	case EPosition::Head:
		Device = bhaptics::PositionType::Head;
		break;
	case EPosition::HandL:
		Device = bhaptics::PositionType::HandL;
		break;
	case EPosition::HandR:
		Device = bhaptics::PositionType::HandR;
		break;
	case EPosition::FootL:
		Device = bhaptics::PositionType::FootL;
		break;
	case EPosition::FootR:
		Device = bhaptics::PositionType::FootR;
		break;
	case EPosition::ForearmL:
		Device = bhaptics::PositionType::ForearmL;
		break;
	case EPosition::ForearmR:
		Device = bhaptics::PositionType::ForearmR;
		break;
	case EPosition::VestFront:
		Device = bhaptics::PositionType::Vest;
		break;
	case EPosition::VestBack:
		Device = bhaptics::PositionType::Vest;
		break;
	default:
		//return false;
		break;
	}
	return Device;
}

static bhaptics::PositionType PositionEnumToPositionType(EPosition Position)
{
	bhaptics::PositionType Device = bhaptics::PositionType::All;

	switch (Position)
	{
	case EPosition::Left:
		Device = bhaptics::PositionType::Left;
		break;
	case EPosition::Right:
		Device = bhaptics::PositionType::Right;
		break;
	case EPosition::Head:
		Device = bhaptics::PositionType::Head;
		break;
	case EPosition::HandL:
		Device = bhaptics::PositionType::HandL;
		break;
	case EPosition::HandR:
		Device = bhaptics::PositionType::HandR;
		break;
	case EPosition::FootL:
		Device = bhaptics::PositionType::FootL;
		break;
	case EPosition::FootR:
		Device = bhaptics::PositionType::FootR;
		break;
	case EPosition::ForearmL:
		Device = bhaptics::PositionType::ForearmL;
		break;
	case EPosition::ForearmR:
		Device = bhaptics::PositionType::ForearmR;
		break;
	case EPosition::VestFront:
		Device = bhaptics::PositionType::VestFront;
		break;
	case EPosition::VestBack:
		Device = bhaptics::PositionType::VestBack;
		break;
	default:
		//return false;
		break;
	}
	return Device;
}
#endif // !PLATFORM_ANDROID

FString PositionEnumToString(EPosition Position)
{
	FString PositionString = "All";

	switch (Position)
	{
	case EPosition::Left:
		PositionString = "Left";
		break;
	case EPosition::Right:
		PositionString = "Right";
		break;
	case EPosition::Head:
		PositionString = "Head";
		break;
	case EPosition::HandL:
		PositionString = "HandL";
		break;
	case EPosition::HandR:
		PositionString = "HandR";
		break;
	case EPosition::FootL:
		PositionString = "FootL";
		break;
	case EPosition::FootR:
		PositionString = "FootR";
		break;
	case EPosition::ForearmL:
		PositionString = "ForearmL";
		break;
	case EPosition::ForearmR:
		PositionString = "ForearmR";
		break;
	case EPosition::VestFront:
		PositionString = "VestFront";
		break;
	case EPosition::VestBack:
		PositionString = "VestBack";
		break;
	default:
		break;
	}

	return PositionString;
}

FString PositionEnumToDeviceString(EPosition Position)
{
	FString PositionString = "All";

	switch (Position)
	{
	case EPosition::Left:
		PositionString = "Left";
		break;
	case EPosition::Right:
		PositionString = "Right";
		break;
	case EPosition::Head:
		PositionString = "Head";
		break;
	case EPosition::HandL:
		PositionString = "HandL";
		break;
	case EPosition::HandR:
		PositionString = "HandR";
		break;
	case EPosition::FootL:
		PositionString = "FootL";
		break;
	case EPosition::FootR:
		PositionString = "FootR";
		break;
	case EPosition::ForearmL:
		PositionString = "ForearmL";
		break;
	case EPosition::ForearmR:
		PositionString = "ForearmR";
		break;
	case EPosition::VestFront:
		PositionString = "Vest";
		break;
	case EPosition::VestBack:
		PositionString = "Vest";
		break;
	default:
		break;
	}

	return PositionString;
}