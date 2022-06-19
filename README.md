# DirectX12-MinimalOculusLibOVR
Minimal DirectX12 LibOVR oculus example from scratch using microsoft msvc++ compiler, just installing visual studio

Steps to compile and run:
1) Install visual studio 2019 (if different, need to change the Compile.bat script)
2) Clone repo
3) Open command prompt in repo
4) Run: `.\Compile.bat`
5) While Oculus Headset is connected, Run: `.\BasicOVR.exe`

To Debug:
1) Run: `.\Compile.bat`
2) Run: `devenv .\BasicOVRDebug.exe`
3) While Oculus Headset is connected, When Visual Studio is running, press `F11`

Controls
- `Esc` to pause/unpause
- `Alt + F4` to quit, or just close it from task manager

Improvements to make:
- All the same improvements as https://github.com/yosmo78/Win32DirectX12-FPSCamera plus things i didn't implement from in there
- Add a Retry Create Headset loop for when headset is disconnected. Also need logic to handle if headset is unplugged from one GPU but then plugged into another gpu
- Hand tracking (controllers) and better head tracking
- Projective time warping
