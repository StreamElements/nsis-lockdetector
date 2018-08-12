1. Install NSIS:

	> cinst -y nsis

2. Go to NSIS Contrib folder:

	> cd /d "c:\program files (x86)\nsis\contrib"

3. Clone nsis-lockdetector repo:

	> git clone https://github.com/StreamElements/nsis-lockdetector.git

4. Open nsis-lockdetector\NSISLockDetector.vcxproj with VS2017 running in Administrator Mode

5. Build -> Batch Build -> Have the following checked:

	> Release - x86
	> 
	> Release - x64
	> 
	> Release Unicode - x86
	> 
	> Release Unicode - x64

6. The plug-in DLL will be deployed to ..\..\Plugins\[arch]\NSISLockDetector.dll

7. Usage (NSIS script):

	> SetPluginUnload alwaysoff
	> 
    > NSISLockDetector::AddWildcardPattern "$INSTDIR\*"
	> 
    > NSISLockDetector::Dialog
	> 
    > Pop $R0
	> 
    > StrCmp "$R0" "OK" programs_ok programs_error
	> 
	> 
	> 
	> programs_error:
	> 
	> 	Abort
    > 
	> 
	> 
	> programs_ok:
