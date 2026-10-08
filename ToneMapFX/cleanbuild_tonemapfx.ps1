cd D:\UE\UnrealEngine-5.8.0-release
Remove-Item -Recurse -Force "Engine\Plugins\Experimental\ToneMapFX\Binaries"
Remove-Item -Recurse -Force "Engine\Plugins\Experimental\ToneMapFX\Intermediate"
.\Engine\Build\BatchFiles\Build.bat UnrealEditor Win64 Development -Plugin="D:\UE\UnrealEngine-5.8.0-release\Engine\Plugins\Experimental\ToneMapFX\ToneMapFX.uplugin"